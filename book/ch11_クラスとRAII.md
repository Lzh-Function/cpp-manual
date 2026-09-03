# 第11章 クラスとRAII

Pythonのクラスは知っているはずなので、**違うところ**を中心に説明します。
特に **RAII** という考え方は、C++の最重要イディオムです。

## 11.1 struct と class

```cpp
struct Atom {          // デフォルトで public
    int  atomic_num;
    bool aromatic;
};

class Molecule {       // デフォルトで private
    std::vector<Atom> atoms_;
public:
    void add_atom(Atom a) { atoms_.push_back(a); }
};
```

**`struct` と `class` の唯一の違いは、デフォルトのアクセス指定子だけ**です。
それ以外は完全に同じ（`struct` にもメソッドや継承が書けます）。

慣習:
- **`struct`**: 単なるデータの集まり。全部public。不変条件がない
- **`class`**: 内部状態を隠蔽し、メソッド経由で操作させる

```cpp
struct Point3D { double x, y, z; };          // データだけ → struct

class Molecule {                              // 不変条件を守る → class
    std::vector<Atom> atoms_;
    std::vector<Bond> bonds_;
    // 不変条件: bonds_ の中の原子インデックスは必ず atoms_ の範囲内
public:
    void add_bond(int i, int j);   // ここでチェックする
};
```

---

## 11.2 アクセス指定子

```cpp
class Molecule {
public:
    // どこからでもアクセス可能。クラスの「公開API」
    std::size_t num_atoms() const;

protected:
    // このクラスと派生クラスからアクセス可能
    void invalidate_cache();

private:
    // このクラスの中からのみアクセス可能
    std::vector<Atom> atoms_;
    mutable double    cached_mw_ = -1.0;
};
```

> 🐍 **Pythonとの対比**
>
> Pythonの `_private` や `__mangled` は**慣習と名前修飾**でしかなく、
> 頑張ればアクセスできます。
> C++の `private` は**コンパイラが強制**します。
>
> ```python
> class Mol:
>     def __init__(self):
>         self._atoms = []      # 「触らないでね」という約束
> obj._atoms.append(x)          # ★ 実際には触れてしまう
> ```
>
> ```cpp
> class Mol { std::vector<Atom> atoms_; };
> obj.atoms_.push_back(x);      // ★ コンパイルエラー
> ```

> 💡 **メンバ変数の命名規則**
> `atoms_`（末尾アンダースコア）または `m_atoms`（先頭 m_）が一般的です。
> ローカル変数と区別でき、コンストラクタの引数と名前が衝突しません。
> 本書では末尾アンダースコアを使います。

---

## 11.3 コンストラクタ

```cpp
class Molecule {
    std::string smiles_;
    std::vector<Atom> atoms_;
public:
    // デフォルトコンストラクタ
    Molecule() = default;

    // 引数付きコンストラクタ
    explicit Molecule(std::string smiles)
        : smiles_(std::move(smiles))      // ★ 初期化リスト
    {
        parse();
    }
};
```

### ★ 初期化リスト（メンバ初期化子リスト）

```cpp
// ✓ 良い: 初期化リストで初期化
Molecule(std::string s) : smiles_(std::move(s)) { }

// ✗ 悪い: コンストラクタ本体で代入
Molecule(std::string s) { smiles_ = s; }
```

なぜ違うのか:

```
初期化リスト:   smiles_ を s から直接構築（1回の操作）
本体で代入:     smiles_ をデフォルト構築 → その後 s を代入（2回の操作）
```

`std::string` や `std::vector` のような重い型では、明確な差になります。

さらに、**初期化リストでないと不可能なケース**があります:

```cpp
class Molecule {
    const int max_atoms_;      // const メンバ
    std::vector<Atom>& ref_;   // 参照メンバ
public:
    // ✓ 初期化リストでしか初期化できない
    Molecule(int n, std::vector<Atom>& r) : max_atoms_(n), ref_(r) {}
};
```

> ⚠️ **初期化リストの順序は、宣言順に従います。**
> ```cpp
> class Bad {
>     int b_;
>     int a_;
> public:
>     Bad() : a_(1), b_(a_ + 1) {}   // ★ b_ が先に初期化される。a_ はまだゴミ！
> };
> ```
> 書いた順ではなく、**メンバの宣言順**に初期化されます。
> `-Wall` で `-Wreorder` 警告が出ます。宣言順に書く習慣をつけてください。

### デフォルトメンバ初期化子（C++11以降、おすすめ）

```cpp
class Molecule {
    std::string smiles_;
    double      mw_        = 0.0;      // ★ ここで初期値を書ける
    int         n_rings_   = 0;
    bool        is_parsed_ = false;
public:
    Molecule() = default;              // 上の初期値が使われる
    explicit Molecule(std::string s) : smiles_(std::move(s)) {}
};
```

**初期化忘れを防げる**ので、積極的に使ってください。

### explicit — 暗黙の変換を防ぐ

```cpp
class Molecule {
public:
    Molecule(std::string smiles);      // explicit なし
};

void process(const Molecule& m);
process("CCO");        // ★ 文字列が勝手に Molecule に変換される！
```

意図しない変換は混乱とバグの元です。

```cpp
explicit Molecule(std::string smiles);
process("CCO");                  // ✗ コンパイルエラー
process(Molecule("CCO"));        // ✓ 明示的に書く
```

> 💡 **引数1つのコンストラクタには、原則 `explicit` を付けてください。**
> 例外は、意図的に暗黙変換させたい場合（`std::string` から `std::string_view` など）。

### 委譲コンストラクタ

```cpp
class Molecule {
    std::string smiles_;
    int         charge_;
public:
    Molecule(std::string s, int c) : smiles_(std::move(s)), charge_(c) {}
    explicit Molecule(std::string s) : Molecule(std::move(s), 0) {}  // 委譲
};
```

Pythonでいう `def __init__(self, s, c=0)` のデフォルト引数に近い書き方です。

---

## 11.4 デストラクタと ★RAII★

### デストラクタ

```cpp
class FileReader {
    std::FILE* fp_;
public:
    explicit FileReader(const char* path) : fp_(std::fopen(path, "r")) {}

    ~FileReader() {                 // ★ デストラクタ
        if (fp_) std::fclose(fp_);
    }
};
```

**デストラクタは、オブジェクトが破棄されるときに自動的に呼ばれます。**

```cpp
void f() {
    FileReader r("data.sdf");
    // ... 何か処理 ...
}   // ★ ここで自動的に ~FileReader() が呼ばれ、ファイルが閉じられる
```

### ★★★ RAII — Resource Acquisition Is Initialization ★★★

これがC++で最も重要な設計イディオムです。

> **リソースの取得をコンストラクタで行い、解放をデストラクタで行う。**
> そうすれば、スコープを抜けたときに**必ず**解放される。

「必ず」というのが重要です。

```cpp
void f() {
    FileReader r("data.sdf");

    if (something) return;          // ★ ここで return しても閉じられる
    if (other) throw std::runtime_error("oops");   // ★ 例外が飛んでも閉じられる

    process(r);
}   // ★ 正常終了でも閉じられる
```

**どんな経路で抜けても、デストラクタは必ず呼ばれます。**
これがC++の安全性の根幹です。

### 🐍 Pythonの `with` 文と同じもの

```python
with open("data.sdf") as f:      # __enter__ でリソース取得
    process(f)
# __exit__ で自動的に閉じられる
```

```cpp
{
    FileReader r("data.sdf");    // コンストラクタでリソース取得
    process(r);
}   // デストラクタで自動的に閉じられる
```

**違いは、C++では `with` と書く必要がないこと**です。
すべてのオブジェクトが自動的に `with` の対象になります。

| | Python | C++ |
|---|---|---|
| リソース解放 | `with` を書く必要がある | **自動** |
| 書き忘れ | `with` を忘れると漏れる | **忘れようがない** |
| 対象 | `__enter__`/`__exit__` を持つ型 | **すべての型** |

> 💡 **これは本当にすごい仕組みです。**
> C++には Python の `finally` に相当する構文がありませんが、
> **必要ないから**です。RAII がその役割を果たします。

### RAII が管理するもの

| リソース | RAII クラス |
|---|---|
| メモリ | `std::vector`, `std::string`, `std::unique_ptr` |
| ファイル | `std::fstream` |
| ミューテックス | `std::lock_guard`, `std::unique_lock` |
| スレッド | `std::jthread`（C++20） |
| ソケット、DBハンドル、GPUメモリ | 自作 |

```cpp
// ミューテックスの例（第25章）
void update(std::mutex& m, Data& d) {
    std::lock_guard<std::mutex> lock(m);    // ★ ここでロック
    d.modify();
    if (error) return;                       // ★ 自動でアンロック
}                                            // ★ 自動でアンロック
```

手動で `unlock()` を書くと、`return` や例外で漏れます。RAII なら漏れません。

### 自作 RAII クラス: スコープタイマー

```cpp
// code/ch11/timer.hpp
#pragma once
#include <chrono>
#include <iostream>
#include <string>

class ScopedTimer {
    std::string label_;
    std::chrono::steady_clock::time_point start_;
public:
    explicit ScopedTimer(std::string label)
        : label_(std::move(label)), start_(std::chrono::steady_clock::now()) {}

    ~ScopedTimer() {
        auto ms = std::chrono::duration<double, std::milli>(
                      std::chrono::steady_clock::now() - start_).count();
        std::cerr << "[" << label_ << "] " << ms << " ms\n";
    }

    // コピーを禁止（タイマーが2回発火してしまうため）
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
};
```

使い方:

```cpp
void screen_library() {
    ScopedTimer t("screening");
    // ... 重い処理 ...
}   // ここで自動的に時間が出力される
```

```
[screening] 1234.5 ms
```

Pythonのコンテキストマネージャや `@timeit` デコレータと同じ発想です。
第28章のプロファイリングで使います。

---

## 11.5 メンバ関数と const

```cpp
class Molecule {
    std::vector<Atom> atoms_;
public:
    // ★ const メンバ関数: オブジェクトを変更しない
    std::size_t num_atoms() const { return atoms_.size(); }

    // 非 const: オブジェクトを変更する
    void add_atom(Atom a) { atoms_.push_back(a); }
};
```

```cpp
const Molecule m = load();
m.num_atoms();    // ✓ OK
m.add_atom(a);    // ✗ コンパイルエラー（const オブジェクトに非constメソッド）
```

> 💡 **変更しないメンバ関数には必ず `const` を付けてください。**
> 付け忘れると、`const Molecule&` で受け取った引数に対して呼べなくなり、
> 呼び出し側が困ります（そして const を外す羽目になり、悪循環が始まります）。
>
> これを「**const 汚染**」と呼びます。最初から正しく付けるのが吉です。

### const オーバーロード

```cpp
class Molecule {
    std::vector<Atom> atoms_;
public:
    Atom&       atom(std::size_t i)       { return atoms_[i]; }  // 書き換え可
    const Atom& atom(std::size_t i) const { return atoms_[i]; }  // 読み取り専用
};

Molecule m;
m.atom(0).aromatic = true;         // 非const版が呼ばれる

const Molecule& cm = m;
bool a = cm.atom(0).aromatic;      // const版が呼ばれる
// cm.atom(0).aromatic = true;     // ✗ const Atom& なので変更不可
```

### mutable — const の中で変更したいメンバ

```cpp
class Molecule {
    std::vector<Atom>      atoms_;
    mutable double         cached_mw_ = -1.0;   // ★ mutable
public:
    double molecular_weight() const {           // const だが cached_mw_ は変更できる
        if (cached_mw_ < 0) {
            cached_mw_ = compute_mw();          // ✓ mutable なので OK
        }
        return cached_mw_;
    }
};
```

**キャッシュやミューテックス**にだけ使ってください。
「論理的には変更していない（外から見た状態は同じ）」ものに限ります。

---

## 11.6 static メンバ

```cpp
class Element {
public:
    static constexpr int MAX_ATOMIC_NUM = 118;    // クラス定数
    static int instance_count;                     // クラス変数

    static double atomic_weight(int z);            // クラスメソッド（this なし）
};

// 静的メンバ変数は .cpp で定義が必要（C++17 なら inline で回避可）
int Element::instance_count = 0;
```

```cpp
std::cout << Element::MAX_ATOMIC_NUM << "\n";
double w = Element::atomic_weight(6);
```

```python
class Element:
    MAX_ATOMIC_NUM = 118          # クラス変数

    @staticmethod
    def atomic_weight(z): ...
```

C++17 なら `inline` でヘッダに書けます:

```cpp
class Element {
public:
    inline static int instance_count = 0;    // .cpp での定義が不要
};
```

🧪 **元素データテーブルの実装例:**

```cpp
// code/ch11/element.hpp
#pragma once
#include <array>
#include <string_view>

class Element {
public:
    static constexpr int MAX_Z = 118;

    static double weight(int z) {
        return (z >= 1 && z <= MAX_Z) ? WEIGHTS[z] : 0.0;
    }
    static std::string_view symbol(int z) {
        return (z >= 1 && z <= MAX_Z) ? SYMBOLS[z] : "??";
    }
    static int atomic_number(std::string_view sym) {
        for (int z = 1; z <= MAX_Z; ++z) {
            if (SYMBOLS[z] == sym) return z;
        }
        return 0;
    }

private:
    // constexpr な静的テーブル（コンパイル時に構築、実行時コストゼロ）
    static constexpr std::array<double, MAX_Z + 1> WEIGHTS = {
        0.0,      1.008,   4.0026,  6.94,    9.0122,  10.81,   12.011,
        14.007,   15.999,  18.998,  20.180,  22.990,  24.305,  26.982,
        28.085,   30.974,  32.06,   35.45,   39.948,  39.098,  40.078,
        // ... 以下省略（実際は118個）
    };
    static constexpr std::array<std::string_view, MAX_Z + 1> SYMBOLS = {
        "",  "H",  "He", "Li", "Be", "B",  "C",  "N",  "O",  "F",  "Ne",
        "Na","Mg", "Al", "Si", "P",  "S",  "Cl", "Ar", "K",  "Ca",
        // ...
    };
};
```

`constexpr` なのでテーブルは**コンパイル時に構築され、
実行ファイルの読み取り専用領域に置かれます**。
実行時の初期化コストがゼロです。

---

## 11.7 演算子オーバーロード

Pythonの `__add__`, `__eq__`, `__lt__` に相当します。

```cpp
struct Vec3 {
    double x = 0, y = 0, z = 0;

    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(double s)      const { return {x*s, y*s, z*s}; }

    Vec3& operator+=(const Vec3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }

    double dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
    double norm() const { return std::sqrt(dot(*this)); }

    bool operator==(const Vec3& o) const = default;   // C++20: 自動生成
};

// スカラー * ベクトル（メンバにできないので自由関数）
inline Vec3 operator*(double s, const Vec3& v) { return v * s; }

// 出力
inline std::ostream& operator<<(std::ostream& os, const Vec3& v) {
    return os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
}
```

使い方:

```cpp
Vec3 a{1, 0, 0}, b{0, 1, 0};
Vec3 c = a + b;
double d = (a - b).norm();
std::cout << c << "\n";        // (1, 1, 0)
```

🧪 3D座標の計算（結合長、結合角、二面角）で大活躍します。

### 宇宙船演算子 `<=>`（C++20）

```cpp
struct Version {
    int major, minor, patch;
    auto operator<=>(const Version&) const = default;   // ★ 6つの比較演算子が全部生える
};

Version a{1,2,3}, b{1,3,0};
bool x = a < b;    // ✓ 使える
bool y = a >= b;   // ✓ 使える
```

これ1行で `<`, `<=`, `>`, `>=`, `==`, `!=` が全部使えるようになります。

> 💡 **演算子オーバーロードの原則: 直感に反する定義をしない。**
> `operator+` で減算したり、`operator<<` でファイル削除したりしないこと。
> 数学的な型（ベクトル、行列、複素数）と、
> ストリーム出力 (`<<`) 以外では控えめに使うのが良い作法です。

---

## 11.8 friend

```cpp
class Molecule {
    std::vector<Atom> atoms_;
    friend std::ostream& operator<<(std::ostream&, const Molecule&);
    friend double tanimoto(const Molecule&, const Molecule&);
};

std::ostream& operator<<(std::ostream& os, const Molecule& m) {
    return os << m.atoms_.size() << " atoms";   // private にアクセスできる
}
```

`friend` は「この関数/クラスには private を見せる」という宣言です。

> 💡 **多用しないでください。** カプセル化を壊します。
> `operator<<` のように「メンバにできないが内部を見たい」場合の
> 例外的な手段と考えてください。
> 多くの場合、public な getter を用意する方が良い設計です。

---

## 11.9 🧪 実践: 分子クラスの設計

```cpp
// code/ch11/molecule.hpp
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct Atom {
    std::uint8_t atomic_num  = 0;
    std::int8_t  charge      = 0;
    std::uint8_t num_h       = 0;      // 暗黙の水素数
    bool         aromatic    = false;
    bool         in_ring     = false;
};

struct Bond {
    std::uint32_t a          = 0;      // 原子インデックス
    std::uint32_t b          = 0;
    std::uint8_t  order      = 1;      // 1=単, 2=二重, 3=三重, 4=芳香
    bool          in_ring    = false;
};

class Molecule {
public:
    // ---- 構築 ----
    Molecule() = default;
    explicit Molecule(std::string name) : name_(std::move(name)) {}

    // ---- 変更 ----
    std::uint32_t add_atom(Atom a) {
        atoms_.push_back(a);
        adjacency_.emplace_back();          // 隣接リストも増やす
        invalidate_cache();
        return static_cast<std::uint32_t>(atoms_.size() - 1);
    }

    void add_bond(std::uint32_t i, std::uint32_t j, std::uint8_t order = 1) {
        if (i >= atoms_.size() || j >= atoms_.size())
            throw std::out_of_range("add_bond: atom index out of range");
        if (i == j)
            throw std::invalid_argument("add_bond: self-bond not allowed");

        bonds_.push_back({i, j, order, false});
        adjacency_[i].push_back(j);
        adjacency_[j].push_back(i);
        invalidate_cache();
    }

    // ---- 参照（const オーバーロード）----
    const Atom& atom(std::size_t i) const { return atoms_.at(i); }
    Atom&       atom(std::size_t i)       { invalidate_cache(); return atoms_.at(i); }

    const std::vector<Atom>& atoms() const noexcept { return atoms_; }
    const std::vector<Bond>& bonds() const noexcept { return bonds_; }

    const std::vector<std::uint32_t>& neighbors(std::size_t i) const {
        return adjacency_.at(i);
    }

    // ---- 問い合わせ（const）----
    std::size_t num_atoms() const noexcept { return atoms_.size(); }
    std::size_t num_bonds() const noexcept { return bonds_.size(); }
    const std::string& name() const noexcept { return name_; }

    std::size_t degree(std::size_t i) const { return adjacency_.at(i).size(); }

    // 遅延計算 + キャッシュ（mutable の実用例）
    double molecular_weight() const {
        if (cached_mw_ < 0.0) cached_mw_ = compute_mw();
        return cached_mw_;
    }

    int num_heavy_atoms() const {
        int n = 0;
        for (const auto& a : atoms_) if (a.atomic_num > 1) ++n;
        return n;
    }

    // 環の数（Frèrejacque 式: 結合数 - 原子数 + 連結成分数）
    int ring_count() const {
        return static_cast<int>(bonds_.size()) -
               static_cast<int>(atoms_.size()) + num_components();
    }

    int num_components() const;   // 実装は .cpp（連結成分数を数える）

private:
    void   invalidate_cache() const noexcept { cached_mw_ = -1.0; }
    double compute_mw() const;

    std::string                              name_;
    std::vector<Atom>                        atoms_;
    std::vector<Bond>                        bonds_;
    std::vector<std::vector<std::uint32_t>>  adjacency_;   // 隣接リスト
    mutable double                           cached_mw_ = -1.0;
};
```

```cpp
// code/ch11/molecule.cpp
#include "molecule.hpp"
#include "element.hpp"
#include <vector>

double Molecule::compute_mw() const {
    double sum = 0.0;
    for (const auto& a : atoms_) {
        sum += Element::weight(a.atomic_num);
        sum += a.num_h * Element::weight(1);      // 暗黙の水素
    }
    return sum;
}

int Molecule::num_components() const {
    if (atoms_.empty()) return 0;
    std::vector<char> visited(atoms_.size(), 0);
    int components = 0;

    std::vector<std::uint32_t> stack;
    for (std::size_t start = 0; start < atoms_.size(); ++start) {
        if (visited[start]) continue;
        ++components;
        stack.clear();
        stack.push_back(static_cast<std::uint32_t>(start));
        visited[start] = 1;
        while (!stack.empty()) {
            std::uint32_t cur = stack.back();
            stack.pop_back();
            for (std::uint32_t nb : adjacency_[cur]) {
                if (!visited[nb]) { visited[nb] = 1; stack.push_back(nb); }
            }
        }
    }
    return components;
}
```

**設計のポイント:**

1. **不変条件を守る**: `add_bond` でインデックスをチェックし、
   `atoms_` / `bonds_` / `adjacency_` の整合性を保証する
2. **`const` の徹底**: 読むだけのメソッドは全部 `const`
3. **`noexcept`**: 例外を投げないメソッドに付けると最適化が効く（第18章）
4. **キャッシュは `mutable`**: 論理的に不変なので `const` メソッドから更新できる
5. **隣接リストを保持**: グラフアルゴリズムを O(1) で近傍取得できるようにする
6. **再帰ではなく明示スタック**: 大きな分子でもスタックオーバーフローしない

---

## 11.10 Pythonクラスとの対応表

| Python | C++ |
|---|---|
| `class Foo:` | `class Foo { };`（末尾の `;` 必須） |
| `def __init__(self, x)` | `Foo(int x)` |
| `def __del__(self)` | `~Foo()` |
| `def method(self)` | `void method()` |
| `def method(self)` （読むだけ） | `void method() const` |
| `@staticmethod` | `static void method()` |
| `@classmethod` | （直接の対応なし。static + 明示的な型） |
| `@property` | getter メソッド `x() const` |
| `self` | `this`（ポインタ。`this->x` または単に `x`） |
| `__str__` | `operator<<` |
| `__eq__` | `operator==` |
| `__lt__` | `operator<` / `operator<=>` |
| `__add__` | `operator+` |
| `__len__` | `size()` |
| `__getitem__` | `operator[]` |
| `__call__` | `operator()` |
| `__enter__` / `__exit__` | コンストラクタ / デストラクタ（RAII） |
| `_private` | `private:` |
| クラス変数 | `static` メンバ |
| `@dataclass` | `struct` + デフォルトメンバ初期化子 |

> ⚠️ **`this` はポインタです。** Pythonの `self` と違い、
> メンバ関数の中では省略できます。
> ```cpp
> void Molecule::f() {
>     atoms_.size();          // ✓ 普通はこう書く
>     this->atoms_.size();    // 同じ意味。曖昧さを避けたいときだけ
> }
> ```

---

## 11.11 この章のまとめ

- `struct` と `class` の違いはデフォルトのアクセス指定子だけ
- **初期化はコンストラクタ本体ではなく初期化リストで**
- 初期化順は**宣言順**。書いた順ではない
- 引数1つのコンストラクタには `explicit`
- **RAII = コンストラクタで取得、デストラクタで解放**
  → どんな経路で抜けても必ず解放される。**Pythonの `with` が言語に組み込まれたもの**
- 変更しないメソッドには必ず `const`
- キャッシュには `mutable`
- 演算子オーバーロードは、数学型と `operator<<` 中心に控えめに
- C++20 の `= default` で `==` や `<=>` を自動生成できる

> 📝 **練習問題 11-1**
>
> `ScopedTimer` を改造して、複数回計測した結果を静的メンバに蓄積し、
> プログラム終了時に集計を表示するようにしてください。
> （ヒント: `static std::unordered_map<std::string, double>` と、
> 終了時に集計を出すもう1つのRAIIクラス）

> 📝 **練習問題 11-2**
>
> `Molecule` クラスに、以下のメソッドを追加してください。
> - `bool has_atom(std::uint8_t atomic_num) const`
> - `std::vector<std::uint32_t> atoms_with_element(std::uint8_t z) const`
> - `double average_degree() const`

> 📝 **練習問題 11-3**
>
> 次のクラスの問題点を3つ挙げて修正してください。
> ```cpp
> class Descriptor {
>     std::string name;
>     double value;
> public:
>     Descriptor(std::string n, double v) { name = n; value = v; }
>     std::string get_name() { return name; }
>     double get_value() { return value; }
> };
> ```
> （ヒント: 初期化リスト、const、戻り値の型）

---

→ [第12章 コピーとムーブ](ch12_コピーとムーブ.md)
