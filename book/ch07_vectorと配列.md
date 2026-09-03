# 第7章 配列と std::vector

Pythonの `list` に相当するのが `std::vector` です。
毎日使う道具なので、内部構造まで理解しておくと大きな差がつきます。

## 7.1 生配列（C配列）— 使わないが、知っておく

```cpp
int arr[5] = {10, 20, 30, 40, 50};
int arr2[5] = {};        // 全部 0 で初期化
int arr3[] = {1, 2, 3};  // サイズは推論される（3）

std::cout << arr[0] << "\n";   // 10
```

**生配列の問題点:**

| 問題 | 内容 |
|---|---|
| サイズを覚えていない | `sizeof(arr)/sizeof(arr[0])` で計算する必要がある |
| 関数に渡すとポインタに退化 | サイズ情報が消える |
| 範囲チェックがない | `arr[100]` が通ってしまう |
| サイズが固定 | 実行時に変えられない |
| コピー・代入ができない | `arr2 = arr1;` はエラー |

```cpp
void f(int arr[5]) {        // ★ 実は int* arr と同じ。5 は無視される
    std::cout << sizeof(arr) << "\n";   // 8（ポインタのサイズ）！
}
```

これが **配列のポインタへの退化 (array-to-pointer decay)** です。
C由来の悪名高い仕様で、バグの温床です。

> 💡 **結論: 生配列は使わない。**
> 固定長なら `std::array<T, N>`、可変長なら `std::vector<T>` を使ってください。
> 生配列は「Cのライブラリと連携するとき」と「他人のコードを読むとき」だけ必要です。

---

## 7.2 std::array — 固定長配列

```cpp
#include <array>

std::array<int, 5> arr = {10, 20, 30, 40, 50};

std::cout << arr[0]     << "\n";   // 10
std::cout << arr.size() << "\n";   // 5  ★ サイズを覚えている
std::cout << arr.at(2)  << "\n";   // 30 ★ 範囲チェック付き（例外を投げる）

for (int x : arr) { ... }          // range-based for が使える

std::array<int, 5> b = arr;        // ✓ コピーできる
```

生配列と**同じ速度・同じメモリレイアウト**（スタック上に連続配置）でありながら、
安全で便利です。ゼロオーバーヘッド原則の好例。

🧪 **フィンガープリントは `std::array` の典型的な用途です:**

```cpp
using Fingerprint = std::array<std::uint64_t, 32>;   // 2048 bit
```

サイズがコンパイル時に決まっているので:
- ヒープ確保が起きない（`std::vector` より速い）
- コンパイラがループを完全展開できる
- `sizeof(Fingerprint) == 256` が保証される

---

## 7.3 std::vector — 可変長配列

### 基本操作とPython対応表

```cpp
#include <vector>

std::vector<int> v;                   // 空
std::vector<int> v2 = {1, 2, 3};      // 初期値付き
std::vector<int> v3(10);              // 要素10個、全部 0
std::vector<int> v4(10, 7);           // 要素10個、全部 7
std::vector<int> v5(v2);              // コピー
std::vector<int> v6(v2.begin(), v2.end());  // 範囲から
```

| やりたいこと | Python | C++ |
|---|---|---|
| 空のリスト | `v = []` | `std::vector<int> v;` |
| 初期値付き | `v = [1,2,3]` | `std::vector<int> v = {1,2,3};` |
| n個の0 | `v = [0]*n` | `std::vector<int> v(n);` |
| n個のx | `v = [x]*n` | `std::vector<int> v(n, x);` |
| 末尾に追加 | `v.append(x)` | `v.push_back(x)` / `v.emplace_back(x)` |
| 末尾を削除 | `v.pop()` | `v.pop_back()`（**値は返さない**） |
| 長さ | `len(v)` | `v.size()` |
| 空か | `not v` | `v.empty()` |
| 要素アクセス | `v[i]` | `v[i]`（チェックなし） / `v.at(i)`（チェックあり） |
| 先頭/末尾 | `v[0]` / `v[-1]` | `v.front()` / `v.back()` |
| 全消去 | `v.clear()` | `v.clear()` |
| 連結 | `v1 + v2` | `v1.insert(v1.end(), v2.begin(), v2.end())` |
| スライス | `v[2:5]` | `std::vector<int>(v.begin()+2, v.begin()+5)` |
| 逆順 | `v[::-1]` | `std::reverse(v.begin(), v.end())` |
| ソート | `v.sort()` | `std::sort(v.begin(), v.end())` |
| 含むか | `x in v` | `std::find(v.begin(), v.end(), x) != v.end()` |
| 最大値 | `max(v)` | `*std::max_element(v.begin(), v.end())` |
| 合計 | `sum(v)` | `std::accumulate(v.begin(), v.end(), 0)` |
| 挿入 | `v.insert(i, x)` | `v.insert(v.begin()+i, x)` |
| 削除 | `del v[i]` | `v.erase(v.begin()+i)` |

> ⚠️ **`v.pop_back()` は値を返しません。** Pythonの `pop()` と違います。
> ```cpp
> int last = v.back();   // 先に取り出して
> v.pop_back();          // それから削除
> ```
> なぜかというと、例外安全性のためです（値を返す途中で例外が飛ぶと要素が失われる）。

> ⚠️ **`v[-1]` は使えません。** 未定義動作です。`v.back()` を使ってください。

### `[]` と `.at()`

```cpp
std::vector<int> v = {1, 2, 3};
v[10];       // ★ 未定義動作。何が起きるか分からない
v.at(10);    // ✓ std::out_of_range 例外を投げる
```

`.at()` は安全ですが、毎回チェックが入るので少し遅いです。

> 💡 **実務での使い分け**
> - ホットループ内で、添字が正しいと確信できる → `[]`
> - ユーザー入力やファイルから来た添字 → `.at()`
> - デバッグ時 → `-D_GLIBCXX_ASSERTIONS`（GCC）を付けると `[]` もチェックされる
>
> ```bash
> g++ -std=c++20 -g -D_GLIBCXX_ASSERTIONS -fsanitize=address prog.cpp
> ```
> これで `[]` の範囲外アクセスも検出できます。開発中は常に付けましょう。

---

## 7.4 ★重要★ vector の内部構造と reserve

### vector の正体

```cpp
// 概念図（実際の実装に近い）
template <typename T>
class vector {
    T*          data_;      // ヒープ上のデータへのポインタ
    std::size_t size_;      // 現在の要素数
    std::size_t capacity_;  // 確保済みの容量
};
```

```
   スタック                       ヒープ
   +-------------+        +----+----+----+----+----+----+----+----+
   | data ------ |------->| 1  | 2  | 3  |    |    |    |    |    |
   | size:     3 |        +----+----+----+----+----+----+----+----+
   | capacity: 8 |        <---- size=3 ---->
   +-------------+        <-------------- capacity=8 ------------->
```

- **size**: 実際に入っている要素数（`v.size()`）
- **capacity**: 確保済みのメモリで入る最大数（`v.capacity()`）

### push_back で何が起きるか

`size < capacity` なら、末尾に書き込むだけ（超高速）。

`size == capacity` になったら:

1. **より大きなメモリをヒープに確保**（通常 capacity の2倍）
2. **既存の全要素を新しい領域にコピー/ムーブ**
3. 古い領域を解放
4. 新しい要素を追加

```
capacity が足りない！
   +----+----+----+----+
   | 1  | 2  | 3  | 4  |   capacity=4, size=4
   +----+----+----+----+
          │
          │ 8個分の新領域を確保して全部コピー
          ▼
   +----+----+----+----+----+----+----+----+
   | 1  | 2  | 3  | 4  | 5  |    |    |    |   capacity=8, size=5
   +----+----+----+----+----+----+----+----+
```

**この再確保が性能の敵です。**

### 実測してみる

```cpp
// code/ch07/reserve_bench.cpp
#include <chrono>
#include <iostream>
#include <vector>

struct Timer {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    double ms() const {
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - t0).count();
    }
};

int main() {
    constexpr std::size_t N = 10'000'000;

    {
        Timer t;
        std::vector<int> v;
        for (std::size_t i = 0; i < N; ++i) v.push_back(static_cast<int>(i));
        std::cout << "without reserve: " << t.ms() << " ms\n";
    }
    {
        Timer t;
        std::vector<int> v;
        v.reserve(N);                     // ★ 一発で確保
        for (std::size_t i = 0; i < N; ++i) v.push_back(static_cast<int>(i));
        std::cout << "with reserve   : " << t.ms() << " ms\n";
    }

    // capacity の成長を観察
    std::vector<int> v;
    std::size_t last = 0;
    for (int i = 0; i < 100; ++i) {
        v.push_back(i);
        if (v.capacity() != last) {
            std::cout << "size=" << v.size() << " capacity=" << v.capacity() << "\n";
            last = v.capacity();
        }
    }
}
```

出力例:

```
without reserve: 42.1 ms
with reserve   : 11.3 ms
size=1 capacity=1
size=2 capacity=2
size=3 capacity=4
size=5 capacity=8
size=9 capacity=16
size=17 capacity=32
size=33 capacity=64
size=65 capacity=128
```

capacity が 1→2→4→8→16... と倍々になっているのが見えます。
`reserve` で**4倍近く速くなりました**。

> 💡 **鉄則: 最終的な要素数が分かっているなら必ず `reserve` する**
>
> ```cpp
> std::vector<Fingerprint> fps;
> fps.reserve(smiles_list.size());       // ← これを書くだけ
> for (const auto& s : smiles_list) {
>     fps.push_back(compute_fp(s));
> }
> ```
> `Fingerprint` が256バイトなら、100万分子で256MBのコピーが何度も発生します。
> `reserve` 1行で劇的に変わります。

> 🐍 **Pythonのリストも同じ仕組み**です（`list.append` は償却O(1)）。
> Pythonには `reserve` に相当するAPIがありませんが、
> `[None] * n` で確保してから代入すると同様の効果が得られます。

### `resize` と `reserve` の違い

```cpp
std::vector<int> v;
v.reserve(10);   // capacity=10, size=0   ★ 要素は作られない
v.resize(10);    // capacity=10, size=10  ★ 要素10個が 0 で作られる

v.reserve(10);
v[0] = 1;        // ★ 未定義動作！ size は 0 なので要素は存在しない
```

- `reserve`: メモリだけ確保。`push_back` の準備
- `resize`: 実際に要素を作る。`v[i] = x` で代入する準備

```cpp
// パターンA: push_back する
std::vector<double> v;
v.reserve(n);
for (...) v.push_back(x);

// パターンB: 添字で代入する
std::vector<double> v(n);       // または v.resize(n)
for (std::size_t i = 0; i < n; ++i) v[i] = x;
```

### shrink_to_fit

```cpp
v.clear();            // size=0 になるが、capacity はそのまま（メモリは解放されない）
v.shrink_to_fit();    // capacity を size に合わせる（メモリ解放）

// 確実に解放したいイディオム
std::vector<int>().swap(v);   // 空のvectorと交換
```

---

## 7.5 ⚠️ イテレータ・参照の無効化

**これはC++で最も見落とされやすいバグです。**

```cpp
std::vector<int> v = {1, 2, 3};
int& r = v[0];          // v[0] への参照
int* p = &v[0];
auto it = v.begin();

v.push_back(4);         // ★ 再確保が起きるかもしれない

std::cout << r;         // ★ ダングリング！ 未定義動作
std::cout << *p;        // ★ 同上
std::cout << *it;       // ★ 同上
```

再確保が起きると、データは**別のアドレスに引っ越します**。
古い場所を指していた参照・ポインタ・イテレータは全部無効になります。

```
  push_back の前         push_back の後
  0x1000: [1][2][3]  →   0x1000: (解放済み)
     ▲                    0x5000: [1][2][3][4]
     r, p, it                       
   (無効な場所を指したまま)
```

### 無効化のルール

| 操作 | 無効化されるもの |
|---|---|
| `push_back`, `emplace_back`, `insert`, `resize` | **再確保が起きれば全部**。起きなければ挿入位置以降 |
| `reserve` | 容量が増えるなら**全部** |
| `erase` | 削除位置以降 |
| `clear` | 全部 |
| `pop_back` | 末尾要素への参照のみ |
| `operator[]`, `at`, `front`, `back`, `size` | 無効化しない（読むだけ） |

> ⚠️ **典型的な事故: ループ中の push_back**
> ```cpp
> for (auto& mol : mols) {
>     if (needs_split(mol)) {
>         mols.push_back(split(mol));   // ★ ループ中のイテレータが無効化！
>     }
> }
> ```
> Pythonでも「ループ中にリストを変更するな」と言われますが、
> Pythonは論理エラーで済むのに対し、**C++はクラッシュか未定義動作**です。
>
> 対策: 別のvectorに集めて後で結合する。
> ```cpp
> std::vector<Molecule> new_mols;
> for (const auto& mol : mols) {
>     if (needs_split(mol)) new_mols.push_back(split(mol));
> }
> mols.insert(mols.end(), new_mols.begin(), new_mols.end());
> ```

> 💡 **`std::deque` や `std::list` は無効化ルールが違います**（第9章）。
> ポインタの安定性が必要なら `std::deque` が選択肢になります。

---

## 7.6 push_back vs emplace_back

```cpp
struct Atom {
    int  atomic_num;
    bool aromatic;
    Atom(int z, bool a) : atomic_num(z), aromatic(a) {}
};

std::vector<Atom> atoms;

atoms.push_back(Atom(6, true));     // 一時オブジェクトを作ってからムーブ
atoms.emplace_back(6, true);        // ★ vector の中で直接構築（コピー/ムーブなし）
```

`emplace_back` は引数をそのままコンストラクタに転送し、
**vector の内部メモリ上で直接オブジェクトを作ります**。

> 💡 **迷ったら `emplace_back`**。ただし:
> - 既存のオブジェクトを追加するなら `push_back(x)` の方が意図が明確
> - `emplace_back` は explicit なコンストラクタも呼べてしまうので、
>   意図しない変換が起きることがある
>
> 実用上の差は、重いオブジェクトのときだけ意味があります。
> `std::vector<int>` に対しては全く同じです。

---

## 7.7 二次元配列

### ❌ よくある悪い書き方

```cpp
std::vector<std::vector<double>> matrix(1000, std::vector<double>(1000));
```

これは動きますが、**遅い**です。理由:

```
  matrix (外側のvector)
  +------+------+------+ ...
  | ptr  | ptr  | ptr  |
  +--+---+--+---+--+---+
     │      │      │
     ▼      ▼      ▼
  [1000個] [1000個] [1000個]   ← 1000個のバラバラなヒープ領域
   0x8A00   0x1F30   0xC720      （メモリ上で隣接していない）
```

- ヒープ確保が**1001回**発生
- 行ごとにメモリが飛ぶ → **キャッシュミス**
- ポインタの間接参照が毎回入る

### ✅ 正しい書き方: フラット配列

```cpp
class Matrix {
    std::size_t rows_, cols_;
    std::vector<double> data_;
public:
    Matrix(std::size_t r, std::size_t c) : rows_(r), cols_(c), data_(r * c) {}

    double&       operator()(std::size_t i, std::size_t j)       { return data_[i * cols_ + j]; }
    const double& operator()(std::size_t i, std::size_t j) const { return data_[i * cols_ + j]; }

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    double*       data()       { return data_.data(); }
    const double* data() const { return data_.data(); }
};

Matrix m(1000, 1000);
m(3, 5) = 1.0;
```

```
  data_ (1個の連続領域、1000*1000*8 = 8MB)
  +----+----+----+ ... +----+----+ ... 
  | 行0の1000個        | 行1の1000個   ...
  +----+----+----+ ... +----+----+ ...
```

- ヒープ確保は**1回**
- 完全に連続 → キャッシュ効率最高
- `data()` でNumPyやBLASにそのまま渡せる

> 🐍 **NumPyと同じ考え方です。**
> `np.zeros((1000,1000))` は内部で1本のフラット配列を持ち、
> shape と strides で2次元に見せています。
> C++でも同じことをしてください。
>
> `i * cols + j` が **row-major（C順）**、`j * rows + i` が **column-major（Fortran順）**。
> NumPyのデフォルトは row-major です。

> 💡 C++23 には `std::mdspan` という多次元ビューが入りました。
> 使えるなら:
> ```cpp
> #include <mdspan>
> std::vector<double> buf(1000*1000);
> std::mdspan m(buf.data(), 1000, 1000);
> m[3, 5] = 1.0;
> ```

---

## 7.8 vector<bool> の罠

```cpp
std::vector<bool> v(10);
bool& r = v[0];      // ✗ コンパイルエラー！
```

**`std::vector<bool>` は普通のvectorではありません。**
1要素を1ビットで格納する特殊化（ビットセット）になっています。

メリット: メモリが1/8
デメリット:
- `operator[]` が `bool&` を返さない（プロキシオブジェクトを返す）
- `bool* p = &v[0];` ができない
- 参照が取れないのでアルゴリズムで問題を起こす
- ビット操作のため個別アクセスが遅い

> 💡 **対策**
> - 単なる真偽値の配列なら **`std::vector<char>`** か `std::vector<std::uint8_t>`
> - ビットセットとして使いたいなら **`std::bitset<N>`**（固定長）か
>   **`std::vector<std::uint64_t>` を自分で管理**（第22章のフィンガープリント）
>
> ```cpp
> std::vector<char> visited(n, 0);   // ✓ 普通に使える
> std::vector<std::uint8_t> flags(n, 0);
> ```

---

## 7.9 🧪 実践: 分子データセットを保持する

```cpp
// code/ch07/dataset.cpp
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using Fingerprint = std::array<std::uint64_t, 32>;   // 2048 bit

// ---------- 設計A: 構造体の配列 (AoS: Array of Structures) ----------
struct MoleculeAoS {
    std::string  smiles;
    std::string  id;
    double       mw;
    double       logp;
    Fingerprint  fp;
    float        activity;
};

// ---------- 設計B: 配列の構造体 (SoA: Structure of Arrays) ----------
struct DatasetSoA {
    std::vector<std::string>  smiles;
    std::vector<std::string>  ids;
    std::vector<double>       mw;
    std::vector<double>       logp;
    std::vector<std::uint64_t> fps;   // フラット化: 分子i は fps[i*32 .. i*32+31]
    std::vector<float>        activity;

    std::size_t size() const { return smiles.size(); }

    void reserve(std::size_t n) {
        smiles.reserve(n); ids.reserve(n);
        mw.reserve(n); logp.reserve(n);
        fps.reserve(n * 32);
        activity.reserve(n);
    }

    void add(std::string s, std::string i, double m, double lp,
             const Fingerprint& f, float act) {
        smiles.push_back(std::move(s));
        ids.push_back(std::move(i));
        mw.push_back(m);
        logp.push_back(lp);
        fps.insert(fps.end(), f.begin(), f.end());
        activity.push_back(act);
    }

    // 分子 i のフィンガープリントの先頭ポインタ
    const std::uint64_t* fp_at(std::size_t i) const { return fps.data() + i * 32; }
};

int main() {
    constexpr std::size_t N = 100'000;

    DatasetSoA ds;
    ds.reserve(N);

    Fingerprint dummy{};
    for (std::size_t i = 0; i < N; ++i) {
        dummy[i % 32] = i;                // 適当な値
        ds.add("CCO", "MOL" + std::to_string(i),
               46.07 + i * 0.01, -0.31, dummy, static_cast<float>(i % 10));
    }

    std::cout << "molecules: " << ds.size() << "\n";
    std::cout << "fp words : " << ds.fps.size() << "\n";
    std::cout << "mw[100]  : " << ds.mw[100] << "\n";

    // SoA の強み: 「全分子のMWの平均」が連続メモリのスキャンで済む
    double sum = 0;
    for (double m : ds.mw) sum += m;
    std::cout << "mean MW  : " << sum / ds.size() << "\n";

    return 0;
}
```

### AoS vs SoA — どちらを選ぶか

| | AoS (`vector<Molecule>`) | SoA (`struct { vector<...>; }`) |
|---|---|---|
| 1分子の全情報にアクセス | **速い**（1箇所にまとまっている） | 遅い（複数のvectorを飛ぶ） |
| 1つのフィールドを全分子でスキャン | 遅い（不要なデータもキャッシュに載る） | **速い**（必要なデータだけ連続） |
| 可読性 | **高い** | 低い |
| SIMD化 | しにくい | **しやすい** |

> 🧪 **ケモインフォマティクスでの使い分け**
>
> - **AoS**: 分子を1個ずつ処理する（パース、記述子計算、描画）
> - **SoA**: 全分子に対する一括処理（類似度検索、フィルタリング、統計）
>
> 大規模スクリーニング（第23章）では **SoA が2〜3倍速い**です。
> 「MWだけ見て500以上を弾く」処理で、SMILES文字列やFPをキャッシュに
> 載せる必要がないからです。
>
> まずAoSで書いて、遅ければSoAに変える、が現実的です。

---

## 7.10 vector の便利メソッド

```cpp
std::vector<int> v = {5, 3, 8, 1, 9};

// --- 追加 ---
v.push_back(4);                       // 末尾に追加
v.emplace_back(4);                    // 末尾に直接構築
v.insert(v.begin() + 2, 100);         // 位置2に挿入（遅い: O(n)）
v.insert(v.end(), other.begin(), other.end());   // 連結

// --- 削除 ---
v.pop_back();                         // 末尾削除
v.erase(v.begin() + 2);               // 位置2を削除（遅い: O(n)）
v.erase(v.begin() + 1, v.begin() + 3);// 範囲削除
v.clear();                            // 全消去

// --- 条件を満たす要素を全削除（erase-remove イディオム）---
v.erase(std::remove_if(v.begin(), v.end(),
                       [](int x){ return x < 5; }),
        v.end());
// C++20 なら1行:
std::erase_if(v, [](int x){ return x < 5; });
std::erase(v, 42);                    // 値42を全削除

// --- 情報 ---
v.size();       v.capacity();     v.empty();
v.front();      v.back();
v.data();       // 生ポインタ（C APIやNumPyに渡すとき）

// --- その他 ---
v.assign(10, 0);      // 中身を「0が10個」に置き換え
v.swap(other);        // 中身を交換（O(1)！ ポインタを入れ替えるだけ）
```

> 💡 **`swap` は O(1) です。** vector が持っているのはポインタ・size・capacity の
> 3つだけなので、それを交換すれば済みます。巨大なvectorでも一瞬です。

### erase-remove イディオム（重要）

Pythonなら:

```python
v = [x for x in v if x >= 5]
```

C++（C++20以前）:

```cpp
v.erase(std::remove_if(v.begin(), v.end(),
                       [](int x){ return x < 5; }),
        v.end());
```

なぜ2段階なのか:
1. `std::remove_if` は「残す要素を前に詰める」だけ。**サイズは変わらない**
2. `erase` で末尾のゴミを実際に削除する

```
元:        [5][3][8][1][9]
remove_if: [5][8][9][?][?]  ← 戻り値は「新しい末尾」のイテレータ
erase:     [5][8][9]
```

C++20 の `std::erase_if` はこれを1行にまとめたものです。**新しいコードではこちらを。**

---

## 7.11 この章のまとめ

- 生配列は使わない。**固定長は `std::array`、可変長は `std::vector`**
- vector は `{data ポインタ, size, capacity}` の3つを持つ構造体
- **`push_back` の前に `reserve` する**（数倍速くなる）
- `reserve` は容量だけ、`resize` は要素も作る
- **再確保でイテレータ・参照・ポインタが無効化される**
- `[]` はチェックなし、`.at()` はチェックあり
- **二次元データはフラット配列 + `i*cols+j`**（vector of vector は遅い）
- **`std::vector<bool>` は特殊。使うな**。`std::vector<char>` か `std::bitset`
- 削除は `std::erase_if`（C++20）または erase-remove イディオム
- 大規模一括処理なら SoA を検討

> 📝 **練習問題 7-1**
>
> `reserve` あり/なしで、`std::vector<std::string>` に100万個の
> SMILES文字列を追加する時間を比較してください。
> `int` のときより差が大きくなるはずです。なぜでしょうか?

> 📝 **練習問題 7-2**
>
> 7.7 の `Matrix` クラスに、行の合計を返すメソッド
> `double row_sum(std::size_t i) const` と
> 列の合計を返す `double col_sum(std::size_t j) const` を追加し、
> 実行時間を比較してください。どちらが速く、それはなぜですか?

> 📝 **練習問題 7-3**
>
> 次のコードのバグを見つけてください。
> ```cpp
> std::vector<int> v = {1,2,3,4,5};
> for (auto it = v.begin(); it != v.end(); ++it) {
>     if (*it % 2 == 0) v.erase(it);
> }
> ```
> （ヒント: `erase` の後の `it` は?）

---

→ [第8章 文字列](ch08_文字列.md)
