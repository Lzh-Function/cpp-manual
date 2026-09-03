# 第8章 文字列 — std::string と string_view

SMILESもINCHIもSDFも文字列です。ケモインフォマティクスは文字列処理の塊なので、
ここは丁寧にやります。

## 8.1 3種類の「文字列」

C++には文字列を表すものが複数あります。まずここを整理しましょう。

| 型 | 実体 | 所有権 | 用途 |
|---|---|---|---|
| `const char*` | Cの文字列（`\0` 終端） | なし | Cライブラリとの連携、リテラル |
| `std::string` | 可変長の文字列クラス | **持つ** | 通常の文字列操作 |
| `std::string_view` | 文字列への「窓」（C++17） | なし | **読むだけの引数**、部分文字列 |

```cpp
const char* a = "CCO";            // リテラル。静的領域にある。変更不可
std::string b = "CCO";            // ヒープにコピーを持つ。変更可能
std::string_view c = b;           // b を指すだけ。コピーなし
```

---

## 8.2 std::string の基本

```cpp
#include <string>

std::string s = "CC(=O)O";
std::string t{"CCO"};
std::string u(5, 'C');            // "CCCCC"
std::string v;                    // 空文字列
```

### Python との対応表

| やりたいこと | Python | C++ |
|---|---|---|
| 長さ | `len(s)` | `s.size()` / `s.length()` |
| 空か | `not s` | `s.empty()` |
| 連結 | `s + t` | `s + t` |
| 追加 | `s += t` | `s += t` / `s.append(t)` |
| 1文字追加 | `s += "C"` | `s += 'C'` / `s.push_back('C')` |
| 添字 | `s[i]` | `s[i]`（`char` が返る） |
| 部分文字列 | `s[2:5]` | `s.substr(2, 3)`（開始, **長さ**） |
| 検索 | `s.find("CC")` | `s.find("CC")`（無ければ `std::string::npos`） |
| 含むか | `"CC" in s` | `s.find("CC") != std::string::npos` / C++23: `s.contains("CC")` |
| 前方一致 | `s.startswith("CC")` | C++20: `s.starts_with("CC")` |
| 後方一致 | `s.endswith("O")` | C++20: `s.ends_with("O")` |
| 置換 | `s.replace("a","b")` | 手書きが必要（後述） |
| 分割 | `s.split(",")` | 手書きが必要（後述） |
| 結合 | `",".join(xs)` | 手書きが必要（後述） |
| 大文字化 | `s.upper()` | `std::transform` + `std::toupper` |
| 数値変換 | `int(s)` | `std::stoi(s)` |
| 文字列化 | `str(n)` | `std::to_string(n)` |
| 反転 | `s[::-1]` | `std::reverse(s.begin(), s.end())` |
| 比較 | `s == t` | `s == t` ✓ |
| 辞書順 | `s < t` | `s < t` ✓ |

> ⚠️ **`substr` の第2引数は「終了位置」ではなく「長さ」です。**
> ```python
> s[2:5]        # Python: インデックス2から5の手前まで（3文字）
> ```
> ```cpp
> s.substr(2, 3)  // C++: インデックス2から3文字
> ```

> ⚠️ **`find` が見つからないとき `-1` ではなく `std::string::npos` を返します。**
> `npos` は `std::size_t` の最大値（18446744073709551615）です。
> ```cpp
> if (s.find("CC") != std::string::npos) { /* 見つかった */ }
> // ✗ if (s.find("CC") >= 0)  ← 符号なしなので常に true！
> ```

### 文字列の中身をループ

```cpp
std::string smiles = "CC(=O)O";

for (char c : smiles) {              // 1文字ずつ（読むだけ）
    std::cout << c;
}

for (char& c : smiles) {             // 書き換える
    c = std::toupper(static_cast<unsigned char>(c));
}

for (std::size_t i = 0; i < smiles.size(); ++i) {   // 添字が必要なとき
    if (smiles[i] == '(') { /* ... */ }
}
```

> ⚠️ **`std::toupper` に `char` をそのまま渡してはいけません。**
> `std::toupper(int)` は「`unsigned char` の値か EOF」を期待します。
> `char` が負の値（マルチバイト文字の一部など）だと未定義動作になります。
> `static_cast<unsigned char>(c)` を挟むのが正しい作法です。

---

## 8.3 std::string の内部構造と SSO

```cpp
// 概念図
class string {
    char*       data_;      // ヒープ上の文字列
    std::size_t size_;
    std::size_t capacity_;
};
```

vector とほぼ同じです。ただし重要な最適化があります。

### SSO (Small String Optimization)

短い文字列は**ヒープを使わず、string オブジェクト自身の中に埋め込みます**。

```
長い文字列 (SSO 適用外):
   +-------------+       ヒープ
   | data ------ |----> "CC(=O)Oc1ccccc1C(=O)O\0"
   | size:    21 |
   | capacity:21 |
   +-------------+

短い文字列 (SSO 適用):
   +--------------------------------+
   | "CCO\0" + フラグとサイズ         |   ← ヒープ確保ゼロ！
   +--------------------------------+
```

libstdc++ では **15文字まで**、libc++ では **22文字まで** がSSOの対象です
（`sizeof(std::string)` は 32 バイト）。

```cpp
std::string s1 = "CCO";                      // ヒープ確保なし（速い）
std::string s2 = "CC(=O)Oc1ccccc1C(=O)O";    // ヒープ確保あり
```

> 💡 **これは実務で効きます。**
> 元素記号（"C", "Cl", "Br"）や短いIDを `std::string` で持っても、
> SSOのおかげでヒープ確保が起きません。
>
> 逆に、SMILES文字列（通常20文字以上）は必ずヒープを使うので、
> **コピーを避けることが重要**です → `const std::string&` か `std::string_view`。

---

## 8.4 ★重要★ std::string_view

**C++17で入った、最も実用的な機能のひとつです。**

```cpp
#include <string_view>

void process(std::string_view sv);   // コピーなしで文字列を受け取る
```

`string_view` は「**ポインタ + 長さ**」だけを持つ、軽量な「窓」です。

```
   std::string s = "CC(=O)Oc1ccccc1";
                     │
                     ▼ ヒープ上のデータ
   +---+---+---+---+---+---+---+---+ ...
   | C | C | ( | = | O | ) | O | c | ...
   +---+---+---+---+---+---+---+---+ ...
       ▲           ▲
       └───────────┘
   std::string_view sv = std::string_view(s).substr(1, 4);
        { ptr = &s[1], size = 4 }   ← コピーなし！
```

### なぜ嬉しいのか

```cpp
// ✗ 従来: const char* を渡すと string が作られる（ヒープ確保 + コピー）
void parse(const std::string& s);
parse("CC(=O)Oc1ccccc1C(=O)O");   // ★ 一時的な std::string が作られる

// ✓ string_view: コピーゼロ
void parse(std::string_view s);
parse("CC(=O)Oc1ccccc1C(=O)O");   // ★ ポインタと長さを渡すだけ
```

さらに、`substr` が劇的に速くなります:

```cpp
std::string s = "very long SMILES ...";

std::string sub1 = s.substr(10, 20);              // ★ 新しい文字列を確保してコピー
std::string_view sub2 = std::string_view(s).substr(10, 20);   // ★ ポインタ計算だけ
```

🧪 **SMILESトークナイザでは、これが数倍の差になります**（第20章）。

### 使い方

```cpp
#include <string_view>

std::string_view sv = "CC(=O)O";

sv.size();
sv[0];
sv.substr(1, 3);           // string_view を返す（コピーなし）
sv.find("=O");
sv.starts_with("CC");      // C++20
sv.remove_prefix(2);       // 先頭2文字を窓から外す（ポインタを進めるだけ）
sv.remove_suffix(1);       // 末尾1文字を窓から外す

for (char c : sv) { }      // ループもできる

std::string s(sv);         // 必要になったら string に変換（ここでコピー）
```

### ⚠️ string_view の危険性: ダングリング

**`string_view` は所有権を持ちません。** 元の文字列が消えたら無効になります。

```cpp
std::string_view bad() {
    std::string s = "CCO";
    return s;               // ★ s は消える。返された view はダングリング
}

std::string_view sv;
{
    std::string s = "CCO";
    sv = s;
}                           // ★ s が破棄された
std::cout << sv;            // ★ 未定義動作
```

```cpp
// これも危険
std::string_view sv = std::string("CCO") + "H";   // ★ 一時オブジェクトが即座に消える
```

> 💡 **string_view を使う場所のルール**
>
> ✅ 使ってよい:
> - **関数の引数**（呼び出し中は元の文字列が生きている）
> - **ローカル変数**（元の文字列と同じスコープ内）
> - パース処理の一時的なトークン
>
> ❌ 使ってはいけない:
> - **クラスのメンバ変数**（元の文字列より長生きしうる）
> - **関数の戻り値**（ローカル文字列を指す場合）
> - **コンテナに格納**（`std::vector<std::string_view>` は寿命管理が難しい）
>
> 迷ったら `std::string` を使ってください。安全側に倒すのが正解です。

### 引数の型: 結論

```cpp
// 読むだけの文字列引数 → string_view
void parse(std::string_view smiles);

// 保存する（メンバに持つ）→ string を値で受けてムーブ
class Molecule {
    std::string smiles_;
public:
    explicit Molecule(std::string s) : smiles_(std::move(s)) {}
};
```

---

## 8.5 Pythonにあって C++ にない機能を自作する

### split

```cpp
// code/ch08/strutil.hpp
#pragma once
#include <string>
#include <string_view>
#include <vector>

// 区切り文字で分割（string_view を返すので高速。元の文字列が生きている必要あり）
inline std::vector<std::string_view> split(std::string_view s, char delim) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    while (true) {
        std::size_t pos = s.find(delim, start);
        if (pos == std::string_view::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

// 文字列版（安全だが遅い）
inline std::vector<std::string> split_owned(std::string_view s, char delim) {
    std::vector<std::string> out;
    for (auto sv : split(s, delim)) out.emplace_back(sv);
    return out;
}
```

使い方:

```cpp
std::string line = "CCO,ethanol,46.07,-0.31";
auto fields = split(line, ',');
// fields[0] == "CCO", fields[1] == "ethanol", ...

std::cout << fields[1] << "\n";
double mw = std::stod(std::string(fields[2]));
```

### join

```cpp
inline std::string join(const std::vector<std::string>& parts,
                        std::string_view sep) {
    if (parts.empty()) return "";
    // 必要なサイズを先に計算して1回で確保（reserveの文字列版）
    std::size_t total = 0;
    for (const auto& p : parts) total += p.size();
    total += sep.size() * (parts.size() - 1);

    std::string out;
    out.reserve(total);
    out += parts[0];
    for (std::size_t i = 1; i < parts.size(); ++i) {
        out += sep;
        out += parts[i];
    }
    return out;
}
```

### trim

```cpp
inline std::string_view ltrim(std::string_view s) {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}
inline std::string_view rtrim(std::string_view s) {
    std::size_t i = s.size();
    while (i > 0 && std::isspace(static_cast<unsigned char>(s[i-1]))) --i;
    return s.substr(0, i);
}
inline std::string_view trim(std::string_view s) { return ltrim(rtrim(s)); }
```

### replace_all

```cpp
inline std::string replace_all(std::string_view s,
                               std::string_view from,
                               std::string_view to) {
    if (from.empty()) return std::string(s);
    std::string out;
    out.reserve(s.size());
    std::size_t start = 0;
    while (true) {
        std::size_t pos = s.find(from, start);
        if (pos == std::string_view::npos) {
            out += s.substr(start);
            break;
        }
        out += s.substr(start, pos - start);
        out += to;
        start = pos + from.size();
    }
    return out;
}
```

### 大文字/小文字変換

```cpp
#include <algorithm>
#include <cctype>

inline std::string to_upper(std::string s) {   // 値で受けて加工して返す
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return s;
}
```

> 💡 これらのユーティリティは、実務では
> [abseil](https://abseil.io/) の `absl::StrSplit`, `absl::StrJoin` や
> [fmt](https://github.com/fmtlib/fmt) を使うのが楽です。
> でも自分で書けることは大事です（10行で済みますし）。

---

## 8.6 数値との変換

```cpp
// 文字列 → 数値
int    i = std::stoi("42");
long   l = std::stol("42");
double d = std::stod("3.14");
float  f = std::stof("3.14");

// 変換できないと例外（std::invalid_argument / std::out_of_range）
try {
    int x = std::stoi("abc");
} catch (const std::invalid_argument& e) {
    std::cerr << "bad number\n";
}

// 数値 → 文字列
std::string s = std::to_string(42);
std::string t = std::to_string(3.14);   // "3.140000" ← 桁数の制御ができない
```

### ★ std::from_chars（C++17）— 高速・例外なし

大量のデータをパースするなら**これを使ってください**。

```cpp
#include <charconv>

std::string_view sv = "180.16";
double value;
auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
if (ec == std::errc{}) {
    // 成功。value に値が入っている
} else {
    // 失敗（例外は投げない）
}
```

**`std::stod` の3〜10倍速い**です。理由:
- `std::string` の一時オブジェクトを作らない
- ロケール（地域設定）を見ない
- 例外を投げない

> 🧪 100万行のSDFやCSVをパースするとき、この差は数秒〜数十秒になります。
> 第24章で使います。
>
> ⚠️ 浮動小数点の `from_chars` は GCC 11+ / MSVC が対応。
> Clang（libc++）は長く未対応でした。整数版は広くサポートされています。

### 数値 → 文字列（高速版）

```cpp
char buf[32];
auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), 3.14159);
std::string s(buf, ptr);
```

---

## 8.7 std::format（C++20）— f-string 相当

```cpp
#include <format>

std::string s = std::format("{} has MW {:.2f}", "aspirin", 180.157);
// "aspirin has MW 180.16"

std::cout << std::format("{:<20}{:>8.3f}\n", name, value);
```

書式指定はPythonの `str.format` とほぼ同じです。

| 書式 | 意味 |
|---|---|
| `{}` | デフォルト |
| `{:.3f}` | 小数点以下3桁 |
| `{:>10}` | 右寄せ幅10 |
| `{:<10}` | 左寄せ幅10 |
| `{:^10}` | 中央寄せ幅10 |
| `{:08.3f}` | ゼロ埋め幅8、小数3桁 |
| `{:x}` / `{:b}` | 16進 / 2進 |
| `{:e}` | 指数表記 |
| `{0} {1} {0}` | 位置指定 |

```python
f"{name:<20}{value:>8.3f}"     # Python の f-string
```

**ほぼ同じです。** 移植が楽なのがありがたい。

> ⚠️ コンパイラが未対応なら [fmt](https://github.com/fmtlib/fmt) を使ってください。
> ```cpp
> #include <fmt/format.h>
> std::string s = fmt::format("{:.2f}", 3.14159);
> fmt::print("{} atoms\n", n);        // 出力も速い
> ```
> fmt は `std::format` の元になったライブラリで、APIはほぼ同一です。
> しかも `printf` より速いです。

### 古い方法（見かけたら）

```cpp
// iostream のマニピュレータ（読みにくい）
#include <iomanip>
std::cout << std::fixed << std::setprecision(2) << std::setw(10) << value << "\n";

// C の printf（型安全でない）
printf("%.2f\n", value);   // ★ 型を間違えるとクラッシュ
```

`std::format` / `fmt` があるなら、そちらを使ってください。

---

## 8.8 🧪 実践: SMILESの正規化とトークン分割

```cpp
// code/ch08/smiles_tokens.cpp
#include <cctype>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

// SMILES のトークン種別
enum class TokenKind {
    Atom,          // C, N, O, [nH], [Fe+2] など
    Bond,          // - = # : / \
    RingBond,      // 1, 2, %10
    BranchOpen,    // (
    BranchClose,   // )
    Dot,           // .
    Unknown
};

struct Token {
    TokenKind        kind;
    std::string_view text;   // 元文字列への窓（コピーなし）
};

const char* kind_name(TokenKind k) {
    switch (k) {
        case TokenKind::Atom:        return "Atom";
        case TokenKind::Bond:        return "Bond";
        case TokenKind::RingBond:    return "Ring";
        case TokenKind::BranchOpen:  return "(";
        case TokenKind::BranchClose: return ")";
        case TokenKind::Dot:         return ".";
        default:                     return "?";
    }
}

// 2文字の有機元素記号か判定
bool is_two_char_element(char a, char b) {
    return (a == 'C' && b == 'l') || (a == 'B' && b == 'r');
}

std::vector<Token> tokenize(std::string_view smiles) {
    std::vector<Token> tokens;
    tokens.reserve(smiles.size());          // 上限は文字数

    std::size_t i = 0;
    while (i < smiles.size()) {
        char c = smiles[i];

        // --- ブラケット原子 [nH], [Fe+2], [13C] ---
        if (c == '[') {
            std::size_t close = smiles.find(']', i);
            if (close == std::string_view::npos) {
                tokens.push_back({TokenKind::Unknown, smiles.substr(i)});
                break;
            }
            tokens.push_back({TokenKind::Atom, smiles.substr(i, close - i + 1)});
            i = close + 1;
            continue;
        }

        // --- 2文字元素 ---
        if (i + 1 < smiles.size() && is_two_char_element(c, smiles[i+1])) {
            tokens.push_back({TokenKind::Atom, smiles.substr(i, 2)});
            i += 2;
            continue;
        }

        // --- 有機部分集合の1文字原子 ---
        if (std::string_view("BCNOPSFI").find(c) != std::string_view::npos ||
            std::string_view("bcnops").find(c)   != std::string_view::npos) {
            tokens.push_back({TokenKind::Atom, smiles.substr(i, 1)});
            ++i;
            continue;
        }

        // --- 結合記号 ---
        if (std::string_view("-=#$:/\\").find(c) != std::string_view::npos) {
            tokens.push_back({TokenKind::Bond, smiles.substr(i, 1)});
            ++i;
            continue;
        }

        // --- 環結合番号（%NN 形式も対応）---
        if (c == '%') {
            std::size_t n = 1;
            while (i + n < smiles.size() && std::isdigit(
                       static_cast<unsigned char>(smiles[i+n]))) ++n;
            tokens.push_back({TokenKind::RingBond, smiles.substr(i, n)});
            i += n;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back({TokenKind::RingBond, smiles.substr(i, 1)});
            ++i;
            continue;
        }

        // --- 構造記号 ---
        if (c == '(') { tokens.push_back({TokenKind::BranchOpen,  smiles.substr(i,1)}); ++i; continue; }
        if (c == ')') { tokens.push_back({TokenKind::BranchClose, smiles.substr(i,1)}); ++i; continue; }
        if (c == '.') { tokens.push_back({TokenKind::Dot,         smiles.substr(i,1)}); ++i; continue; }

        tokens.push_back({TokenKind::Unknown, smiles.substr(i, 1)});
        ++i;
    }
    return tokens;
}

int main() {
    std::vector<std::string> tests = {
        "CCO",
        "CC(=O)Oc1ccccc1C(=O)O",
        "Cn1cnc2c1c(=O)n(C)c(=O)n2C",
        "[Na+].[Cl-]",
        "C[C@H](N)C(=O)O",              // L-アラニン（立体あり）
        "ClCCBr",
        "c1ccc2c(c1)cccc2",             // ナフタレン
    };

    for (const auto& s : tests) {
        std::cout << "=== " << s << " ===\n";
        auto tokens = tokenize(s);      // s は生きているので string_view は安全
        for (const auto& t : tokens) {
            std::cout << "  " << kind_name(t.kind) << " : " << t.text << "\n";
        }
        std::cout << "  total tokens: " << tokens.size() << "\n\n";
    }
    return 0;
}
```

```bash
g++ -std=c++20 -O2 -Wall smiles_tokens.cpp -o smiles_tokens
./smiles_tokens
```

出力（一部）:

```
=== CC(=O)Oc1ccccc1C(=O)O ===
  Atom : C
  Atom : C
  ( : (
  Bond : =
  Atom : O
  ) : )
  Atom : O
  Atom : c
  Ring : 1
  ...
```

**ポイント:**
- `std::string_view` でトークンを表現 → **文字列のコピーがゼロ**
- `tokens.reserve()` で再確保を回避
- `switch` / `find` による高速な文字分類

第20章で、これを本格的な分子グラフ構築まで発展させます。

---

## 8.9 文字コードの話（少しだけ）

C++の `std::string` は**バイト列**です。UTF-8 も入りますが、
`s.size()` は**バイト数**であって文字数ではありません。

```cpp
std::string s = "分子";
std::cout << s.size() << "\n";   // 6（UTF-8で1文字3バイト）
```

```python
s = "分子"
len(s)   # 2（Python 3 の str は Unicode コードポイント列）
```

> 💡 **ケモインフォマティクスでは幸い問題になりません。**
> SMILES、InChI、元素記号はすべてASCIIです。
> 日本語を扱うなら、C++20 の `char8_t`/`std::u8string` や
> [ICU](https://icu.unicode.org/) ライブラリが必要になりますが、
> 本書では扱いません。
>
> ただし、**分子名やコメント欄には日本語が入りうる**ので、
> 「`std::string` はバイト列」ということだけ覚えておいてください。
> バイト単位で切ると文字化けします。

---

## 8.10 この章のまとめ

- `std::string` は所有する文字列、`std::string_view` は所有しない「窓」
- **読むだけの引数は `std::string_view`**（コピーゼロ）
- **ただし string_view はメンバ変数や戻り値にしない**（ダングリング）
- `substr(開始, **長さ**)`。Pythonのスライスとは第2引数の意味が違う
- `find` が見つからないときは `std::string::npos`（`-1` ではない）
- 短い文字列は SSO でヒープ確保が起きない
- split / join / trim は標準にないので自作（10行）
- **大量パースには `std::from_chars`**（`stod` より数倍速い）
- 書式化は `std::format`（or `fmt`）。Pythonの f-string とほぼ同じ
- `std::string` はバイト列。UTF-8 の文字数 ≠ `size()`

> 📝 **練習問題 8-1**
>
> `split` を使って、CSV形式の1行
> `"CHEMBL25,CC(=O)Oc1ccccc1C(=O)O,180.16,1.19"` から
> ID, SMILES, MW, LogP を取り出すコードを書いてください。
> 数値変換には `std::from_chars` を使ってみましょう。

> 📝 **練習問題 8-2**
>
> `std::string` と `std::string_view` で `substr` を100万回実行し、
> 実行時間を比較してください。

> 📝 **練習問題 8-3**
>
> 8.8 のトークナイザに、立体化学記号 `@`, `@@` と
> 電荷 `+`, `-` を扱う処理を追加してください。
> （ヒント: ブラケット内は既に1トークンとして扱われています。
> ブラケット内をさらに解析する関数を書いてみましょう）

> 📝 **練習問題 8-4**
>
> 次のコードの何が危険ですか?
> ```cpp
> std::vector<std::string_view> get_tokens() {
>     std::string s = read_smiles_from_file();
>     return tokenize(s);
> }
> ```

---

→ [第9章 STLコンテナ完全ガイド](ch09-stl-containers.md)
