# 第17章 モダンC++道具箱

C++11以降に追加された、日常的に使う便利機能をまとめて紹介します。
「知っていれば楽になる」ものばかりです。

## 17.1 auto と decltype

### auto の使いどころ（復習と補足）

```cpp
auto x = 42;                              // int
auto& r = v[0];                           // 参照（コピーしない）
const auto& cr = v[0];                    // const 参照
auto* p = &x;                             // ポインタ
auto it = m.find(key);                    // イテレータ（型名が長い）
for (const auto& [k, v] : map) { }        // 構造化束縛
```

> ⚠️ **auto は const と参照を落とします。**
> ```cpp
> const std::vector<int>& get();
> auto v = get();          // ★ std::vector<int>（コピー！const も参照も消える）
> const auto& r = get();   // ✓ const std::vector<int>&
> auto&& u = get();        // ✓ 万能（転送参照。const も参照も保つ）
> ```

### decltype と decltype(auto)

```cpp
int x = 5;
decltype(x) y = 10;                   // int
decltype(x + 1.0) z = 3;              // double

// 関数の戻り値型を「そのまま」返す（const/参照を保つ）
template <typename Container>
decltype(auto) first(Container& c) {
    return c[0];                      // T& を返す（auto なら T になってしまう）
}
```

`decltype(auto)` はテンプレートを書くときに時々必要になります。

---

## 17.2 構造化束縛（C++17）

```cpp
std::pair<std::string, double> p{"CCO", 46.07};
auto [smiles, mw] = p;                     // アンパック

std::tuple<int, double, std::string> t{1, 2.0, "x"};
auto [i, d, s] = t;

// map の反復
for (const auto& [key, value] : my_map) { }

// 構造体もOK（public メンバのみ）
struct Point { double x, y, z; };
Point pt{1, 2, 3};
auto [x, y, z] = pt;

// 配列も
int arr[3] = {1, 2, 3};
auto [a, b, c] = arr;
```

```python
smiles, mw = ("CCO", 46.07)      # Python のアンパック
```

参照で束縛することもできます:

```cpp
auto& [x, y, z] = pt;    // 元の pt を変更できる
x = 10;                   // pt.x が変わる

const auto& [k, v] = *it; // コピーなしで読む（推奨）
```

🧪 実用例:

```cpp
// map への挿入結果を受け取る
auto [it, inserted] = cache.emplace(smiles, fp);
if (!inserted) { /* 既にあった */ }

// 複数値を返す関数
auto [min_it, max_it] = std::minmax_element(v.begin(), v.end());

// from_chars
auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
```

---

## 17.3 std::optional — 「値があるかもしれない」

Pythonの `None` を返す関数を、型安全に表現します。

```cpp
#include <optional>

std::optional<Molecule> parse_smiles(std::string_view s) {
    if (!is_valid(s)) return std::nullopt;    // 失敗
    return Molecule(s);                        // 成功
}

auto mol = parse_smiles("CCO");
if (mol) {                          // 値があるか
    std::cout << mol->num_atoms();  // -> でアクセス
    std::cout << (*mol).num_atoms();
}

// または
if (auto mol = parse_smiles("CCO"); mol.has_value()) {
    use(*mol);
}
```

```python
def parse_smiles(s) -> Molecule | None:
    if not is_valid(s): return None
    return Molecule(s)

mol = parse_smiles("CCO")
if mol is not None:
    mol.num_atoms()
```

### API

```cpp
std::optional<int> o;

o.has_value();        // 値があるか
o.value();            // 値を取得（無ければ std::bad_optional_access 例外）
o.value_or(0);        // 値、または既定値
*o;                   // 値を取得（チェックなし。無ければ未定義動作）
o.reset();            // 空にする
o = 42;               // 値を設定
o = std::nullopt;     // 空にする

if (o) { }            // bool 変換
```

> ⚠️ **`*o` はチェックしません。** 必ず `if (o)` してから使うか、`value()` を使ってください。

### なぜ optional が嬉しいのか

```cpp
// ✗ 昔のやり方1: 特別な値で失敗を表す
int find_atom(const Molecule& m, int z);      // -1 が「見つからない」? 0? それとも例外?

// ✗ 昔のやり方2: 出力引数 + bool
bool find_atom(const Molecule& m, int z, int& out_index);

// ✓ optional
std::optional<int> find_atom(const Molecule& m, int z);
```

**「値が無いかもしれない」ことが型に書いてある**ので、
使う側が必ずチェックすることになります（`-Wall` では警告されませんが、
`*o` の前に `if` を書く習慣がつきます）。

### コスト

```cpp
sizeof(std::optional<int>)     // 8 （int 4 + bool 1 + パディング）
sizeof(std::optional<double>)  // 16
```

**ヒープ確保はありません。** 値をインラインで持ち、
「有効かどうか」のフラグを1バイト追加するだけです。

### C++23 のモナド操作

```cpp
auto result = parse_smiles(s)
            .transform([](const Molecule& m) { return m.mw(); })
            .value_or(0.0);

auto r2 = parse_smiles(s)
        .and_then([](const Molecule& m) -> std::optional<double> {
            return m.is_valid() ? std::optional{m.logp()} : std::nullopt;
        });
```

Rust の `Option` や Python の `Maybe` パターンに近い書き方です。

---

## 17.4 std::variant — 型安全な union

「A か B か C のどれか」を表します。

```cpp
#include <variant>

using AtomProperty = std::variant<int, double, std::string>;

AtomProperty p = 42;
p = 3.14;
p = std::string("aromatic");

// 型を確認して取り出す
if (std::holds_alternative<double>(p)) {
    double d = std::get<double>(p);
}

// visit で全ケースを処理
std::visit([](const auto& v) { std::cout << v << "\n"; }, p);

// インデックスでも取れる
std::cout << p.index();     // 0=int, 1=double, 2=string
```

```python
from typing import Union
AtomProperty = Union[int, float, str]
```

### visit のパターン

```cpp
// ① ジェネリックラムダ（全型で同じ処理）
std::visit([](const auto& v) { std::cout << v; }, p);

// ② if constexpr で型ごとに分岐
std::visit([](const auto& v) {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<T, int>) {
        std::cout << "int: " << v;
    } else if constexpr (std::is_same_v<T, double>) {
        std::cout << "double: " << v;
    } else {
        std::cout << "string: " << v;
    }
}, p);

// ③ overloaded イディオム（最も読みやすい）
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;   // C++17 では必要

std::visit(overloaded{
    [](int v)                { std::cout << "int: " << v; },
    [](double v)             { std::cout << "double: " << v; },
    [](const std::string& v) { std::cout << "string: " << v; },
}, p);
```

🧪 **実用例: SDF のプロパティフィールド**

```cpp
using PropValue = std::variant<std::int64_t, double, std::string>;
std::unordered_map<std::string, PropValue> properties;

properties["MW"]     = 180.16;
properties["ID"]     = std::string("CHEMBL25");
properties["N_RING"] = std::int64_t{1};
```

### variant vs 継承（再掲）

| | `std::variant` | 継承 + `unique_ptr` |
|---|---|---|
| ヒープ確保 | **なし**（値として持つ） | あり |
| 呼び出し | インライン展開されうる | 仮想呼び出し |
| 型の追加 | **再コンパイルが必要** | 実行時に追加可能 |
| サイズ | 最大の型のサイズ | ポインタサイズ |
| 用途 | 種類が固定 | プラグイン的拡張 |

---

## 17.5 std::span（C++20）— 配列への「窓」

`string_view` の配列版です。

```cpp
#include <span>

// 従来: 型ごとにオーバーロードが必要だった
double sum(const std::vector<double>& v);
double sum(const double* p, std::size_t n);
double sum(const std::array<double, 10>& a);

// span なら1つでOK
double sum(std::span<const double> data) {
    double s = 0;
    for (double x : data) s += x;
    return s;
}

std::vector<double> v = {1, 2, 3};
std::array<double, 3> a = {1, 2, 3};
double raw[3] = {1, 2, 3};

sum(v);    // ✓
sum(a);    // ✓
sum(raw);  // ✓
sum({v.data() + 1, 2});   // ✓ 部分ビュー
```

### 部分ビュー

```cpp
std::span<const double> s = v;
auto head = s.first(10);       // 先頭10要素
auto tail = s.last(10);        // 末尾10要素
auto mid  = s.subspan(5, 20);  // 5番目から20要素
```

🧪 **フィンガープリントのフラット配列を分割するのに最適:**

```cpp
std::vector<std::uint64_t> flat_fps(n_mols * 32);

// 分子 i のフィンガープリントを span で取り出す（コピーなし）
std::span<const std::uint64_t> fp_at(std::size_t i) const {
    return std::span(flat_fps).subspan(i * 32, 32);
}

double t = tanimoto(fp_at(0), fp_at(1));
```

> ⚠️ **span も所有権を持ちません。** 元の配列が生きている間だけ有効です。
> `string_view` と同じ注意点が当てはまります。

> 💡 **NumPy の配列を C++ に渡すとき、`std::span` は理想的なインタフェース**です。
> 第26章の pybind11 で使います。

---

## 17.6 std::chrono — 時間の扱い

```cpp
#include <chrono>
using namespace std::chrono;

auto t0 = steady_clock::now();
// ... 処理 ...
auto t1 = steady_clock::now();

auto ms = duration_cast<milliseconds>(t1 - t0).count();
std::cout << ms << " ms\n";

// 小数で欲しいとき
double d = duration<double, std::milli>(t1 - t0).count();
double sec = duration<double>(t1 - t0).count();
```

### 3つの時計

| 時計 | 用途 |
|---|---|
| `steady_clock` | **経過時間の計測**（絶対に巻き戻らない）★これを使う |
| `system_clock` | 現在時刻（カレンダー）。NTPで巻き戻ることがある |
| `high_resolution_clock` | 高精度（実装依存。多くの環境で steady_clock のエイリアス） |

> 💡 **ベンチマークには必ず `steady_clock`** を使ってください。
> `system_clock` は時刻同期で巻き戻ることがあり、負の経過時間が出ます。

### リテラル

```cpp
using namespace std::chrono_literals;

std::this_thread::sleep_for(100ms);
auto timeout = 5s;
auto interval = 1min;
```

---

## 17.7 std::filesystem — パス操作

```cpp
#include <filesystem>
namespace fs = std::filesystem;

fs::path p = "data/molecules/library.sdf";

p.filename();        // "library.sdf"
p.stem();            // "library"
p.extension();       // ".sdf"
p.parent_path();     // "data/molecules"

fs::exists(p);
fs::file_size(p);
fs::is_directory(p);
fs::create_directories("output/results");
fs::remove(p);
fs::copy(src, dst);

// ディレクトリの走査
for (const auto& entry : fs::directory_iterator("data")) {
    if (entry.path().extension() == ".sdf") {
        std::cout << entry.path() << "\n";
    }
}

// 再帰的に走査
for (const auto& entry : fs::recursive_directory_iterator("data")) { }

// パスの結合（OS 依存のセパレータを自動処理）
fs::path out = fs::path("results") / "run1" / "output.csv";
```

```python
from pathlib import Path
p = Path("data/molecules/library.sdf")
p.name; p.stem; p.suffix; p.parent
```

**Pythonの `pathlib` とほぼ同じ**設計です。移植が楽。

🧪 SDFファイルを一括処理する例:

```cpp
std::vector<fs::path> sdf_files;
for (const auto& e : fs::directory_iterator(input_dir)) {
    if (e.is_regular_file() && e.path().extension() == ".sdf") {
        sdf_files.push_back(e.path());
    }
}
std::sort(sdf_files.begin(), sdf_files.end());
```

---

## 17.8 std::format と std::print（C++20 / C++23）

第8章でも触れましたが、再掲します。

```cpp
#include <format>

std::string s = std::format("{:.3f}", 3.14159);
std::cout << std::format("{:<20}{:>10.2f}\n", name, value);

// C++23
#include <print>
std::print("{} has MW {:.2f}\n", name, mw);
std::println("{} atoms", n);          // 改行付き
```

### 自作型を format 対応にする

```cpp
struct Vec3 { double x, y, z; };

template <>
struct std::formatter<Vec3> : std::formatter<std::string> {
    auto format(const Vec3& v, format_context& ctx) const {
        return std::formatter<std::string>::format(
            std::format("({:.3f}, {:.3f}, {:.3f})", v.x, v.y, v.z), ctx);
    }
};

std::cout << std::format("{}\n", Vec3{1,2,3});   // (1.000, 2.000, 3.000)
```

---

## 17.9 その他の便利機能

### enum class（スコープ付き列挙型）

```cpp
enum class BondType : std::uint8_t {
    Single = 1, Double = 2, Triple = 3, Aromatic = 4
};

BondType b = BondType::Aromatic;

// int と暗黙変換されない（安全）
// int x = b;                    // ✗ エラー
int x = static_cast<int>(b);     // ✓ 明示的に

switch (b) {
    case BondType::Single: break;
    case BondType::Double: break;
    // ...
}
```

古い `enum` との違い:

```cpp
enum OldStyle { Single, Double };       // ★ Single, Double がグローバルに漏れる
enum class NewStyle { Single, Double }; // ✓ NewStyle::Single と書く必要がある
```

> 💡 **常に `enum class` を使ってください。** 名前の衝突と暗黙変換を防げます。
> `: std::uint8_t` で基底型を指定すると、メモリを節約できます。

### 属性

```cpp
[[nodiscard]] double tanimoto(const FP& a, const FP& b);
// 戻り値を無視すると警告が出る

[[deprecated("use tanimoto_fast instead")]]
double old_tanimoto(const FP& a, const FP& b);

[[maybe_unused]] int debug_counter = 0;   // 未使用警告を抑制

[[likely]] / [[unlikely]]                  // 分岐予測のヒント（C++20）
if (x > 0) [[likely]] { ... }

[[fallthrough]];                           // switch で意図的に落とす
[[noreturn]] void fatal_error(const char* msg);
```

`[[nodiscard]]` は特に有用です。「計算結果を使い忘れる」バグを防げます。

### 数値リテラルの区切り

```cpp
int n = 1'000'000;                  // 読みやすい
std::uint64_t mask = 0xFFFF'FFFF'FFFF'FFFF;
double x = 3.141'592'653;
```

### 二進リテラル・数値サフィックス

```cpp
std::uint64_t mask = 0b1010'1010;
auto a = 42u;      // unsigned
auto b = 42L;      // long
auto c = 42LL;     // long long
auto d = 42ULL;    // unsigned long long
auto e = 3.14f;    // float
```

### `<bit>`（C++20）— ビット操作

```cpp
#include <bit>

std::popcount(x);          // 立っているビット数（★フィンガープリントで必須）
std::countl_zero(x);       // 先頭の0の数
std::countr_zero(x);       // 末尾の0の数（最下位の1のインデックス）
std::has_single_bit(x);    // 2のべき乗か
std::bit_ceil(x);          // x以上の最小の2のべき乗
std::bit_width(x);         // 表現に必要なビット数
std::rotl(x, n);           // 左ローテート
std::bit_cast<float>(i);   // ビット列を別の型として解釈（安全な reinterpret）
```

第22章のフィンガープリント演算で全面的に使います。

### `<numbers>`（C++20）— 数学定数

```cpp
#include <numbers>
double pi = std::numbers::pi;
double e  = std::numbers::e;
double sqrt2 = std::numbers::sqrt2;
double ln2 = std::numbers::ln2;
```

### `<random>` — 乱数

```cpp
#include <random>

std::mt19937 rng(42);                                  // メルセンヌツイスタ、シード42
std::uniform_real_distribution<double> uniform(0.0, 1.0);
std::uniform_int_distribution<int> dice(1, 6);
std::normal_distribution<double> gauss(0.0, 1.0);

double x = uniform(rng);
int    d = dice(rng);
double g = gauss(rng);

// シャッフル
std::shuffle(v.begin(), v.end(), rng);

// ランダムに1つ選ぶ
std::uniform_int_distribution<std::size_t> pick(0, v.size() - 1);
auto& chosen = v[pick(rng)];
```

> ⚠️ **`rand()` は使わないでください。** 品質が低く、スレッドセーフでもありません。
>
> 💡 再現性のために、**シードを明示的に指定**してください。
> 実験の再現性はケモインフォマティクスで極めて重要です。
> ```cpp
> std::mt19937_64 rng(seed);   // 64bit 版。より高品質
> ```

### `<cmath>`

```cpp
#include <cmath>
std::sqrt, std::pow, std::exp, std::log, std::log10, std::log2
std::sin, std::cos, std::atan2
std::abs, std::floor, std::ceil, std::round, std::trunc
std::fmod, std::hypot
std::isnan, std::isinf, std::isfinite
std::lerp(a, b, t)      // C++20: 線形補間
std::midpoint(a, b)     // C++20: オーバーフローしない中点
```

---

## 17.10 🧪 実践: 全部入りの分子レコード

```cpp
// code/ch17/modern_record.cpp
#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

enum class Source : std::uint8_t { ChEMBL, PubChem, Internal, Unknown };

std::string_view to_string(Source s) {
    switch (s) {
        case Source::ChEMBL:   return "ChEMBL";
        case Source::PubChem:  return "PubChem";
        case Source::Internal: return "Internal";
        default:               return "Unknown";
    }
}

using PropValue = std::variant<std::int64_t, double, std::string, bool>;

struct MolRecord {
    std::string                  id;
    std::string                  smiles;
    Source                       source = Source::Unknown;
    std::optional<double>        activity;      // ★ 測定されていないかもしれない
    std::optional<std::string>   inchi_key;
    std::unordered_map<std::string, PropValue> props;

    [[nodiscard]] bool has_activity() const noexcept { return activity.has_value(); }
};

std::string format_prop(const PropValue& v) {
    return std::visit([](const auto& x) -> std::string {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, bool>) return x ? "true" : "false";
        else if constexpr (std::is_same_v<T, double>) return std::format("{:.3f}", x);
        else if constexpr (std::is_same_v<T, std::string>) return x;
        else return std::format("{}", x);
    }, v);
}

// span を使えば vector でも array でも受け取れる
double mean_activity(std::span<const MolRecord> records) {
    double sum = 0.0;
    int    n   = 0;
    for (const auto& r : records) {
        if (r.activity) { sum += *r.activity; ++n; }
    }
    return n ? sum / n : 0.0;
}

int main() {
    std::vector<MolRecord> records;

    records.push_back({
        .id = "CHEMBL25",
        .smiles = "CC(=O)Oc1ccccc1C(=O)O",
        .source = Source::ChEMBL,
        .activity = 5.2,
        .inchi_key = "BSYNRYMUTXBXSQ-UHFFFAOYSA-N",
        .props = {{"MW", 180.16}, {"n_rings", std::int64_t{1}},
                  {"is_acid", true}, {"name", std::string("aspirin")}}
    });

    records.push_back({
        .id = "INT001",
        .smiles = "CCO",
        .source = Source::Internal,
        .activity = std::nullopt,                 // 未測定
        .props = {{"MW", 46.07}, {"n_rings", std::int64_t{0}}}
    });

    for (const auto& r : records) {
        std::cout << std::format("{:<12} {:<28} {:<10} activity={}\n",
                                 r.id, r.smiles, to_string(r.source),
                                 r.activity ? std::format("{:.2f}", *r.activity)
                                            : "N/A");
        for (const auto& [k, v] : r.props) {
            std::cout << std::format("    {:<10} = {}\n", k, format_prop(v));
        }
    }

    std::cout << std::format("\nmean activity: {:.3f}\n", mean_activity(records));

    // 時間計測
    auto t0 = std::chrono::steady_clock::now();
    volatile double acc = 0;
    for (int i = 0; i < 1'000'000; ++i) acc += i * 0.001;
    auto ms = std::chrono::duration<double, std::milli>(
                  std::chrono::steady_clock::now() - t0).count();
    std::cout << std::format("loop took {:.3f} ms\n", ms);

    // ファイルシステム
    fs::path out = fs::path("results") / "run1" / "records.csv";
    std::cout << std::format("output stem = {}, ext = {}\n",
                             out.stem().string(), out.extension().string());
    return 0;
}
```

---

## 17.11 この章のまとめ

| 機能 | 用途 | Python相当 |
|---|---|---|
| `auto` | 型推論 | 型ヒント省略 |
| 構造化束縛 | アンパック | `a, b = tuple` |
| `std::optional<T>` | 値が無いかも | `T \| None` |
| `std::variant<A,B>` | どれか1つ | `Union[A, B]` |
| `std::span<T>` | 配列への窓 | memoryview / ndarray view |
| `std::string_view` | 文字列への窓 | — |
| `std::chrono` | 時間 | `time` / `datetime` |
| `std::filesystem` | パス操作 | `pathlib` |
| `std::format` | 書式化 | f-string |
| `enum class` | 列挙型 | `enum.Enum` |
| `<bit>` | ビット操作 | `int.bit_count()` |
| `<random>` | 乱数 | `random` / `np.random` |
| `[[nodiscard]]` | 戻り値の無視を警告 | — |

> 📝 **練習問題 17-1**
>
> `std::optional<double>` を返す `parse_double(std::string_view)` を書いてください。
> `std::from_chars` を使い、失敗したら `std::nullopt` を返します。

> 📝 **練習問題 17-2**
>
> `std::variant<int, double, std::string>` のvectorを受け取り、
> 各要素を型に応じてフォーマットして出力する関数を、
> `overloaded` イディオムで書いてください。

> 📝 **練習問題 17-3**
>
> `std::filesystem` を使って、指定ディレクトリ以下の `.smi` ファイルを
> すべて見つけ、合計行数を数えるプログラムを書いてください。

> 📝 **練習問題 17-4**
>
> `std::span` を使って、フラットな `std::vector<double>` を
> N行M列の行列として扱い、各行の合計を返す関数を書いてください。

---

→ [第18章 エラー処理](ch18-error-handling.md)
