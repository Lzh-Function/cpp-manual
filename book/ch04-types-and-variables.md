# 第4章 型と変数 — 「なんでも入る箱」との決別

Pythonから来た人が最初に面食らうのが型です。
でも実は、あなたは既に型を知っています。`np.float32` と `np.float64` を
使い分けたことがあるなら、それがC++の世界です。

## 4.1 静的型付けとは

```python
x = 3          # int
x = "hello"    # 同じ変数に str を入れられる
x = [1, 2, 3]  # list も入る
```

```cpp
int x = 3;
x = "hello";   // ✗ コンパイルエラー！
```

Pythonの変数は「オブジェクトへの付箋」で、貼り替え自由。
C++の変数は「特定の型のデータを置くための、決まったサイズのメモリ領域」です。

```
Python:  x ──付箋──> [int 3]
         x ──付箋──> ["hello"]     貼り替えただけ

C++:     x = [ 4バイトの整数領域 ]  ここに整数以外は物理的に置けない
```

> 🐍 **Pythonとの対比**
>
> `x: int = 3` という型ヒントを書いたことがありますか?
> Pythonの型ヒントは**実行時に無視されます**（mypyが静的にチェックするだけ）。
> C++の型は**コンパイラが強制**します。型が合わなければビルドが通りません。
>
> 面倒に見えますが、これは**巨大な恩恵**です。
> 「実行3時間後に `TypeError` で落ちる」が原理的に起きません。

---

## 4.2 基本型の一覧

### 整数型

| 型 | サイズ | 範囲 | Pythonでの相当物 |
|---|---|---|---|
| `bool` | 1バイト | `true` / `false` | `bool` |
| `char` | 1バイト | -128〜127（処理系依存） | — |
| `signed char` | 1バイト | -128 〜 127 | `np.int8` |
| `unsigned char` | 1バイト | 0 〜 255 | `np.uint8` |
| `short` | 2バイト | -32768 〜 32767 | `np.int16` |
| `int` | **4バイト** | 約 ±21億 | `np.int32` |
| `long` | 4or8バイト | **処理系依存！** | — |
| `long long` | 8バイト | 約 ±9.2×10^18 | `np.int64` |
| `unsigned int` | 4バイト | 0 〜 約43億 | `np.uint32` |
| `std::size_t` | 8バイト(64bit環境) | 0 〜 約1.8×10^19 | 配列添字用 |

> ⚠️ **`long` は使わないでください。**
> Linux 64bitでは8バイト、Windows 64bitでは**4バイト**です。
> 同じコードが環境で違う動きをします。8バイト欲しいなら `long long` か、
> 次に説明する `std::int64_t` を使ってください。

### 固定幅整数型（おすすめ）

```cpp
#include <cstdint>

std::int8_t   a;   // 必ず8bit  符号あり
std::int16_t  b;   // 必ず16bit
std::int32_t  c;   // 必ず32bit
std::int64_t  d;   // 必ず64bit
std::uint8_t  e;   // 必ず8bit  符号なし
std::uint32_t f;
std::uint64_t g;   // フィンガープリントのビット演算でよく使う
```

> 💡 **本書の方針**
> - 普通のループカウンタ、個数 → `int`
> - 配列の添字、`size()` の戻り値 → `std::size_t`
> - サイズが重要なとき（バイナリI/O、ビット演算、メモリ節約） → `std::int32_t` などの固定幅型
>
> NumPy の `dtype` を選ぶときと同じ感覚です。

### 浮動小数点型

| 型 | サイズ | 有効桁数 | NumPy相当 |
|---|---|---|---|
| `float` | 4バイト | 約7桁 | `np.float32` |
| `double` | **8バイト** | 約15桁 | `np.float64` = Pythonの `float` |
| `long double` | 8〜16バイト | 処理系依存 | — |

**C++の `double` が、Pythonの `float` です。** 名前が紛らわしいので注意。

```python
x = 3.14          # Python の float は 64bit = C++ の double
```

```cpp
double x = 3.14;  // これが正解
float  y = 3.14f; // 32bit にしたいときは f を付ける
```

> 💡 **いつ `float` を使うか**
>
> 分子記述子や座標データを大量に持つとき、`float` にすると**メモリが半分**になり、
> キャッシュ効率が2倍になります。SIMD命令も2倍のデータを一度に処理できます。
> 深層学習で `float32` を使うのと全く同じ理由です。
>
> ただし、**累積誤差が問題になる計算（力場の積算、統計量）では `double`** を使ってください。

### 文字型

```cpp
char c = 'A';           // シングルクォートは1文字
const char* s = "ABC";  // ダブルクォートは文字列
std::string t = "ABC";  // C++の文字列（第8章）
```

> ⚠️ **`'A'` と `"A"` は全く別物です。**
> - `'A'` は `char` 型（1バイトの数値。ASCIIで65）
> - `"A"` は `const char[2]`（'A' と終端の '\0'）
>
> Pythonでは両方とも同じ `str` なので、ここは切り替えが必要です。

```cpp
char c = 'C';
int n = c;              // 67 になる（'C' のASCIIコード）
std::cout << c << "\n"; // C
std::cout << n << "\n"; // 67
```

🧪 これはSMILESパースで実際に使います:

```cpp
// SMILES の中の環結合番号 '1'〜'9' を数値に変換
char c = '3';
int ring_id = c - '0';   // 51 - 48 = 3
```

### void

「型がない」ことを表す特別な型。関数が値を返さないときに使います。

```cpp
void print_banner() {          // 戻り値なし（Pythonの None を返す関数）
    std::cout << "=====\n";
}
```

---

## 4.3 変数の宣言と初期化

### 4つの書き方

```cpp
int a = 42;       // コピー初期化（一番よく見る）
int b(42);        // 直接初期化
int c{42};        // ★ 波括弧初期化（C++11以降の推奨）
int d = {42};     // 波括弧 + イコール
```

**`{}` を推奨する理由: 縮小変換を防いでくれるから。**

```cpp
int x = 3.9;      // ✓ コンパイルは通る。x は 3 になる（小数点以下切り捨て！）
int y{3.9};       // ✗ コンパイルエラー: narrowing conversion
```

`int x = 3.9;` は静かにデータを壊します。`{}` ならコンパイラが止めてくれます。

> 💡 **本書の方針**: 迷ったら `{}`。ただし可読性のため
> `int n = 0;` のような明らかな場合は `=` も使います。

### ⚠️ 未初期化変数という地雷

```cpp
int x;                    // 初期化していない
std::cout << x << "\n";   // ★ 未定義動作！ 何が出るか分からない
```

Pythonでは変数を作った時点で必ず値がありますが、
C++では**初期化しないとゴミが入っています**。
前にそのメモリを使っていた誰かのデータの残骸です。

```cpp
int x{};      // ✓ 0 で初期化される
int y = 0;    // ✓ 同じ
double d{};   // ✓ 0.0
bool b{};     // ✓ false
```

> ⚠️ これは初心者バグの巨大な供給源です。
> **必ず初期化してください。** `-Wall` を付けていれば
> `warning: 'x' is used uninitialized` と教えてくれることが多いです。
> `-fsanitize=memory` や Valgrind でも検出できます（第29章）。

### auto — 型推論

```cpp
auto x = 42;          // int と推論
auto y = 3.14;        // double
auto z = 3.14f;       // float
auto s = "hello";     // const char*  （std::string ではない！注意）
auto t = std::string("hello");  // std::string
```

`auto` は「コンパイラよ、右辺から型を推論してくれ」という指示です。
**動的型付けではありません。** 型はコンパイル時に確定します。

```python
x = 42        # Python: 実行時に int オブジェクトが作られる
```

```cpp
auto x = 42;  // C++: コンパイル時に int と決まる。実行時コストゼロ
```

`auto` が真価を発揮するのは、型名が長いときです:

```cpp
// auto なし（読めない）
std::unordered_map<std::string, std::vector<int>>::const_iterator it
    = fingerprint_index.find("CCO");

// auto あり
auto it = fingerprint_index.find("CCO");
```

> 💡 **`auto` を使うべき/使わないべき**
>
> 使う:
> - イテレータ（型名が長すぎる）
> - range-based for（`for (const auto& x : v)`）
> - ラムダの戻り値
> - `std::make_unique` などファクトリ関数の戻り値
>
> 使わない:
> - 型が読者にとって重要なとき（`auto n = compute();` は何が返るか不明）
> - APIの境界（関数の戻り値型は明示した方が親切）

> ⚠️ **`auto` の落とし穴: 参照が落ちる**
>
> ```cpp
> std::vector<Molecule> mols = load();
> for (auto m : mols) {   // ★ mols の要素を毎回コピーしている！
>     m.compute();        // コピーを変更しても元は変わらない
> }
> for (auto& m : mols) {  // ✓ 参照。コピーなし。変更が反映される
>     m.compute();
> }
> for (const auto& m : mols) {  // ✓ 読むだけならこれ
>     std::cout << m.name();
> }
> ```
>
> **これは実務で最も多いパフォーマンスバグです。** 第6章で詳しくやります。
> 「読むだけなら `const auto&`」と覚えてください。

---

## 4.4 const — 変更しないという約束

```cpp
const int MAX_ATOMS = 1000;
MAX_ATOMS = 2000;   // ✗ コンパイルエラー
```

Pythonには本物の定数がありません（`MAX_ATOMS = 1000` は慣習で大文字にしているだけ）。
C++の `const` はコンパイラが強制します。

**`const` の本当の価値は、定数を作ることではなく「意図を型で表現すること」です。**

```cpp
// この関数は smiles を変更しない、と保証している
int count_atoms(const std::string& smiles);

// この関数は mol を変更するかもしれない
void add_hydrogens(Molecule& mol);
```

関数のシグネチャを見ただけで、引数が書き換わるかどうか分かります。
これは巨大なコードベースで効いてきます。

> 💡 **const は「型で書く単体テスト」です。**
> 「この値は変わらない」という主張をコンパイラに検証させています。
> できる限り `const` を付けてください。デメリットは何もありません。

### const の位置に注意

```cpp
const int x = 5;              // x は変更不可
int const x = 5;              // 同じ意味（読み方が違うだけ）

const int* p;                 // p が指す先が変更不可（p 自体は変えられる）
int* const p = &x;            // p 自体が変更不可（指す先は変えられる）
const int* const p = &x;      // 両方変更不可
```

読み方のコツ: **右から左に読む**。

- `const int* p` → `p` is a pointer to an int that is const
- `int* const p` → `p` is a const pointer to an int

実務では `const T&`（constな参照）が圧倒的によく出ます。第6章で。

### constexpr — コンパイル時定数

```cpp
constexpr int FP_BITS = 2048;
constexpr int FP_WORDS = FP_BITS / 64;   // コンパイル時に 32 と計算される

std::array<std::uint64_t, FP_WORDS> fp;  // 配列サイズに使える
```

`const` は「実行時に決まるが以後変わらない」でもOKですが、
`constexpr` は「**コンパイル時に値が確定している**」ことを要求します。
配列サイズやテンプレート引数に使えます。

関数も `constexpr` にできます:

```cpp
constexpr int words_for_bits(int bits) {
    return (bits + 63) / 64;
}
constexpr int N = words_for_bits(2048);   // コンパイル時に 32 と計算
std::array<std::uint64_t, N> fp;
```

コンパイル時に計算が終わるので、実行時コストがゼロになります。

---

## 4.5 型変換（キャスト）

### 暗黙の変換（勝手に起きる）

```cpp
int    i = 3;
double d = i;      // int → double。安全（値は保たれる）
int    j = 3.9;    // double → int。★ 3 になる。情報が失われる
```

C++は結構勝手に変換します。これが罠になります。

### ⚠️ 整数の割り算

```cpp
int a = 7, b = 2;
double x = a / b;         // ★ 3.0 になる（3.5 ではない！）
```

`a / b` の時点で**両方 int なので整数除算**が行われ、結果の 3 が
その後 double に変換されます。

```cpp
double x = static_cast<double>(a) / b;   // ✓ 3.5
double y = a / static_cast<double>(b);   // ✓ 3.5
double z = 1.0 * a / b;                  // ✓ 3.5（よく使う小技）
```

> 🐍 **Pythonとの対比**
> Python 3 では `7 / 2 == 3.5`、`7 // 2 == 3`。
> C++には `//` がなく、`/` が**両辺の型で挙動を変えます**。
> これはケモインフォマティクスで実際に事故ります:
>
> ```cpp
> int inter = 12, uni = 40;
> double tanimoto = inter / uni;   // ★ 0.0 になる！！
> ```
> Tanimoto係数が全部0になって「バグった」と半日悩む、という事故が実在します。

### 明示的なキャスト

C++には4種類のキャストがあります。

```cpp
static_cast<double>(i)       // 通常の型変換。これを使う
const_cast<int*>(p)          // const を外す。ほぼ使わない（設計ミスのサイン）
reinterpret_cast<char*>(p)   // ビット列の再解釈。危険。ほぼ使わない
dynamic_cast<Derived*>(base) // 継承関係の安全なダウンキャスト（第14章）
```

古いCスタイルのキャスト `(double)i` も使えますが、**使わないでください**。
どの種類のキャストが行われるか不明で、検索もできません。

> 💡 `static_cast` は長くてタイプが面倒ですが、それは**意図的**です。
> 「型変換は本来避けるべきもの」という思想の表れです。
> 長いおかげで `grep` で探せるという実務的な利点もあります。

### ⚠️ 符号あり/なしの混在

これは本当に事故ります。

```cpp
std::vector<int> v = {1, 2, 3};
for (int i = 0; i < v.size(); ++i) { ... }
// warning: comparison of integer expressions of different signedness
```

`v.size()` は `std::size_t`（符号なし64bit）を返します。
`int`（符号あり32bit）と比較すると、`int` が符号なしに変換されます。

致命的な例:

```cpp
std::vector<int> v;   // 空！
for (std::size_t i = 0; i < v.size() - 1; ++i) {   // ★ 無限ループ
    // v.size() は 0。0 - 1 は符号なしなので 18446744073709551615 になる
}
```

対策:

```cpp
// 1. range-based for を使う（最も安全）
for (const auto& x : v) { ... }

// 2. インデックスが必要なら size_t を使い、引き算を避ける
for (std::size_t i = 0; i + 1 < v.size(); ++i) { ... }

// 3. C++20 なら std::ssize（符号付きサイズ）
for (std::ptrdiff_t i = 0; i < std::ssize(v); ++i) { ... }
```

---

## 4.6 浮動小数点の罠（ケモインフォ研究者向け）

これはPythonでも同じですが、C++では最適化が絡んでさらに複雑になります。

### 等値比較をしてはいけない

```cpp
double a = 0.1 + 0.2;
if (a == 0.3) { ... }    // ★ 成立しない
```

```cpp
#include <cmath>
constexpr double EPS = 1e-9;
if (std::abs(a - 0.3) < EPS) { ... }   // ✓
```

分子の座標や記述子を比較するときは必ず許容誤差を使ってください。

### NaN の扱い

```cpp
#include <cmath>
double x = std::sqrt(-1.0);      // NaN
if (x == x) { }                  // false! NaN は自分自身と等しくない
if (std::isnan(x)) { }           // ✓ こちらを使う
if (std::isfinite(x)) { }        // NaN でも inf でもない
```

> 🧪 記述子計算では NaN が頻繁に発生します（logP計算の失敗、0除算など）。
> 出力の前に `std::isfinite` でチェックする習慣をつけてください。
> NaN を含む配列を `std::sort` に渡すと**未定義動作**（クラッシュもあり得る）です。

### ⚠️ `-ffast-math` の危険性

「速くなるらしい」と `-ffast-math` を付けると、
コンパイラは「NaN や inf は存在しない」と仮定して最適化します。
その結果、`std::isnan(x)` が**常に false を返す**ようになります。

科学計算では基本的に使わないでください。使うなら影響を完全に理解した上で。

---

## 4.7 型エイリアス

長い型名に短い名前を付けられます。

```cpp
using AtomIndex = std::size_t;
using Fingerprint = std::array<std::uint64_t, 32>;
using MolMap = std::unordered_map<std::string, Fingerprint>;

Fingerprint fp;
MolMap index;
```

古い書き方 `typedef std::size_t AtomIndex;` も見ますが、
`using` の方が読みやすく、テンプレートにも使えるので `using` を推奨します。

```python
# Python の型エイリアスと同じ発想
AtomIndex = int
Fingerprint = list[int]
```

> 💡 型エイリアスは**ドキュメント**です。
> `std::size_t` と書くより `AtomIndex` と書いた方が意図が伝わります。
> ただし、型としては同じなので、`AtomIndex` に `BondIndex` を代入しても
> エラーになりません。本当に区別したいなら「強い型」を作る必要があります（第11章）。

---

## 4.8 スコープと寿命

```cpp
int main() {
    int x = 1;
    {                       // 新しいブロック
        int y = 2;
        std::cout << x + y; // 3。外側の x が見える
    }                       // ★ ここで y が破棄される
    // std::cout << y;      // ✗ エラー: y はもういない
    return 0;
}
```

Pythonでは `if` や `for` のブロックが新しいスコープを作りませんが、
C++では **`{ }` ごとにスコープ**ができます。

```python
for i in range(3):
    pass
print(i)      # 2 が出る。ループ変数が残っている
```

```cpp
for (int i = 0; i < 3; ++i) { }
// std::cout << i;   // ✗ エラー。i はループ内だけ
```

この「スコープを抜けたら自動的に破棄される」という性質が、
第11章の **RAII** という強力な仕組みの土台になります。

---

## 4.9 🧪 実践: 分子記述子を格納する構造体

ここまでの型知識を使って、実用的なデータ構造を書いてみます。

```cpp
// code/ch04/descriptor.cpp
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>

// 分子の基本記述子をまとめた構造体
struct Descriptors {
    std::string smiles;
    double      mw            = 0.0;    // 分子量
    double      logp          = 0.0;    // 分配係数
    double      tpsa          = 0.0;    // 極性表面積
    std::int32_t heavy_atoms  = 0;
    std::int32_t rings        = 0;
    std::int32_t hbd          = 0;      // 水素結合ドナー
    std::int32_t hba          = 0;      // 水素結合アクセプタ
    bool        rotatable_ok  = false;
};

// Lipinski の Rule of Five 判定
bool passes_ro5(const Descriptors& d) {
    int violations = 0;
    if (d.mw   > 500.0) ++violations;
    if (d.logp > 5.0)   ++violations;
    if (d.hbd  > 5)     ++violations;
    if (d.hba  > 10)    ++violations;
    return violations <= 1;
}

int main() {
    Descriptors aspirin{
        .smiles = "CC(=O)Oc1ccccc1C(=O)O",
        .mw = 180.16, .logp = 1.19, .tpsa = 63.6,
        .heavy_atoms = 13, .rings = 1, .hbd = 1, .hba = 3
    };

    std::cout << "MW    = " << aspirin.mw << "\n";
    std::cout << "LogP  = " << aspirin.logp << "\n";
    std::cout << "Ro5   = " << (passes_ro5(aspirin) ? "PASS" : "FAIL") << "\n";

    // sizeof で構造体のメモリサイズを確認
    std::cout << "sizeof(Descriptors) = " << sizeof(Descriptors) << " bytes\n";
    return 0;
}
```

ポイント:

- `struct` は「データをまとめた型」。Pythonの `@dataclass` に相当（第11章で詳説）
- `= 0.0` の**デフォルトメンバ初期化子**で、初期化忘れを防いでいる
- `.smiles = ...` は **指示付き初期化**（C++20）。Pythonのキーワード引数風に書ける
- `const Descriptors&` で渡している = コピーせず、変更もしない（第6章）

`sizeof` を出力しているのに注目してください。
`std::string`(32) + `double`×3(24) + `int32`×4(16) + `bool`(1) + パディング = 80 bytes 程度。

> 💡 **メモリレイアウトを意識する**
>
> `bool` を最後に置いたのは意図的です。
> 構造体のメンバは**アライメント**の都合で隙間（パディング）が入ります。
> サイズの大きいメンバから順に並べると、隙間が減ってメモリが節約できます。
>
> 100万分子分保持するなら、80バイト vs 96バイトの差は 16MB になります。
> 第23章で本格的にやります。

---

## 4.9.5 sizeof で確かめる

```cpp
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::cout << sizeof(bool)        << "\n";   // 1
    std::cout << sizeof(char)        << "\n";   // 1
    std::cout << sizeof(int)         << "\n";   // 4
    std::cout << sizeof(long long)   << "\n";   // 8
    std::cout << sizeof(float)       << "\n";   // 4
    std::cout << sizeof(double)      << "\n";   // 8
    std::cout << sizeof(void*)       << "\n";   // 8 (64bit環境)
    std::cout << sizeof(std::size_t) << "\n";   // 8
    std::cout << sizeof(std::string) << "\n";   // 32 (libstdc++)
    std::cout << sizeof(std::vector<int>) << "\n";  // 24
}
```

`sizeof(std::vector<int>)` が **24** なのが重要です。
中に100万要素入っていても、**vector自体は24バイト**です
（データへのポインタ、size、capacity の3つ）。
実データはヒープにあります。第6章と第7章で。

---

## 4.10 この章のまとめ

- C++の変数は「型が決まったメモリ領域」。Pythonの「付箋」とは違う
- **`double` = Pythonの `float`**。`float` は32bit
- **初期化を忘れるとゴミが入る**。`int x{};` のように必ず初期化する
- `{}` 初期化は縮小変換を防いでくれる
- `auto` は型推論。**実行時コストはゼロ**。ただしループでは `auto&` を忘れずに
- `const` は「変更しない」という**コンパイラ検証付きのドキュメント**
- **整数どうしの `/` は整数除算**。Tanimoto計算で事故る第1位
- 符号あり/なしの混在に注意。`size()` は `size_t` を返す
- 浮動小数点の等値比較はしない。NaN は `std::isnan` で
- `{ }` ごとにスコープができ、抜けると変数は破棄される

### Python → C++ 型対応表（保存版）

| Python | C++ | 備考 |
|---|---|---|
| `int` | `int` / `long long` / `std::int64_t` | Pythonは無限精度、C++は固定幅 |
| `float` | **`double`** | 名前に注意 |
| `bool` | `bool` | |
| `str` | `std::string` | 第8章 |
| `bytes` | `std::vector<std::uint8_t>` / `std::string` | |
| `list` | `std::vector<T>` | 型は統一される。第7章 |
| `tuple` | `std::tuple` / `struct` | |
| `dict` | `std::unordered_map<K,V>` | 第9章 |
| `set` | `std::unordered_set<T>` | 第9章 |
| `None` | `std::optional<T>` の空 / `nullptr` | 第17章 |
| `np.int32` | `std::int32_t` | |
| `np.float32` | `float` | |
| `np.ndarray` | `std::vector<T>` + shape、または `std::span` | 第17章 |

> 📝 **練習問題 4-1**
>
> 次のコードの出力を予想してから、実行して確かめてください。
> ```cpp
> int a = 5, b = 2;
> std::cout << a / b << "\n";
> std::cout << a % b << "\n";
> std::cout << 1.0 * a / b << "\n";
> std::cout << static_cast<double>(a / b) << "\n";
> ```

> 📝 **練習問題 4-2**
>
> `int` の最大値 `2147483647` に 1 を足すとどうなりますか?
> ```cpp
> #include <limits>
> int x = std::numeric_limits<int>::max();
> std::cout << x << " " << x + 1 << "\n";
> ```
> （これは**符号あり整数のオーバーフロー = 未定義動作**です。
> `-fsanitize=undefined` を付けてコンパイルすると警告してくれます）

> 📝 **練習問題 4-3**
>
> 4.9 の `Descriptors` 構造体のメンバの順番を入れ替えて、
> `sizeof` がどう変わるか調べてください。
> `bool` を先頭に置くとどうなりますか?

---

→ [第5章 制御構文と関数](ch05-control-flow-and-functions.md)
