# 第18章 エラー処理

Pythonでは何でも例外で済ませますが、C++には複数の選択肢があります。
使い分けができると、堅牢で速いコードが書けます。

## 18.1 エラーの4分類

まず、「エラー」を分類することから始めましょう。

| 種類 | 例 | 対処法 |
|---|---|---|
| **① プログラマのバグ** | 配列の範囲外、nullptr参照、不変条件違反 | `assert` で開発中に検出。本番では起きてはいけない |
| **② 想定内の失敗** | パースエラー、ファイルが無い、キーが無い | `optional` / `expected` で戻り値に含める |
| **③ 想定外の実行時エラー** | メモリ不足、ディスク障害 | 例外 |
| **④ 回復不能** | データ破損、内部矛盾 | ログを出して `std::abort()` |

**Pythonは①〜④を全部例外にしますが、C++では使い分けます。**

---

## 18.2 assert — バグの検出

```cpp
#include <cassert>

double tanimoto(const Fingerprint& a, const Fingerprint& b) {
    assert(a.size() == b.size() && "fingerprints must have the same size");
    // ...
}
```

`assert` は、条件が偽なら**プログラムを即座に終了**します。

```
Assertion failed: a.size() == b.size() && "fingerprints must have the same size",
file tanimoto.cpp, line 42
```

> 💡 **`&& "メッセージ"` のトリック**
> `assert(cond && "message")` と書くと、失敗時にメッセージも表示されます。
> 文字列リテラルは常に真なので、条件の意味は変わりません。

### NDEBUG で消える

```bash
g++ -DNDEBUG -O2 prog.cpp    # assert が全部消える（実行時コストゼロ）
```

`CMAKE_BUILD_TYPE=Release` では自動的に `-DNDEBUG` が付きます。

> ⚠️ **assert の中に副作用を書かないでください。**
> ```cpp
> assert(compute_and_update() > 0);   // ★ Release ビルドで実行されない！
> ```

### assert を使うべき場面

```cpp
// ✓ 事前条件（呼び出し側が守るべき約束）
void set_atom(std::size_t i, Atom a) {
    assert(i < atoms_.size());
    atoms_[i] = a;
}

// ✓ 事後条件（この関数が保証すること）
double normalize(double x) {
    double r = x / total_;
    assert(r >= 0.0 && r <= 1.0);
    return r;
}

// ✓ 不変条件（クラスが常に満たすべき性質）
void check_invariant() const {
    assert(bonds_.size() <= atoms_.size() * (atoms_.size() - 1) / 2);
}

// ✗ ユーザー入力の検証には使わない（Release で消えるので）
assert(user_input_is_valid);   // ★ ダメ。例外か optional を使う
```

### static_assert — コンパイル時の検証

```cpp
static_assert(sizeof(Fingerprint) == 256, "Fingerprint must be 256 bytes");
static_assert(std::is_trivially_copyable_v<Atom>, "Atom must be POD for binary I/O");

template <std::size_t N>
class FixedFP {
    static_assert(N % 64 == 0, "N must be a multiple of 64");
};
```

**コンパイル時にチェックされるので、実行時コストはゼロ**です。
バイナリファイル形式の互換性チェックなどに有用です。

---

## 18.3 例外

```cpp
#include <stdexcept>

Molecule parse_smiles(std::string_view s) {
    if (s.empty()) {
        throw std::invalid_argument("empty SMILES string");
    }
    if (!brackets_balanced(s)) {
        throw std::runtime_error(std::format("unbalanced brackets in: {}", s));
    }
    // ...
}

try {
    auto mol = parse_smiles(input);
    process(mol);
} catch (const std::invalid_argument& e) {
    std::cerr << "bad input: " << e.what() << "\n";
} catch (const std::exception& e) {          // ★ 基底クラスで受ける
    std::cerr << "error: " << e.what() << "\n";
} catch (...) {                              // 何でも受ける（最後の砦）
    std::cerr << "unknown error\n";
}
```

```python
try:
    mol = parse_smiles(input)
except ValueError as e:
    print(f"bad input: {e}")
except Exception as e:
    print(f"error: {e}")
```

### 標準例外の階層

```
std::exception
├── std::logic_error              プログラムの論理エラー（バグ）
│   ├── std::invalid_argument     不正な引数
│   ├── std::domain_error         定義域外
│   ├── std::length_error         長さの制限超過
│   └── std::out_of_range         範囲外（vector::at が投げる）
├── std::runtime_error            実行時にしか分からないエラー
│   ├── std::range_error
│   ├── std::overflow_error
│   ├── std::underflow_error
│   └── std::system_error         OS のエラー（ファイルI/Oなど）
├── std::bad_alloc                メモリ確保失敗（new が投げる）
├── std::bad_cast                 dynamic_cast の失敗
└── std::bad_optional_access      optional::value() の失敗
```

> 💡 **`catch (const std::exception& e)` で受ければ、ほぼ全部拾えます。**
> **必ず `const 参照`で受けてください**（値で受けるとスライシングが起きます）。

### 自作の例外

```cpp
class SmilesParseError : public std::runtime_error {
    std::string smiles_;
    std::size_t position_;
public:
    SmilesParseError(std::string smiles, std::size_t pos, const std::string& msg)
        : std::runtime_error(std::format("{} at position {} in '{}'", msg, pos, smiles))
        , smiles_(std::move(smiles)), position_(pos) {}

    const std::string& smiles() const noexcept { return smiles_; }
    std::size_t position() const noexcept { return position_; }
};

throw SmilesParseError(std::string(s), i, "unexpected character");
```

---

## 18.4 ★例外安全性とRAII★

**例外が飛んでも、リソースが漏れないことが重要です。**

```cpp
// ✗ 例外安全でない
void bad() {
    Molecule* m = new Molecule("CCO");
    process(m);        // ★ ここで例外が飛ぶと delete されない → リーク
    delete m;
}

// ✓ RAII で安全
void good() {
    auto m = std::make_unique<Molecule>("CCO");
    process(*m);       // 例外が飛んでも、unique_ptr のデストラクタが解放する
}
```

**第11章のRAIIが、そのまま例外安全性の答えになっています。**
これがC++の設計の美しいところです。

### 例外安全性の3レベル

| 保証 | 意味 |
|---|---|
| **基本保証** | 例外が飛んでもリークせず、オブジェクトは有効な状態のまま（値は不定） |
| **強い保証** | 例外が飛んだら、呼び出し前の状態に完全に戻る（トランザクション的） |
| **無例外保証** | 例外を投げない（`noexcept`） |

**最低でも基本保証**を満たすように書いてください。RAIIを使えば自動的に満たせます。

強い保証が欲しいときの定石は **copy-and-swap**:

```cpp
void Molecule::add_atoms(const std::vector<Atom>& new_atoms) {
    auto tmp = atoms_;                        // コピー
    tmp.insert(tmp.end(), new_atoms.begin(), new_atoms.end());   // ここで例外が飛んでも
    atoms_.swap(tmp);                         // ★ swap は noexcept。ここまで来れば確実
}
```

### noexcept

```cpp
std::size_t num_atoms() const noexcept { return atoms_.size(); }
```

「この関数は例外を投げない」という宣言です。

**効果:**
1. コンパイラが最適化できる（例外処理のコードを生成しない）
2. `std::vector` がムーブを選択する（第12章。**これが最重要**）
3. ドキュメントになる

> ⚠️ **`noexcept` 宣言した関数から例外が飛ぶと、`std::terminate` が呼ばれます**
> （プログラム即死。catch できません）。確実な場合だけ付けてください。

**付けるべき関数:**
- **ムーブコンストラクタ、ムーブ代入** ← 最重要
- `swap`
- デストラクタ（デフォルトで `noexcept`）
- 単純な getter
- 数値計算だけの関数

---

## 18.5 ⚠️ 例外の落とし穴

### ① デストラクタから例外を投げてはいけない

```cpp
class Bad {
public:
    ~Bad() { throw std::runtime_error("oops"); }   // ★ 絶対にダメ
};
```

デストラクタは**暗黙的に `noexcept`** です。例外が飛ぶと `std::terminate` します。
さらに、既に例外が飛んでいる最中にもう1つ例外が飛ぶと、確実にプログラムが死にます。

```cpp
~FileWriter() {
    try {
        flush();
    } catch (...) {
        // ログを出すだけ。再スローしない
    }
}
```

### ② コストの誤解

**例外は「投げなければ」ほぼゼロコスト**です（ゼロコスト例外モデル）。
`try` ブロックを書いても、例外が飛ばない限り実行時コストはありません。

しかし、**投げるときは非常に遅い**です（スタック巻き戻し、型情報の検索で
マイクロ秒オーダー）。

> ⚠️ **ループの中で例外を制御フローに使わないでください。**
> ```cpp
> // ✗ 100万分子のパースで、10万個失敗する → 10万回の例外 → 激遅
> for (const auto& s : smiles_list) {
>     try { mols.push_back(parse_smiles(s)); }
>     catch (const SmilesParseError&) { ++failures; }
> }
>
> // ✓ optional / expected で返す
> for (const auto& s : smiles_list) {
>     if (auto m = try_parse_smiles(s)) mols.push_back(std::move(*m));
>     else ++failures;
> }
> ```
>
> 🧪 これは実務で本当に効きます。ChEMBLのような実データには
> パースできないSMILESが数％含まれており、例外だと数十秒余分にかかります。

### ③ 例外を無効化している環境がある

組込み、ゲーム、一部のライブラリは `-fno-exceptions` でビルドされます。
そういう環境では例外を使えません。

---

## 18.6 ★std::expected★（C++23）— 失敗の理由も返す

`optional` は「値が無い」ことしか表せませんが、
`expected` は**「なぜ失敗したか」も返せます**。

```cpp
#include <expected>

enum class ParseError {
    EmptyInput, UnbalancedBrackets, UnknownAtom, InvalidRingClosure
};

std::expected<Molecule, ParseError> parse_smiles(std::string_view s) {
    if (s.empty())                return std::unexpected(ParseError::EmptyInput);
    if (!brackets_balanced(s))    return std::unexpected(ParseError::UnbalancedBrackets);
    return Molecule(s);           // 成功
}

auto result = parse_smiles("CC(=O");
if (result) {
    process(*result);
} else {
    switch (result.error()) {
        case ParseError::UnbalancedBrackets:
            std::cerr << "brackets don't match\n"; break;
        // ...
    }
}
```

### API

```cpp
std::expected<T, E> r = ...;

r.has_value();       // 成功したか
*r / r.value();      // 値（失敗時に value() は例外）
r.error();           // エラー
r.value_or(fallback);

// モナド操作
r.and_then(f);       // 成功なら f を適用（f も expected を返す）
r.transform(f);      // 成功なら値を変換
r.or_else(f);        // 失敗なら f を適用
r.transform_error(f);// エラーを変換
```

### C++20 以前の代替

`std::expected` が使えない環境では:

```cpp
// ① optional + 別途エラーを渡す
std::optional<Molecule> parse_smiles(std::string_view s, ParseError* err = nullptr);

// ② variant
std::variant<Molecule, ParseError> parse_smiles(std::string_view s);

// ③ 構造体で返す
struct ParseResult {
    std::optional<Molecule> mol;
    ParseError              error = ParseError::None;
    explicit operator bool() const { return mol.has_value(); }
};

// ④ tl::expected（ヘッダオンリーの外部ライブラリ。std::expected とほぼ同一API）
#include <tl/expected.hpp>
```

> 💡 **`tl::expected` は実用的な選択肢**です。1ヘッダだけなので導入が楽です。
> https://github.com/TartanLlama/expected

---

## 18.7 エラーコード（C言語スタイル）

```cpp
enum class ErrorCode { Ok, FileNotFound, ParseError, OutOfMemory };

ErrorCode load_sdf(const std::string& path, std::vector<Molecule>& out);
```

古典的ですが、**C APIとの境界では今でも必要**です。
`std::error_code` という標準の仕組みもあります。

```cpp
#include <system_error>

std::error_code ec;
auto size = std::filesystem::file_size(path, ec);   // 例外を投げない版
if (ec) {
    std::cerr << ec.message() << "\n";
}
```

`std::filesystem` の関数は、多くが「例外版」と「`error_code` 版」の
両方を提供しています。

---

## 18.8 使い分けの指針

```
   エラーが起きた
        │
   プログラムのバグ?（起きてはいけない）
        │
   ┌────┴────┐
  Yes        No
   │          │
 assert    想定内の失敗?（呼び出し側が対処すべき）
           （パース失敗、not found）
                │
         ┌──────┴──────┐
        Yes            No（メモリ不足、ディスク障害）
         │              │
   失敗の理由が       例外を投げる
   1つだけ?
         │
   ┌─────┴─────┐
  Yes          No
   │            │
 optional    expected
```

### 実務での推奨

| 場面 | 方法 |
|---|---|
| SMILESのパース失敗 | **`optional` / `expected`**（頻繁に起きる） |
| ファイルが開けない | `expected` または例外 |
| メモリ不足 | 例外（`std::bad_alloc` が自動で飛ぶ） |
| 配列の範囲外アクセス | `assert`（バグ） |
| 引数の nullptr | `assert`（バグ） |
| ユーザー設定の値が不正 | 例外（起動時に1回だけなので） |
| ホットループ内の失敗 | **`optional` 一択**（例外は遅すぎる） |
| ライブラリの公開API | 例外 + `noexcept` 版の両方を提供するのが親切 |

---

## 18.9 🧪 実践: 堅牢なSMILESパーサのエラー処理

```cpp
// code/ch18/robust_parse.cpp
#include <cassert>
#include <format>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// ---- エラー型 ----
struct ParseError {
    enum class Kind {
        EmptyInput, UnbalancedParen, UnbalancedBracket,
        UnknownAtom, UnclosedRing, InvalidCharacter
    };
    Kind        kind;
    std::size_t position = 0;
    std::string detail;

    std::string message() const {
        std::string_view k;
        switch (kind) {
            case Kind::EmptyInput:        k = "empty input"; break;
            case Kind::UnbalancedParen:   k = "unbalanced parenthesis"; break;
            case Kind::UnbalancedBracket: k = "unbalanced bracket"; break;
            case Kind::UnknownAtom:       k = "unknown atom"; break;
            case Kind::UnclosedRing:      k = "unclosed ring bond"; break;
            case Kind::InvalidCharacter:  k = "invalid character"; break;
        }
        return std::format("{} at position {}{}", k, position,
                           detail.empty() ? "" : " (" + detail + ")");
    }
};

// ---- 簡易 expected（C++23 が使えない環境向け）----
template <typename T, typename E>
class Result {
    std::variant<T, E> data_;
public:
    Result(T v)  : data_(std::move(v)) {}
    Result(E e)  : data_(std::move(e)) {}

    bool has_value() const { return data_.index() == 0; }
    explicit operator bool() const { return has_value(); }

    const T& value() const { return std::get<0>(data_); }
    T&       value()       { return std::get<0>(data_); }
    const T& operator*() const { return value(); }
    T&       operator*()       { return value(); }

    const E& error() const { return std::get<1>(data_); }

    template <typename U>
    T value_or(U&& fallback) const {
        return has_value() ? value() : static_cast<T>(std::forward<U>(fallback));
    }
};

// ---- 簡易分子 ----
struct SimpleMol {
    std::vector<char> atoms;
    int               ring_bonds = 0;
    int               branches   = 0;
};

using ParseResult = Result<SimpleMol, ParseError>;

// ---- パーサ本体（例外を投げない）----
ParseResult parse_smiles(std::string_view s) {
    if (s.empty()) {
        return ParseError{ParseError::Kind::EmptyInput, 0, ""};
    }

    SimpleMol mol;
    int paren_depth = 0;
    bool in_bracket = false;
    std::vector<int> open_rings(10, 0);

    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];

        if (c == '[') {
            if (in_bracket)
                return ParseError{ParseError::Kind::UnbalancedBracket, i, "nested '['"};
            in_bracket = true;
            continue;
        }
        if (c == ']') {
            if (!in_bracket)
                return ParseError{ParseError::Kind::UnbalancedBracket, i, "unexpected ']'"};
            in_bracket = false;
            mol.atoms.push_back('*');
            continue;
        }
        if (in_bracket) continue;

        if (c == '(') { ++paren_depth; ++mol.branches; continue; }
        if (c == ')') {
            if (--paren_depth < 0)
                return ParseError{ParseError::Kind::UnbalancedParen, i, "extra ')'"};
            continue;
        }

        if (c >= '1' && c <= '9') {
            int r = c - '0';
            open_rings[r] ^= 1;                  // トグル
            if (open_rings[r] == 0) ++mol.ring_bonds;
            continue;
        }

        if (std::string_view("BCNOPSFIHbcnops").find(c) != std::string_view::npos) {
            mol.atoms.push_back(c);
            continue;
        }
        if (std::string_view("-=#$:/\\.@+").find(c) != std::string_view::npos) {
            continue;   // 結合・立体・電荷は今は無視
        }

        return ParseError{ParseError::Kind::InvalidCharacter, i,
                          std::string(1, c)};
    }

    if (paren_depth != 0)
        return ParseError{ParseError::Kind::UnbalancedParen, s.size(),
                          std::format("{} unclosed", paren_depth)};
    if (in_bracket)
        return ParseError{ParseError::Kind::UnbalancedBracket, s.size(), "unclosed '['"};

    for (int r = 1; r <= 9; ++r) {
        if (open_rings[r])
            return ParseError{ParseError::Kind::UnclosedRing, s.size(),
                              std::format("ring {}", r)};
    }

    return mol;
}

// ---- 一括処理（例外を使わないので速い）----
struct BatchResult {
    std::vector<SimpleMol>                      molecules;
    std::vector<std::pair<std::string, ParseError>> failures;
};

BatchResult parse_batch(const std::vector<std::string>& smiles_list) {
    BatchResult out;
    out.molecules.reserve(smiles_list.size());

    for (const auto& s : smiles_list) {
        auto r = parse_smiles(s);
        if (r) out.molecules.push_back(std::move(*r));
        else   out.failures.emplace_back(s, r.error());
    }
    return out;
}

int main() {
    std::vector<std::string> inputs = {
        "CCO",
        "CC(=O)Oc1ccccc1C(=O)O",
        "",                              // 空
        "CC(=O",                         // 括弧が閉じていない
        "CCO)",                          // 余分な閉じ括弧
        "c1ccccc",                       // 環が閉じていない
        "CC[NH3+]CC",                    // ブラケット原子
        "CCXO",                          // 未知の文字
        "Cn1cnc2c1c(=O)n(C)c(=O)n2C",
    };

    auto result = parse_batch(inputs);

    std::cout << std::format("parsed : {}\n", result.molecules.size());
    std::cout << std::format("failed : {}\n\n", result.failures.size());

    for (const auto& [smi, err] : result.failures) {
        std::cout << std::format("  '{}': {}\n", smi, err.message());
    }

    std::cout << "\nsuccessful molecules:\n";
    for (const auto& m : result.molecules) {
        std::cout << std::format("  atoms={} rings={} branches={}\n",
                                 m.atoms.size(), m.ring_bonds, m.branches);
    }
    return 0;
}
```

出力:

```
parsed : 5
failed : 4

  '': empty input at position 0
  'CC(=O': unbalanced parenthesis at position 5 (1 unclosed)
  'CCO)': unbalanced parenthesis at position 3 (extra ')')
  'c1ccccc': unclosed ring bond at position 7 (ring 1)
  ...
```

**このコードの良いところ:**
- **例外を一切使わない**ので、100万件のパースでも高速
- エラーの**位置と理由**が分かるので、デバッグしやすい
- 失敗した入力を集めて後で分析できる
- `Result` 型により、成功/失敗の扱い忘れが起きにくい

---

## 18.10 デバッグ用のログとアサーション

```cpp
// code/ch18/debug_util.hpp
#pragma once
#include <format>
#include <iostream>
#include <source_location>          // C++20

// 呼び出し元の情報を自動取得する
inline void log_debug(std::string_view msg,
                      const std::source_location loc = std::source_location::current()) {
    std::cerr << std::format("[{}:{}] {}: {}\n",
                             loc.file_name(), loc.line(), loc.function_name(), msg);
}

// 独自アサーション（Release でも残したい検証用）
#define CHECK(cond, msg)                                                \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::cerr << std::format("CHECK FAILED: {} ({}:{})\n",      \
                                     msg, __FILE__, __LINE__);          \
            std::abort();                                               \
        }                                                               \
    } while (0)
```

`std::source_location`（C++20）は、`__FILE__` / `__LINE__` マクロの
型安全な代替です。Pythonの `inspect.stack()` に近い機能です。

---

## 18.11 この章のまとめ

- エラーを4分類する: **バグ / 想定内の失敗 / 想定外の実行時エラー / 回復不能**
- **`assert`** はバグの検出用。Release では消える。副作用を書かない
- **`static_assert`** はコンパイル時検証。コストゼロ
- **例外**は「投げなければタダ、投げると遅い」
  → **ホットループの制御フローに使わない**
- **例外安全性はRAIIで自動的に達成される**
- **ムーブコンストラクタには `noexcept`**（vector が数十倍速くなる）
- **デストラクタから例外を投げない**
- **想定内の失敗は `optional` / `expected`** で返す
- `catch (const std::exception& e)` で受ける（**必ず const 参照**）

### エラー処理チートシート

```cpp
assert(i < size);                                 // バグの検出（Debugのみ）
static_assert(sizeof(T) == 256);                  // コンパイル時検証
throw std::runtime_error("...");                  // 想定外のエラー
std::optional<T> f();                             // 失敗するかも（理由不要）
std::expected<T, E> f();                          // 失敗するかも（理由あり）
void f() noexcept;                                // 絶対に失敗しない
```

> 📝 **練習問題 18-1**
>
> `parse_smiles` を、例外を投げる版と `Result` を返す版の両方で実装し、
> 10万件（うち5%が不正）のパース時間を比較してください。

> 📝 **練習問題 18-2**
>
> 次のコードのバグを3つ見つけてください。
> ```cpp
> class Reader {
>     FILE* fp_;
> public:
>     Reader(const char* p) { fp_ = fopen(p, "r"); }
>     ~Reader() { if (!fclose(fp_)) throw std::runtime_error("close failed"); }
>     std::string read_all() {
>         char* buf = new char[1024];
>         fread(buf, 1, 1024, fp_);
>         std::string s(buf);
>         delete[] buf;
>         return s;
>     }
> };
> ```

> 📝 **練習問題 18-3**
>
> `std::vector` の要素を安全に取得する
> `template <typename T> std::optional<T> try_at(const std::vector<T>& v, std::size_t i)`
> を書いてください。

---

**第II部はこれで終わりです。お疲れさまでした。**

ここまでで、C++の言語機能はほぼカバーしました。
第III部では、これらを組み合わせて**実際に動くケモインフォマティクスのコード**を書いていきます。

→ [第19章 プロジェクトを作る（CMake）](ch19-project-and-cmake.md)
