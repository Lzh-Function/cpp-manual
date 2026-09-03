# 第15章 テンプレート入門

「テンプレートは難しい」と言われますが、**使う分には簡単**です。
実際、あなたは既にたくさん使っています（`std::vector<int>` がそれです）。

難しいのは「テンプレートメタプログラミング」であって、
**普通のジェネリックプログラミングは、Pythonの関数を書くのとほぼ同じ**です。

## 15.1 Pythonの関数は最初からテンプレート

```python
def maximum(a, b):
    return a if a > b else b

maximum(3, 5)          # int で動く
maximum(3.1, 5.2)      # float で動く
maximum("abc", "abd")  # str で動く
```

Pythonは動的型付けなので、**型を書かなければ何でも受け取れます**。
C++で同じことをしたいときに使うのがテンプレートです。

```cpp
template <typename T>
T maximum(T a, T b) {
    return (a > b) ? a : b;
}

maximum(3, 5);            // T = int
maximum(3.1, 5.2);        // T = double
maximum(std::string("abc"), std::string("abd"));   // T = std::string
```

### 決定的な違い: いつ型が決まるか

| | Python | C++ |
|---|---|---|
| 型チェック | **実行時** | **コンパイル時** |
| `a > b` ができない型を渡したら | 実行時に `TypeError` | **コンパイルエラー** |
| 生成されるコード | 1つ（汎用） | **型ごとに専用のコードが生成される** |
| 速度 | 型判定のオーバーヘッドあり | **専用コードなので最速** |

```cpp
maximum(3, 5);        // int 専用の maximum が生成される
maximum(3.1, 5.2);    // double 専用の maximum が生成される
```

これを **インスタンス化 (instantiation)** と呼びます。
コンパイラが型ごとに関数を「コピペして型を埋めた」ものを作ります。

> 💡 **だからテンプレートは速い。**
> `int` 用の関数の中には `int` の処理しかなく、
> 分岐も型チェックもありません。完全にインライン展開されます。
>
> **だからコンパイルが遅い。**
> 使った型の数だけコードが生成されるからです。

---

## 15.2 関数テンプレート

```cpp
template <typename T>
double mean(const std::vector<T>& v) {
    if (v.empty()) return 0.0;
    double sum = 0.0;
    for (const T& x : v) sum += x;
    return sum / v.size();
}

std::vector<int>    a = {1, 2, 3};
std::vector<double> b = {1.5, 2.5};
mean(a);    // T = int と推論される
mean(b);    // T = double
```

型は**引数から自動推論**されます。明示することもできます:

```cpp
mean<double>(a);      // 明示的に指定
```

### 複数の型パラメータ

```cpp
template <typename T, typename U>
auto add(T a, U b) {
    return a + b;      // 戻り値の型は auto で推論
}

add(1, 2.5);           // T=int, U=double → double が返る
```

### 非型テンプレートパラメータ

型だけでなく、**値**もテンプレート引数にできます。

```cpp
template <std::size_t N>
class Fingerprint {
    std::array<std::uint64_t, N> bits_{};
public:
    static constexpr std::size_t num_bits() { return N * 64; }

    void set(std::size_t i) { bits_[i / 64] |= (1ULL << (i % 64)); }
    bool get(std::size_t i) const { return bits_[i / 64] & (1ULL << (i % 64)); }

    int popcount() const {
        int n = 0;
        for (auto w : bits_) n += std::popcount(w);
        return n;
    }
};

Fingerprint<32>  fp2048;   // 2048 bit
Fingerprint<16>  fp1024;   // 1024 bit
```

**サイズがコンパイル時に決まるので、ループが完全展開され、
ヒープ確保も起きません。** 最速のフィンガープリント実装です（第22章）。

> ⚠️ `Fingerprint<32>` と `Fingerprint<16>` は**完全に別の型**です。
> 代入も比較もできません。それが意図通りなら良い設計です。

---

## 15.3 クラステンプレート

```cpp
template <typename T>
class Matrix {
    std::size_t rows_, cols_;
    std::vector<T> data_;
public:
    Matrix(std::size_t r, std::size_t c) : rows_(r), cols_(c), data_(r * c) {}

    T&       operator()(std::size_t i, std::size_t j)       { return data_[i * cols_ + j]; }
    const T& operator()(std::size_t i, std::size_t j) const { return data_[i * cols_ + j]; }

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    T*          data()       { return data_.data(); }
    const T*    data() const { return data_.data(); }
};

Matrix<double> distances(100, 100);
Matrix<float>  similarities(1000, 1000);   // メモリ半分
Matrix<std::uint8_t> adjacency(50, 50);    // 隣接行列
```

### CTAD（クラステンプレート引数推論、C++17）

```cpp
std::vector v = {1, 2, 3};        // std::vector<int> と推論される（C++17）
std::pair p{1, 2.5};              // std::pair<int, double>
```

C++17以降、多くの場合 `<T>` を省略できます。

---

## 15.4 ★重要★ テンプレートはヘッダに書く

```cpp
// ✗ これは動かない
// matrix.hpp
template <typename T>
class Matrix {
    T& at(std::size_t i, std::size_t j);   // 宣言だけ
};

// matrix.cpp
template <typename T>
T& Matrix<T>::at(std::size_t i, std::size_t j) { ... }   // 定義

// main.cpp
Matrix<double> m;
m.at(0, 0);      // ★ リンクエラー: undefined reference
```

**理由:** `main.cpp` をコンパイルするとき、コンパイラは
`Matrix<double>::at` の**コードを生成する必要**があります。
そのためには**実装が見えていなければなりません**。
`matrix.cpp` の中身は見えないので、生成できないのです。

```cpp
// ✓ 正しい: 実装もヘッダに書く
// matrix.hpp
template <typename T>
class Matrix {
public:
    T& at(std::size_t i, std::size_t j) { return data_[i * cols_ + j]; }   // 実装も書く
};
```

> 💡 **「テンプレートは全部ヘッダに書く」**と覚えてください。
> これがC++のコンパイルが遅い大きな理由のひとつです。
>
> 例外: 使う型が限られているなら、明示的インスタンス化ができます。
> ```cpp
> // matrix.cpp の末尾
> template class Matrix<double>;
> template class Matrix<float>;
> ```
> これで `double` と `float` 版だけがコンパイルされ、他の .cpp からリンクできます。

---

## 15.5 テンプレートのエラーメッセージ

正直に言います。**テンプレートのエラーメッセージは地獄です。**

```cpp
std::vector<Molecule> mols;
std::sort(mols.begin(), mols.end());   // Molecule に operator< がない
```

これで**400行のエラー**が出ます。

**読み方のコツ:**

1. **一番上のエラーだけ読む**（下は連鎖した派生エラー）
2. `required from here` を探す。**そこがあなたのコードの行**
3. `error:` の行を探す。他は `note:`（補足情報）
4. `no match for 'operator<'` のような、**核心の一文**を探す

```
error: no match for 'operator<' (operand types are 'Molecule' and 'Molecule')
   ...
note: in instantiation of ... required from here
main.cpp:42:5:   required from here      ← ★ ここがあなたのコード
```

> 💡 **Clang のエラーメッセージは GCC より格段に読みやすい**です。
> GCC でエラーが読めないとき、Clang でコンパイルしてみると
> 一発で分かることがよくあります。
>
> ```bash
> clang++ -std=c++20 -fsyntax-only prog.cpp
> ```
>
> また、`-fmax-errors=1`（GCC）や `-ferror-limit=1`（Clang）で
> 最初のエラーだけ出させると読みやすくなります。

---

## 15.6 ★concept★ (C++20) — 制約でエラーを人間語にする

concept は、**テンプレート引数に条件を付ける**機能です。
これでエラーメッセージが劇的に読みやすくなります。

```cpp
#include <concepts>

template <std::floating_point T>          // ★ T は浮動小数点型に限る
T normalize(T x, T mean, T sd) {
    return (x - mean) / sd;
}

normalize(1.0, 0.0, 1.0);    // ✓ OK
normalize(1, 0, 1);          // ✗ エラー: "constraint not satisfied" と明示される
```

エラーメッセージ:

```
error: no matching function for call to 'normalize(int, int, int)'
note: constraints not satisfied
note: the expression 'is_floating_point_v<T>' evaluated to 'false'
```

**400行が3行になりました。**

### 標準の concept

```cpp
#include <concepts>

std::integral<T>          // 整数型
std::floating_point<T>    // 浮動小数点型
std::same_as<T, U>        // 同じ型
std::convertible_to<T, U> // 変換可能
std::equality_comparable<T>  // == で比較可能
std::totally_ordered<T>   // < で全順序
std::invocable<F, Args...>   // 呼び出し可能
std::copyable<T>          // コピー可能
std::movable<T>           // ムーブ可能
```

範囲用:

```cpp
#include <ranges>
std::ranges::range<R>              // range-based for が使える
std::ranges::random_access_range<R>
std::ranges::sized_range<R>
```

### 自作の concept

```cpp
template <typename T>
concept Descriptor = requires(const T& d, const Molecule& m) {
    { d.compute(m) } -> std::convertible_to<double>;
    { d.name() }     -> std::convertible_to<std::string>;
};

template <Descriptor D>
void print_descriptor(const D& d, const Molecule& m) {
    std::cout << d.name() << " = " << d.compute(m) << "\n";
}
```

**これはインタフェースを「継承なしで」表現しています。**
仮想関数のコストなしに、要件をコンパイラに伝えられます。

🧪 フィンガープリント用の concept:

```cpp
template <typename T>
concept BitVector = requires(const T& fp, std::size_t i) {
    { fp.get(i) }      -> std::same_as<bool>;
    { fp.popcount() }  -> std::convertible_to<int>;
    { fp.num_bits() }  -> std::convertible_to<std::size_t>;
};

template <BitVector FP>
double tanimoto(const FP& a, const FP& b) {
    // ...
}
```

### 書き方のバリエーション

```cpp
// ① テンプレートパラメータの位置
template <std::integral T>
void f(T x);

// ② requires 節
template <typename T>
requires std::integral<T>
void f(T x);

// ③ 短縮形（auto に concept を付ける）
void f(std::integral auto x);

// ④ 後置 requires
template <typename T>
void f(T x) requires std::integral<T>;
```

**③が最も簡潔**です。C++20 の関数テンプレートは `auto` 引数でも書けます:

```cpp
double mean(const std::ranges::range auto& v);
```

---

## 15.7 if constexpr — コンパイル時の分岐

```cpp
template <typename T>
void print(const T& x) {
    if constexpr (std::is_floating_point_v<T>) {
        std::cout << std::format("{:.4f}", x);
    } else if constexpr (std::is_integral_v<T>) {
        std::cout << x;
    } else {
        std::cout << "<object>";
    }
}
```

`if constexpr` は**コンパイル時に分岐が解決され、
選ばれなかった枝はコードが生成されません**。

普通の `if` だと:

```cpp
if (std::is_floating_point_v<T>) {
    std::cout << std::format("{:.4f}", x);   // ★ T=std::string でもコンパイルされる → エラー
}
```

`if constexpr` なら、`T = std::string` のとき `format` の枝は
**存在しないことになる**のでエラーになりません。

> 💡 C++17 以前は SFINAE という難解な技法が必要でしたが、
> `if constexpr` と concept でほぼ不要になりました。
> 古いコードで `std::enable_if_t<...>` を見たら、
> 「これは今なら concept で書ける」と読み替えてください。

---

## 15.8 型特性 (type traits)

```cpp
#include <type_traits>

std::is_integral_v<T>         // 整数型か
std::is_floating_point_v<T>   // 浮動小数点型か
std::is_same_v<T, U>          // 同じ型か
std::is_pointer_v<T>
std::is_const_v<T>
std::is_trivially_copyable_v<T>   // memcpy でコピーできるか（重要）

std::remove_const_t<T>
std::remove_reference_t<T>
std::decay_t<T>               // 参照とconstを外す（引数の「値の型」を得る）
std::conditional_t<B, T, U>   // コンパイル時の三項演算子
```

🧪 実用例: `memcpy` で高速コピーできるか判定

```cpp
template <typename T>
void fast_copy(const std::vector<T>& src, std::vector<T>& dst) {
    dst.resize(src.size());
    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(dst.data(), src.data(), src.size() * sizeof(T));
    } else {
        std::copy(src.begin(), src.end(), dst.begin());
    }
}
```

> 💡 実は `std::copy` 自体が内部でこの判定をしているので、
> 普通は自分で書く必要はありません。
> でも、バイナリファイルI/Oを書くときには自分で判定することがあります（第24章）。

---

## 15.9 可変長テンプレート（読めれば十分）

```cpp
template <typename... Args>
void log(Args&&... args) {
    (std::cout << ... << args) << "\n";      // 畳み込み式（C++17）
}

log("MW = ", 180.16, ", logP = ", 1.19);
```

`...` が「0個以上の引数」を表します。Pythonの `*args` に相当します。

```python
def log(*args):
    print(*args)
```

> 💡 自分で書く機会は少ないですが、`std::format`, `emplace_back`,
> `make_unique` がこれを使っていることを知っておくと理解が深まります。

---

## 15.10 🧪 実践: ジェネリックなフィンガープリント演算

```cpp
// code/ch15/generic_fp.cpp
#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <iostream>
#include <vector>

// ---- concept: ビットベクタの要件 ----
template <typename T>
concept BitVectorLike = requires(const T& v, std::size_t i) {
    { v.size_bits() } -> std::convertible_to<std::size_t>;
    { v.word(i) }     -> std::convertible_to<std::uint64_t>;
    { v.num_words() } -> std::convertible_to<std::size_t>;
};

// ---- 固定長版（コンパイル時にサイズ決定。最速）----
template <std::size_t NBits>
class FixedFingerprint {
    static_assert(NBits % 64 == 0, "NBits must be a multiple of 64");
    static constexpr std::size_t NWords = NBits / 64;
    std::array<std::uint64_t, NWords> words_{};

public:
    static constexpr std::size_t size_bits() { return NBits; }
    static constexpr std::size_t num_words() { return NWords; }

    std::uint64_t word(std::size_t i) const { return words_[i]; }

    void set(std::size_t bit)   { words_[bit / 64] |=  (1ULL << (bit % 64)); }
    void clear(std::size_t bit) { words_[bit / 64] &= ~(1ULL << (bit % 64)); }
    bool test(std::size_t bit) const {
        return (words_[bit / 64] >> (bit % 64)) & 1ULL;
    }

    int popcount() const {
        int n = 0;
        for (auto w : words_) n += std::popcount(w);
        return n;
    }
};

// ---- 可変長版（実行時にサイズ決定）----
class DynamicFingerprint {
    std::vector<std::uint64_t> words_;
public:
    explicit DynamicFingerprint(std::size_t nbits)
        : words_((nbits + 63) / 64, 0) {}

    std::size_t size_bits() const { return words_.size() * 64; }
    std::size_t num_words() const { return words_.size(); }
    std::uint64_t word(std::size_t i) const { return words_[i]; }

    void set(std::size_t bit) { words_[bit / 64] |= (1ULL << (bit % 64)); }
    bool test(std::size_t bit) const {
        return (words_[bit / 64] >> (bit % 64)) & 1ULL;
    }
    int popcount() const {
        int n = 0;
        for (auto w : words_) n += std::popcount(w);
        return n;
    }
};

// ---- 型に依存しない Tanimoto ----
template <BitVectorLike FP>
double tanimoto(const FP& a, const FP& b) {
    int inter = 0, uni = 0;
    const std::size_t n = a.num_words();
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t x = a.word(i), y = b.word(i);
        inter += std::popcount(x & y);
        uni   += std::popcount(x | y);
    }
    return uni ? static_cast<double>(inter) / uni : 0.0;
}

// ---- Dice 係数（同じインタフェースで動く）----
template <BitVectorLike FP>
double dice(const FP& a, const FP& b) {
    int inter = 0, total = 0;
    for (std::size_t i = 0; i < a.num_words(); ++i) {
        const std::uint64_t x = a.word(i), y = b.word(i);
        inter += std::popcount(x & y);
        total += std::popcount(x) + std::popcount(y);
    }
    return total ? 2.0 * inter / total : 0.0;
}

int main() {
    FixedFingerprint<2048> a, b;
    for (std::size_t i = 0; i < 2048; i += 3)  a.set(i);
    for (std::size_t i = 0; i < 2048; i += 5)  b.set(i);

    std::cout << "fixed:\n";
    std::cout << "  |a| = " << a.popcount() << ", |b| = " << b.popcount() << "\n";
    std::cout << "  tanimoto = " << tanimoto(a, b) << "\n";
    std::cout << "  dice     = " << dice(a, b) << "\n";

    DynamicFingerprint c(1024), d(1024);
    for (std::size_t i = 0; i < 1024; i += 3) c.set(i);
    for (std::size_t i = 0; i < 1024; i += 5) d.set(i);

    std::cout << "dynamic:\n";
    std::cout << "  tanimoto = " << tanimoto(c, d) << "\n";   // ★ 同じ関数が使える

    // sizeof を比較
    std::cout << "sizeof(FixedFingerprint<2048>)  = "
              << sizeof(FixedFingerprint<2048>) << "\n";      // 256
    std::cout << "sizeof(DynamicFingerprint)      = "
              << sizeof(DynamicFingerprint) << "\n";          // 24 (+ ヒープ 128)
}
```

**このコードの美点:**

- `tanimoto` は**1回書くだけ**で、固定長版にも可変長版にも使える
- concept `BitVectorLike` により、要件を満たさない型を渡すと
  **人間が読めるエラー**が出る
- テンプレートなので、**それぞれの型に最適化されたコードが生成される**
  （固定長版はループが完全展開される）
- **仮想関数を使っていないので、オーバーヘッドがゼロ**

継承でこれをやると、`word(i)` が仮想呼び出しになり、
インライン展開されず、数倍遅くなります。

> 💡 **これがテンプレートの正しい使い方です。**
> 「コンパイル時に型が決まる多態性」= **静的ポリモーフィズム**。
> ゼロオーバーヘッドで抽象化できます。

---

## 15.11 テンプレートを使うべきか

| 状況 | 使う? |
|---|---|
| 複数の数値型で同じ処理をしたい（float/double） | **✓ 使う** |
| コンテナや汎用アルゴリズムを書く | **✓ 使う** |
| コンパイル時にサイズを固定したい | **✓ 使う**（非型パラメータ） |
| 仮想関数のコストを避けたい | **✓ 使う** |
| 実行時に型が決まる | ✗ 継承か variant |
| プラグイン的に拡張したい | ✗ 継承 |
| 型が1つしかない | ✗ 普通のクラスで |

> 💡 **YAGNI (You Aren't Gonna Need It)**
> 「将来 float も使うかも」でテンプレート化するのは早計です。
> **2つ目の型が実際に必要になってから**テンプレート化しましょう。
> コンパイル時間とエラーメッセージの読みにくさは実コストです。

---

## 15.12 この章のまとめ

- テンプレートは「型を後から決める」仕組み。**Pythonの関数の型なし版に近い**
- **型ごとに専用のコードが生成される**（だから速い、だからコンパイルが遅い）
- **テンプレートの実装はヘッダに書く**（リンクエラーの主要因）
- 非型テンプレートパラメータ `template <std::size_t N>` で
  コンパイル時サイズ指定 → ループ完全展開、ヒープ確保なし
- **concept (C++20) でエラーメッセージが劇的に改善する**
- `if constexpr` でコンパイル時分岐
- **静的ポリモーフィズムは、仮想関数のコストなしに抽象化できる**
- テンプレートメタプログラミングは**書かなくてよい**。読めれば十分

> 📝 **練習問題 15-1**
>
> `template <typename T> T clamp(T x, T lo, T hi)` を書き、
> concept で「T は比較可能でなければならない」制約を付けてください。

> 📝 **練習問題 15-2**
>
> 15.10 の `FixedFingerprint<2048>` と `DynamicFingerprint(2048)` で
> Tanimoto を1億回計算し、速度を比較してください。
> godbolt.org で生成されたアセンブリも見比べてみましょう。

> 📝 **練習問題 15-3**
>
> `Matrix<T>` に `transpose()` と `operator*`（行列積）を追加してください。
> `T` が浮動小数点型でなければコンパイルエラーになるよう concept を付けてください。

> 📝 **練習問題 15-4**
>
> 次のコードのエラーメッセージを見て、原因を特定してください。
> ```cpp
> struct Mol { double mw; };
> std::vector<Mol> v(3);
> std::sort(v.begin(), v.end());
> ```
> concept を使って、より分かりやすいエラーにするにはどうすればよいですか?

---

→ [第16章 ラムダと関数オブジェクト](ch16-lambdas.md)
