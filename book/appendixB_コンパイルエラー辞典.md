# 付録B コンパイルエラー辞典

C++で最初に心が折れるのが、エラーメッセージです。
**呪文に見えますが、パターンは限られています。**

## B.0 エラーを読むための3原則

### 原則1: 一番上のエラーだけ読む

```
error: ...          ← ★ これだけ読む
error: ...             （下は連鎖した派生エラー）
error: ...
note: ...
note: ...
```

**最初のエラーを直すと、下の20個も一緒に消えることがよくあります。**

```bash
g++ -fmax-errors=1 prog.cpp        # GCC: 最初の1個だけ表示
clang++ -ferror-limit=1 prog.cpp   # Clang
```

### 原則2: `error:` と `note:` を区別する

- **`error:`** — 実際の問題
- **`note:`** — 補足情報（候補、インスタンス化の経路）

`note` の中に **`required from here`** があれば、
そこが**あなたのコードの行**です。

### 原則3: Clang を使う

```bash
clang++ -std=c++20 -fsyntax-only prog.cpp
```

**GCCで読めないエラーが、Clangでは3行で分かる**ことがよくあります。
コンパイルせず構文チェックだけなので速いです。

---

## B.1 構文エラー

### `error: expected ';' before ...`

```cpp
int x = 1     // ★ セミコロン忘れ
int y = 2;    // ← エラーはここに出る
```

**エラーの指す行の1つ上を見てください。**

クラス定義の後の `;` 忘れも頻出:

```cpp
class Molecule {
    int x;
}          // ★ ここに ; が必要
```

### `error: expected '}' at end of input`

波括弧の対応が取れていません。エディタの括弧マッチング機能を使ってください。

### `error: 'x' was not declared in this scope`

原因は3つ:

```cpp
// ① スペルミス
std::cout << mesage;      // message のタイポ

// ② #include 忘れ
std::vector<int> v;       // → #include <vector>

// ③ std:: 忘れ
cout << "x";              // → std::cout

// ④ 宣言より前に使っている
int main() { f(); }
void f() { }              // ★ main の後に定義している
```

**対策: 関数はヘッダで宣言するか、`main` より前に定義する。**

### `error: 'vector' does not name a type`

```cpp
vector<int> v;            // ✗
std::vector<int> v;       // ✓
```

または `#include <vector>` 忘れ。

### `error: expected unqualified-id before ...`

予約語を変数名に使っています。

```cpp
int class = 5;        // ✗ class は予約語
int new_ = 5;         // ✗ new も
int template_ = 5;    // ✗
```

### `error: stray '\343' in program`

**全角文字が混入しています。** 全角スペースが典型的な原因です。
エディタで「全角スペースを表示」する設定にしてください。

---

## B.2 型エラー

### `error: no matching function for call to 'f(...)'`

```cpp
void f(int, double);
f(1);              // ★ 引数が足りない
f(1, 2, 3);        // ★ 引数が多い
f("a", "b");       // ★ 型が違う
```

`note: candidate: ...` に候補が並ぶので、
**自分の呼び出しと見比べて**ください。

```
error: no matching function for call to 'tanimoto(std::vector<int>&, Fingerprint&)'
note: candidate: 'double tanimoto(const Fingerprint&, const Fingerprint&)'
note:   no known conversion for argument 1 from 'std::vector<int>' to 'const Fingerprint&'
                                                 ^^^^^^^^^^^^^^^^  ★ ここが原因
```

### `error: invalid conversion from 'X' to 'Y'`

```cpp
int* p = 5;                    // ✗ int を int* に代入できない
std::string s = 42;            // ✗
char c = "abc";                // ✗ const char* を char に
```

### `error: no match for 'operator<<'`

```cpp
struct Mol { int x; };
std::cout << mol;              // ★ Mol に operator<< がない
```

**対策:**

```cpp
std::ostream& operator<<(std::ostream& os, const Mol& m) {
    return os << "Mol(" << m.x << ")";
}
```

### `error: no match for 'operator<'` （sort で出る）

```cpp
std::vector<Mol> v;
std::sort(v.begin(), v.end());   // ★ Mol に operator< がない
```

**対策:**

```cpp
// 方法1: 比較関数を渡す
std::sort(v.begin(), v.end(),
          [](const Mol& a, const Mol& b){ return a.mw < b.mw; });

// 方法2: operator<=> を定義（C++20）
struct Mol {
    double mw;
    auto operator<=>(const Mol&) const = default;
};
```

### `error: passing 'const X' as 'this' argument discards qualifiers`

**`const` メンバ関数から非 `const` メンバ関数を呼んでいます。**

```cpp
class Molecule {
    int compute();                       // ★ const がない
public:
    int get() const { return compute(); } // ✗ エラー
};
```

**対策: `compute()` に `const` を付ける。**

```cpp
int compute() const;
```

これが第11章で言った「**const 汚染**」です。
下位の関数から順に `const` を付けていく必要があります。

### `error: cannot bind non-const lvalue reference to an rvalue`

```cpp
void f(std::string& s);
f("hello");           // ✗ 一時オブジェクトは非constの参照に束縛できない
f(std::string("x"));  // ✗ 同じ
```

**対策: `const std::string&` にする。**

```cpp
void f(const std::string& s);   // ✓ 一時オブジェクトもOK
```

### `error: assignment of read-only variable`

```cpp
const int x = 5;
x = 10;               // ✗
```

または `const` メンバ関数の中でメンバを変更しています。

### `error: use of deleted function`

```cpp
std::unique_ptr<T> a, b;
b = a;                          // ★ コピー代入が delete されている
b = std::move(a);               // ✓

std::vector<std::unique_ptr<T>> v1, v2;
v2 = v1;                        // ★ 要素がコピー不可なので vector もコピー不可
```

`note: declared here` を見ると、どの関数が delete されているか分かります。

### `error: 'X' is not a class, struct, or union type`

テンプレート引数の書き間違いが多いです。

```cpp
std::vector v;              // ✗ 型引数がない（C++17のCTADが効かない文脈）
std::vector<int> v;         // ✓
```

---

## B.3 リンクエラー

**「コンパイルは通ったのにビルドが失敗する」**のがリンクエラーです。

### `undefined reference to 'f(int)'`

**宣言はあるが、実装が見つかりません。**

原因:

```cpp
// ① .cpp をコンパイル対象に入れ忘れ
g++ main.cpp -o prog                  // ✗ molecule.cpp がない
g++ main.cpp molecule.cpp -o prog     // ✓

// ② 宣言と定義のシグネチャが違う
// molecule.hpp
int count(const std::string& s);
// molecule.cpp
int count(std::string s) { ... }      // ★ const& がない = 別の関数

// ③ 関数を宣言しただけで実装を書いていない

// ④ ライブラリのリンク忘れ
g++ prog.cpp -o prog                  // ✗
g++ prog.cpp -lchemcpp -o prog        // ✓

// ⑤ テンプレートの実装を .cpp に書いた（第15章）
```

**マングリングされた名前を読む:**

```bash
$ g++ main.o molecule.o -o prog
undefined reference to `_ZN8Molecule5parseERKNSt7__cxx1112basic_stringIcEE'

$ echo '_ZN8Molecule5parseERKNSt7__cxx1112basic_stringIcEE' | c++filt
Molecule::parse(std::__cxx11::basic_string<char> const&)
```

**`c++filt` を覚えてください。** 呪文が人間語になります。

### `undefined reference to 'vtable for X'`

**仮想関数を宣言したが、実装していません。**

```cpp
class Base {
public:
    virtual ~Base();          // ★ 宣言だけ。実装がない
    virtual void f();         // ★ 同じ
};
```

**対策:**

```cpp
virtual ~Base() = default;    // または .cpp に実装を書く
virtual void f() = 0;         // 純粋仮想にする
```

> 💡 **経験則: 最初の非インライン仮想関数がある .cpp に vtable が置かれます。**
> 全部インラインにするか、1つは .cpp に実装を置いてください。

### `undefined reference to 'main'`

`main` 関数がありません。あるいは:

```cpp
int Main() { }        // ★ 大文字
void main() { }       // ★ 戻り値が int でない（一部処理系では通る）
```

### `multiple definition of 'x'`

**同じものが2回定義されています。**

```cpp
// ヘッダに書いてはいけないもの
// header.hpp
int global_count = 0;             // ★ include した数だけ定義される

// 対策
inline int global_count = 0;      // ✓ C++17
extern int global_count;          // ✓ 宣言だけ（定義は .cpp に）
```

関数も同様:

```cpp
// header.hpp
int f() { return 1; }             // ★ multiple definition
inline int f() { return 1; }      // ✓
```

### `undefined symbol` （実行時、.so をロードするとき）

共有ライブラリのリンク漏れです。

```bash
ldd libmymodule.so          # 依存ライブラリを確認
nm -D --undefined-only libmymodule.so | c++filt   # 未解決シンボルを見る
```

pybind11 でよく出ます（第26章）。

---

## B.4 テンプレートのエラー

### 400行のエラーが出た

**読み方:**

1. `required from here` を探す → **あなたのコードの行**
2. その直前の `error:` を読む
3. `no match for 'operator...'` などの核心を探す

```
In file included from /usr/include/c++/13/algorithm:61,
                 from main.cpp:2:
/usr/include/c++/13/bits/stl_algo.h:1867:5: error: no match for
  'operator<' (operand types are 'Mol' and 'Mol')
 1867 |     if (__comp(__a, __b))
      |     ^~~~~~~~~~~~~~~~~~~~
main.cpp:15:5: required from here            ← ★ あなたのコード
```

**答え: `Mol` に `operator<` がない。**

### concept で防ぐ（C++20）

```cpp
// Before: 400行のエラー
template <typename T>
void sort_all(std::vector<T>& v) { std::sort(v.begin(), v.end()); }

// After: 3行のエラー
template <std::totally_ordered T>
void sort_all(std::vector<T>& v) { std::sort(v.begin(), v.end()); }
```

```
error: constraints not satisfied
note: the required expression '(a < b)' is invalid
```

**concept を使うと、エラーメッセージが劇的に改善します。**

### `error: 'X' was not declared in this scope` （テンプレート内）

**依存名の問題**です。

```cpp
template <typename T>
void f() {
    T::value_type x;              // ✗ value_type が型か値か分からない
    typename T::value_type x;     // ✓ typename を付ける
}

template <typename T>
void g(T t) {
    t.template method<int>();     // ✓ テンプレートメンバには template が必要
}
```

**ルール: テンプレート引数に依存する型名には `typename` を付ける。**

### `error: no type named 'type' in 'struct std::enable_if<false>'`

**SFINAE の失敗**です。テンプレートの制約を満たしていません。
古いコードでよく見ます。concept に書き換えられるサインです。

---

## B.5 実行時エラー

### Segmentation fault (core dumped)

**メモリの不正アクセス。** 原因:

| 原因 | 例 |
|---|---|
| nullptr のデリファレンス | `T* p = nullptr; p->f();` |
| 配列の範囲外 | `v[1000]`（サイズ10） |
| 解放後の使用 | `delete p; *p = 1;` |
| ダングリング参照 | ローカル変数への参照を返した |
| スタックオーバーフロー | 無限再帰、巨大なローカル配列 |
| イテレータの無効化 | `push_back` 後に古いイテレータを使用 |

**対策: ASan（第29章）**

```bash
g++ -g -fsanitize=address -D_GLIBCXX_ASSERTIONS prog.cpp -o prog
./prog
```

**行番号まで教えてくれます。**

### `terminate called after throwing an instance of 'std::bad_alloc'`

メモリ確保に失敗しました。

```cpp
std::vector<int> v(1000000000000);        // 巨大すぎる

int n = -1;
std::vector<int> w(n);                    // ★ size_t に変換されて巨大な値に
```

**符号の問題**が原因のことが多いです。

### `terminate called after throwing an instance of 'std::out_of_range'`

```cpp
v.at(100);                    // 範囲外
m.at("missing_key");          // 存在しないキー
```

`what():` にメッセージが出るので読んでください。

### `terminate called without an active exception`

**`std::thread` を join せずに破棄しました**（第25章）。

```cpp
{
    std::thread t(f);
}   // ★ join も detach もしていない

// 対策
std::jthread t(f);            // ✓ C++20。自動 join
```

### `pure virtual method called`

**コンストラクタ/デストラクタの中で仮想関数を呼んでいます。**

```cpp
class Base {
public:
    Base() { init(); }                // ★ 危険
    virtual void init() = 0;
};
```

コンストラクタ実行中は、派生クラスがまだ構築されていません。

### 結果が毎回違う / たまに落ちる

| 原因 | 対策 |
|---|---|
| 未初期化変数 | `-Wall`、`int x{};` で初期化 |
| データ競合 | TSan（第29章） |
| ダングリング参照 | ASan |
| ハッシュマップの反復順序に依存 | `std::map` を使うか、明示的にソート |

### Debug では動くが Release で落ちる

**ほぼ確実に未定義動作です。**

```bash
g++ -O2 -fsanitize=undefined prog.cpp -o prog
```

典型例:
- 符号付き整数のオーバーフロー
- 未初期化変数（Debugでは偶然0だった）
- 配列の範囲外（Debugでは偶然無害な場所だった）
- strict aliasing 違反

---

## B.6 数値の異常

### 結果が0になる

```cpp
int inter = 12, uni = 40;
double t = inter / uni;        // ★ 整数除算 → 0
double t = 1.0 * inter / uni;  // ✓ 0.3
```

**Tanimoto計算で最頻出のバグです**（第4章）。

### 結果が NaN

```cpp
double x = 0.0 / 0.0;          // NaN
double y = std::sqrt(-1.0);    // NaN
double z = std::log(0.0);      // -inf
```

```cpp
if (std::isnan(x)) { }         // ✓
if (x != x) { }                // ✓（トリッキーだが有効）
if (x == NAN) { }              // ✗ 常に false
```

### 結果が inf

ゼロ除算、オーバーフロー。

```cpp
if (denom != 0.0) result = num / denom;   // ゼロチェック
if (std::isfinite(result)) { }             // 結果チェック
```

### 期待より小さい/大きい値

```cpp
// ① int のオーバーフロー
int n = 100000;
int sq = n * n;                // ★ 10^10 は int の範囲外
long long sq = 1LL * n * n;    // ✓

// ② 符号なしの減算
std::size_t a = 3, b = 5;
auto d = a - b;                // ★ 巨大な値になる
auto d = static_cast<long long>(a) - b;   // ✓

// ③ shift のオーバーフロー
std::uint64_t mask = 1 << 40;      // ★ int は32bit
std::uint64_t mask = 1ULL << 40;   // ✓
```

---

## B.7 パフォーマンスの問題

### C++にしたのに遅い

チェックリスト:

```
□ -O2 または -O3 でビルドしたか?（-O0 は10倍遅い）
□ CMAKE_BUILD_TYPE を指定したか?
□ ループで const auto& を使っているか?（auto はコピー）
□ 引数を const T& で受けているか?
□ push_back の前に reserve したか?
□ ループの中でヒープ確保していないか?
□ std::endl を使っていないか?
□ vector<vector<T>> を使っていないか?
□ サニタイザを有効にしたままでないか?（2倍遅い）
```

### 徐々に遅くなる

- vector の再確保が繰り返されている
- メモリリークでスワップが発生
- キャッシュ（`unordered_map`）が無制限に成長

### 並列化しても速くならない

- ロック競合（ループ内でロックしている）
- false sharing（隣接メモリへの書き込み）
- メモリ帯域の飽和（memory bound）
- タスクの粒度が細かすぎる

---

## B.8 環境・ビルド関連

### `fatal error: X.h: No such file or directory`

インクルードパスが通っていません。

```bash
g++ -I/path/to/include prog.cpp
```

```cmake
target_include_directories(myapp PRIVATE /path/to/include)
```

### `error: 'std::format' is not a member of 'std'`

コンパイラがC++20の `<format>` に未対応です。

- GCC 13+ / Clang 17+ / MSVC 19.29+ が必要
- 代替: [fmt](https://github.com/fmtlib/fmt) ライブラリ

### `error: 'popcount' is not a member of 'std'`

`#include <bit>` 忘れ、または C++20 未対応。

```cpp
#if __cpp_lib_bitops >= 201907L
    int n = std::popcount(x);
#else
    int n = __builtin_popcountll(x);
#endif
```

### Windowsで「〜.dll が見つかりません」

MSYS2でビルドした実行ファイルが、MSYS2のDLLを見つけられません。

```bash
# 対策1: PATH を通す
export PATH=/c/msys64/ucrt64/bin:$PATH

# 対策2: 静的リンク
g++ -static -static-libgcc -static-libstdc++ prog.cpp -o prog.exe
```

### CMake: `Could NOT find X`

```bash
cmake -DCMAKE_PREFIX_PATH=/path/to/lib -S . -B build
# conda 環境なら
cmake -DCMAKE_PREFIX_PATH=$CONDA_PREFIX -S . -B build
```

### 変更が反映されない

```bash
rm -rf build
cmake -S . -B build ...
```

CMakeのキャッシュが古い可能性があります。

---

## B.9 エラーメッセージ検索のコツ

### ① 固有名を消す

```
error: no matching function for call to 'MyClass::process(std::vector<Foo>&)'
                                          ^^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^
```

固有の名前を消して、**一般的な部分だけで検索**します:

```
"no matching function for call to" c++ vector
```

### ② コンパイラを変えてみる

```bash
clang++ -fsyntax-only prog.cpp     # Clang の方が親切
```

### ③ 最小再現コードを作る

エラーが出る部分だけを抜き出した10行のコードを作ると、
**その過程で自分で原因に気づく**ことがよくあります。

### ④ godbolt.org で試す

複数のコンパイラで同時にコンパイルして、エラーを比較できます。

---

## B.10 エラー別クイックリファレンス

| エラーメッセージの一部 | 原因 | 対策 |
|---|---|---|
| `expected ';'` | セミコロン忘れ | 1つ上の行を見る |
| `was not declared in this scope` | 宣言忘れ / include忘れ / std:: 忘れ | 宣言を追加 |
| `no matching function for call` | 引数の型/個数が違う | candidate を見比べる |
| `discards qualifiers` | const メンバ関数の問題 | 呼ぶ側の関数に const を追加 |
| `cannot bind non-const lvalue` | 一時オブジェクトを `T&` に | `const T&` にする |
| `use of deleted function` | コピー不可な型をコピー | `std::move` を使う |
| `undefined reference` | 実装がない / リンク漏れ | .cpp を追加、ライブラリをリンク |
| `undefined reference to vtable` | 仮想関数の実装がない | `= default` か実装を書く |
| `multiple definition` | ヘッダに定義を書いた | `inline` を付ける |
| `no match for 'operator<'` | 比較演算子がない | 比較関数を渡す / `<=>` を定義 |
| `Segmentation fault` | メモリの不正アクセス | ASan で調べる |
| `std::bad_alloc` | メモリ不足 / 巨大なサイズ | 符号を確認 |
| `terminate without active exception` | thread の join 忘れ | `jthread` を使う |
| `stray '\343'` | 全角文字 | 全角スペースを探す |
| 結果が 0 | 整数除算 | `1.0 *` を付ける |
| 結果が NaN | 0/0, sqrt(負) | `isnan` でチェック |
| Debug OK / Release NG | 未定義動作 | UBSan で調べる |
| C++にしたのに遅い | `-O0` でビルド | `-O2` を付ける |

---

→ [付録C 用語集](appendixC_用語集.md)
