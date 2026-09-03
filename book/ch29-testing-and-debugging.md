# 第29章 テスト・デバッグ・サニタイザ

C++は「バグが静かに潜む」言語です。
Pythonなら `IndexError` で止まるところが、C++では**動き続けて後で壊れます**。

だからこそ、**ツールで自動検出する体制**が重要です。

## 29.1 サニタイザ — C++で最も価値のあるツール

**この節だけでも読んでください。** サニタイザを知らずにC++を書くのは、
シートベルトなしで運転するようなものです。

### AddressSanitizer (ASan) — メモリエラー検出

```bash
g++ -std=c++20 -g -O1 -fsanitize=address -fno-omit-frame-pointer prog.cpp -o prog
./prog
```

検出できるもの:

| バグ | 例 |
|---|---|
| **配列の範囲外アクセス** | `v[100]` （サイズ10） |
| **解放後の使用 (use-after-free)** | `delete p; *p = 1;` |
| **スコープを抜けた変数の使用** | ローカル変数へのポインタを返す |
| **二重解放** | `delete p; delete p;` |
| **メモリリーク** | `new` して `delete` しない |
| **スタックオーバーフロー** | 深い再帰 |

実例:

```cpp
#include <vector>
int main() {
    std::vector<int> v = {1, 2, 3};
    return v[10];              // ★ 範囲外
}
```

```bash
g++ -g -fsanitize=address -D_GLIBCXX_ASSERTIONS test.cpp -o test
./test
```

```
=================================================================
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602000000038
READ of size 4 at 0x602000000038 thread T0
    #0 0x4011f5 in main test.cpp:4
    #1 0x7f2a in __libc_start_main

0x602000000038 is located 28 bytes to the right of 12-byte region
allocated by thread T0 here:
    #0 0x7f3b in operator new(unsigned long)
    #1 0x401 in std::vector<int>::_M_realloc_insert
```

**行番号まで教えてくれます。**

> 💡 **`-D_GLIBCXX_ASSERTIONS`（GCC）を併用してください。**
> `std::vector::operator[]` の範囲チェックが有効になります。
> libc++ なら `-D_LIBCPP_ENABLE_ASSERTIONS=1`（新しめの版では `-D_LIBCPP_HARDENING_MODE=...`）。

**コスト: 実行速度2倍、メモリ3倍。** 開発中は常に有効にすべきです。

### UndefinedBehaviorSanitizer (UBSan)

```bash
g++ -std=c++20 -g -O1 -fsanitize=undefined prog.cpp -o prog
```

検出できるもの:

| バグ | 例 |
|---|---|
| **符号付き整数のオーバーフロー** | `INT_MAX + 1` |
| **ゼロ除算** | `x / 0` |
| **不正なシフト** | `1 << 40`（intに対して） |
| **null ポインタのデリファレンス** | `*(int*)nullptr` |
| **アライメント違反** | 不正なキャスト |
| **不正な enum 値** | 範囲外の値をenumにキャスト |
| **float → int の範囲外変換** | `(int)1e20` |

```cpp
int main() {
    int x = 1;
    return x << 40;       // ★ int は32bit
}
```

```
prog.cpp:3:14: runtime error: shift exponent 40 is too large
                for 32-bit type 'int'
```

**コスト: 実行速度20%程度。** ASanと同時に使えます。

```bash
g++ -fsanitize=address,undefined ...
```

### ThreadSanitizer (TSan) — データ競合検出

```bash
g++ -std=c++20 -g -O1 -fsanitize=thread prog.cpp -o prog
```

```cpp
#include <thread>
int counter = 0;
void work() { for (int i = 0; i < 1000; ++i) ++counter; }
int main() {
    std::thread t1(work), t2(work);
    t1.join(); t2.join();
}
```

```
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x55f8 by thread T2:
    #0 work() prog.cpp:3

  Previous write of size 4 at 0x55f8 by thread T1:
    #0 work() prog.cpp:3
```

**並列コードのバグは再現しないので、これは必須です。**

> ⚠️ TSan は ASan と**同時に使えません**。別々にビルドしてください。
> コスト: 実行速度5〜15倍、メモリ5〜10倍。

### MemorySanitizer (MSan) — 未初期化メモリ

```bash
clang++ -std=c++20 -g -O1 -fsanitize=memory -fno-omit-frame-pointer prog.cpp
```

```cpp
int main() {
    int x;
    if (x > 0) return 1;      // ★ 未初期化
    return 0;
}
```

> ⚠️ **Clangのみ。** さらに、**標準ライブラリも MSan 付きでビルドする必要**があり、
> 導入のハードルが高いです。`-Wall` の `-Wuninitialized` である程度カバーできます。

### CMake での設定

```cmake
option(ENABLE_ASAN "AddressSanitizer + UBSan" OFF)
option(ENABLE_TSAN "ThreadSanitizer"          OFF)

if(ENABLE_ASAN AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
    add_compile_definitions(_GLIBCXX_ASSERTIONS)
endif()

if(ENABLE_TSAN AND NOT MSVC)
    add_compile_options(-fsanitize=thread -g)
    add_link_options(-fsanitize=thread)
endif()
```

```bash
cmake --preset debug-asan && ctest --preset debug-asan
cmake --preset debug-tsan && ctest --preset debug-tsan
```

> 💡 **MSVC にも AddressSanitizer があります:**
> ```
> cl /fsanitize=address /Zi prog.cpp
> ```

### Valgrind（サニタイザが使えない環境で）

```bash
valgrind --leak-check=full --track-origins=yes ./prog
```

**遅い（20〜50倍）**ですが、再コンパイル不要で使えます。

---

## 29.2 テストフレームワーク

### Catch2

```cmake
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(tests test_main.cpp test_smiles.cpp test_fp.cpp)
target_link_libraries(tests PRIVATE chemcpp::chemcpp Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(tests)
```

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("tanimoto basics", "[similarity]") {
    Fingerprint a{}, b{};

    SECTION("empty fingerprints") {
        CHECK(tanimoto(a, b) == 0.0);
    }

    SECTION("identical fingerprints") {
        set_bit(a, 5); set_bit(a, 100);
        b = a;
        CHECK(tanimoto(a, b) == 1.0);
    }

    SECTION("disjoint fingerprints") {
        set_bit(a, 5);
        set_bit(b, 6);
        CHECK(tanimoto(a, b) == 0.0);
    }

    SECTION("partial overlap") {
        set_bit(a, 1); set_bit(a, 2); set_bit(a, 3);
        set_bit(b, 2); set_bit(b, 3); set_bit(b, 4);
        // 共通2、和集合4 → 0.5
        CHECK_THAT(tanimoto(a, b), Catch::Matchers::WithinAbs(0.5, 1e-12));
    }
}
```

**`SECTION` は、それぞれ独立に `TEST_CASE` の先頭から実行されます。**
（`a`, `b` は各セクションで初期状態に戻ります）これがCatch2の強力な機能です。

### アサーションマクロ

```cpp
REQUIRE(cond);          // 失敗したらそのテストを中断
CHECK(cond);            // 失敗しても続行（複数の失敗を一度に見られる）

REQUIRE_FALSE(cond);
CHECK_FALSE(cond);

REQUIRE_THROWS(expr);
REQUIRE_THROWS_AS(expr, std::runtime_error);
REQUIRE_NOTHROW(expr);

CHECK_THAT(value, Catch::Matchers::WithinAbs(3.14, 0.01));
CHECK_THAT(value, Catch::Matchers::WithinRel(3.14, 0.001));
CHECK_THAT(str,   Catch::Matchers::ContainsSubstring("CCO"));
CHECK_THAT(vec,   Catch::Matchers::Contains(42));

INFO("smiles = " << s);        // 失敗時に追加情報を表示
CAPTURE(i, j, value);          // 変数の値を自動表示
```

### パラメータ化テスト

```cpp
TEST_CASE("parse known molecules", "[smiles]") {
    struct Case { const char* smiles; std::size_t atoms; double mw; };

    auto c = GENERATE(
        Case{"CCO",                     3, 46.07},
        Case{"c1ccccc1",                6, 78.11},
        Case{"CC(=O)Oc1ccccc1C(=O)O",  13, 180.16},
        Case{"Cn1cnc2c1c(=O)n(C)c(=O)n2C", 14, 194.19}
    );

    INFO("SMILES: " << c.smiles);
    auto mol = parse_smiles(c.smiles);
    REQUIRE(mol);
    CHECK(mol->num_atoms() == c.atoms);
    CHECK_THAT(molecular_weight(*mol),
               Catch::Matchers::WithinAbs(c.mw, 0.05));
}
```

`GENERATE` は、各値についてテストを繰り返します。

### ベンチマークも書ける

```cpp
TEST_CASE("tanimoto performance", "[!benchmark]") {
    auto fps = make_random_fps(1000);

    BENCHMARK("tanimoto") {
        return tanimoto(fps[0], fps[1]);
    };

    BENCHMARK("tanimoto_precomputed") {
        return tanimoto_fast(fps[0], c0, fps[1], c1);
    };
}
```

```bash
./tests "[!benchmark]"
```

### 実行

```bash
./tests                          # 全部
./tests "[smiles]"               # タグで絞る
./tests "parse known molecules"  # 名前で絞る
./tests -s                       # 成功したテストも表示
./tests --list-tests
ctest --test-dir build --output-on-failure -j8
```

### doctest（軽量な代替）

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("tanimoto") {
    CHECK(tanimoto(a, b) == 0.5);
}
```

**1ヘッダだけ。コンパイルが Catch2 の5倍速い。**
本番コードに直接テストを埋め込めるのも特徴です。

---

## 29.3 何をテストするか

### ケモインフォマティクス的なテスト戦略

```cpp
// ① 既知の正解（ゴールデンテスト）
TEST_CASE("known molecular weights") {
    CHECK_THAT(mw("CCO"),      WithinAbs(46.069, 0.01));
    CHECK_THAT(mw("c1ccccc1"), WithinAbs(78.114, 0.01));
}

// ② 不変条件（プロパティテスト）
TEST_CASE("tanimoto properties") {
    auto a = random_fp(), b = random_fp();

    CHECK(tanimoto(a, a) == 1.0);                    // 反射律
    CHECK(tanimoto(a, b) == tanimoto(b, a));         // 対称律
    CHECK(tanimoto(a, b) >= 0.0);                    // 値域
    CHECK(tanimoto(a, b) <= 1.0);
}

// ③ 往復変換（ラウンドトリップ）
TEST_CASE("smiles roundtrip") {
    for (const auto& s : test_smiles) {
        auto m1 = parse_smiles(s);
        REQUIRE(m1);
        auto canonical = to_smiles(*m1);
        auto m2 = parse_smiles(canonical);
        REQUIRE(m2);
        CHECK(m1->num_atoms() == m2->num_atoms());
        CHECK(morgan_fingerprint(*m1) == morgan_fingerprint(*m2));
    }
}

// ④ 参照実装との比較（RDKit と）
TEST_CASE("agrees with rdkit", "[integration]") {
    // 事前にRDKitで計算した値をファイルから読む
    for (const auto& [smiles, expected_mw] : load_reference_data()) {
        auto m = parse_smiles(smiles);
        REQUIRE(m);
        CHECK_THAT(molecular_weight(*m), WithinRel(expected_mw, 0.001));
    }
}

// ⑤ エッジケース
TEST_CASE("edge cases") {
    CHECK_FALSE(parse_smiles("").has_value());          // 空文字列
    CHECK_FALSE(parse_smiles("(((").has_value());       // 壊れた入力
    CHECK(parse_smiles("C").has_value());               // 最小の分子
    CHECK(parse_smiles("[He]").has_value());            // 結合なし

    Molecule empty;
    CHECK(popcount(morgan_fingerprint(empty)) == 0);    // 空の分子
    CHECK(molecular_weight(empty) == 0.0);
}

// ⑥ 回帰テスト（バグを見つけたら必ずテストを書く）
TEST_CASE("regression: issue #42 - Cl parsed as C and l") {
    auto m = parse_smiles("ClCCl");
    REQUIRE(m);
    CHECK(m->num_atoms() == 3);                         // C, Cl, Cl
    CHECK(m->atom(0).atomic_num == 17);
}
```

### ファジング（発展）

```cpp
// fuzz/fuzz_smiles.cpp
#include <chemcpp/smiles.hpp>
#include <cstdint>
#include <cstddef>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    std::string s(reinterpret_cast<const char*>(data), size);
    auto mol = chemcpp::parse_smiles(s);   // クラッシュしないことを確認
    if (mol) {
        (void)chemcpp::molecular_weight(*mol);
        (void)chemcpp::morgan_fingerprint(*mol);
    }
    return 0;
}
```

```bash
clang++ -std=c++20 -g -O1 -fsanitize=fuzzer,address \
    fuzz_smiles.cpp -o fuzz_smiles -lchemcpp
./fuzz_smiles -max_total_time=60
```

**ランダムな入力を大量に投げて、クラッシュを見つけます。**
パーサには特に有効です。

---

## 29.4 デバッガの使い方

### gdb（Linux）

```bash
g++ -g -O0 prog.cpp -o prog
gdb ./prog
```

| コマンド | 意味 |
|---|---|
| `run` / `r` | 実行 |
| `break main` / `b file.cpp:42` | ブレークポイント |
| `break f if x > 100` | 条件付きブレークポイント |
| `next` / `n` | 次の行（関数に入らない） |
| `step` / `s` | 次の行（関数に入る） |
| `finish` | 現在の関数から抜ける |
| `continue` / `c` | 次のブレークポイントまで |
| `print x` / `p x` | 変数を表示 |
| `p *v._M_impl._M_start@10` | vector の中身を10個表示 |
| `backtrace` / `bt` | コールスタック |
| `frame 2` | スタックフレームを移動 |
| `info locals` | ローカル変数を全部表示 |
| `watch x` | 変数が変わったら止まる |
| `x/16xb ptr` | メモリをダンプ |
| `set var x = 5` | 変数を書き換える |
| `quit` | 終了 |

### クラッシュの原因を特定する

```bash
# コアダンプを有効化
ulimit -c unlimited
./prog                       # クラッシュ
gdb ./prog core
(gdb) bt                     # ★ どこでクラッシュしたか分かる
```

または直接:

```bash
gdb --args ./prog arg1 arg2
(gdb) run
# クラッシュしたら
(gdb) bt full
```

### STL コンテナを見やすく表示する

```bash
# ~/.gdbinit
python
import sys
sys.path.insert(0, '/usr/share/gcc/python')
from libstdcxx.v6.printers import register_libstdcxx_printers
register_libstdcxx_printers(None)
end

set print pretty on
```

これで `p v` が `{1, 2, 3}` と表示されるようになります。

### lldb（macOS）

```bash
lldb ./prog
(lldb) breakpoint set --file main.cpp --line 42
(lldb) run
(lldb) frame variable
(lldb) bt
```

コマンド名が違うだけで、gdbとほぼ同じです。

### VSCode でのデバッグ

第2章の `launch.json` を設定すれば、F5でGUIデバッグできます。
ブレークポイント、変数ウォッチ、コールスタックがGUIで見られます。

**Pythonのデバッグと同じ感覚で使えます。**

### printf デバッグも侮らない

```cpp
#define DBG(x) std::cerr << #x " = " << (x) << "  (" << __FILE__ \
                         << ":" << __LINE__ << ")\n"

DBG(mol.num_atoms());
// mol.num_atoms() = 13  (smiles.cpp:142)
```

`#x` はマクロの引数を文字列にする演算子（stringize）です。

---

## 29.5 静的解析

コンパイル前にバグを見つけるツールです。

### コンパイラの警告（最も費用対効果が高い）

```bash
g++ -Wall -Wextra -Wpedantic \
    -Wshadow                 # 変数の隠蔽 \
    -Wconversion             # 暗黙の型変換 \
    -Wsign-conversion        # 符号の変換 \
    -Wnon-virtual-dtor       # 仮想デストラクタ忘れ \
    -Wold-style-cast         # C形式のキャスト \
    -Woverloaded-virtual     # 仮想関数の隠蔽 \
    -Wnull-dereference \
    -Wdouble-promotion       # float → double の暗黙変換 \
    -Wformat=2 \
    -Wcast-align \
    -Wunused \
    prog.cpp
```

**警告をゼロに保つ習慣**が、最も安いバグ対策です。

```bash
-Werror     # 警告をエラーにする（CI で有効化）
```

### clang-tidy

```yaml
# .clang-tidy
Checks: >
  bugprone-*,
  performance-*,
  modernize-*,
  readability-*,
  cppcoreguidelines-*,
  -modernize-use-trailing-return-type,
  -readability-magic-numbers,
  -cppcoreguidelines-avoid-magic-numbers,
  -readability-identifier-length

WarningsAsErrors: 'bugprone-*'
HeaderFilterRegex: 'include/chemcpp/.*'
```

```bash
clang-tidy src/*.cpp -p build/
# 自動修正
clang-tidy -fix src/*.cpp -p build/
```

CMakeで統合:

```cmake
set(CMAKE_CXX_CLANG_TIDY clang-tidy;-warnings-as-errors=bugprone-*)
```

clang-tidy が見つけるもの:

```cpp
// performance-unnecessary-value-param
void f(std::string s);         // → const std::string& にすべき

// modernize-use-nullptr
int* p = NULL;                 // → nullptr

// bugprone-use-after-move
auto b = std::move(a);
use(a);                        // ★ ムーブ後の使用

// performance-for-range-copy
for (auto x : vec)             // → const auto&

// modernize-loop-convert
for (int i = 0; i < v.size(); ++i)   // → range-based for
```

### cppcheck

```bash
cppcheck --enable=all --std=c++20 --inline-suppr src/
```

### include-what-you-use

```bash
iwyu_tool.py -p build/
```

不要な `#include` を見つけてコンパイル時間を削減します。

---

## 29.6 CI（継続的インテグレーション）

```yaml
# .github/workflows/ci.yml
name: CI

on: [push, pull_request]

jobs:
  build-and-test:
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
        build_type: [Debug, Release]
        include:
          - os: ubuntu-latest
            build_type: Debug
            sanitize: ON

    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4

      - name: Install Ninja
        uses: seanmiddleditch/gha-setup-ninja@master

      - name: Configure
        run: >
          cmake -S . -B build -G Ninja
          -DCMAKE_BUILD_TYPE=${{ matrix.build_type }}
          -DCHEMCPP_BUILD_TESTS=ON
          -DENABLE_ASAN=${{ matrix.sanitize || 'OFF' }}

      - name: Build
        run: cmake --build build --parallel

      - name: Test
        run: ctest --test-dir build --output-on-failure --parallel

  sanitizers:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        sanitizer: [address, thread, undefined]
    steps:
      - uses: actions/checkout@v4
      - name: Configure
        run: >
          cmake -S . -B build
          -DCMAKE_BUILD_TYPE=Debug
          -DCMAKE_CXX_FLAGS="-fsanitize=${{ matrix.sanitizer }} -g"
          -DCHEMCPP_BUILD_TESTS=ON
      - run: cmake --build build --parallel
      - run: ctest --test-dir build --output-on-failure

  static-analysis:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install tools
        run: sudo apt-get install -y clang-tidy cppcheck
      - name: Configure
        run: cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
      - name: clang-tidy
        run: clang-tidy src/*.cpp -p build/
      - name: cppcheck
        run: cppcheck --enable=warning,performance --error-exitcode=1 src/

  format:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Check formatting
        run: |
          sudo apt-get install -y clang-format
          find src include -name '*.cpp' -o -name '*.hpp' | \
            xargs clang-format --dry-run --Werror
```

**プッシュするたびに、3つのOS × 2つのビルドタイプ × 3つのサニタイザで
検証されます。** これでC++の危険性はかなり下がります。

---

## 29.7 コードカバレッジ

```bash
g++ -std=c++20 -g -O0 --coverage prog.cpp -o prog
./prog

# gcov
gcov prog.cpp

# lcov でHTMLレポート
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/build/_deps/*' -o coverage.info
genhtml coverage.info --output-directory coverage_html
```

CMake:

```cmake
option(ENABLE_COVERAGE "Enable coverage" OFF)
if(ENABLE_COVERAGE AND NOT MSVC)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()
```

> 💡 カバレッジ100%を目指す必要はありません。
> **「重要なパスがテストされているか」**を確認するための道具です。
> エラー処理のパスが全くテストされていない、といった発見に使えます。

---

## 29.8 🧪 実践: テストスイートの全体像

```
tests/
├── CMakeLists.txt
├── test_main.cpp              # Catch2 のメイン（v3では不要）
├── unit/
│   ├── test_smiles.cpp        # パーサの単体テスト
│   ├── test_molecule.cpp      # データ構造
│   ├── test_fingerprint.cpp   # ECFP
│   ├── test_similarity.cpp    # Tanimoto など
│   └── test_descriptors.cpp   # 記述子
├── integration/
│   ├── test_pipeline.cpp      # パース→FP→検索の一連
│   └── test_rdkit_agree.cpp   # RDKitとの一致確認
├── regression/
│   └── test_issues.cpp        # 過去のバグ
├── property/
│   └── test_invariants.cpp    # 不変条件
└── data/
    ├── reference_mw.csv       # RDKitで計算した正解データ
    ├── test_molecules.smi
    └── malformed.smi          # 壊れた入力
```

```cpp
// tests/property/test_invariants.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <chemcpp/smiles.hpp>
#include <chemcpp/fingerprint.hpp>
#include <chemcpp/similarity.hpp>
#include <random>

using namespace chemcpp;
using Catch::Matchers::WithinAbs;

namespace {
Fingerprint random_fp(std::mt19937_64& rng, int nbits) {
    Fingerprint fp{};
    for (int i = 0; i < nbits; ++i) {
        const std::size_t b = rng() % 2048;
        fp[b / 64] |= 1ULL << (b % 64);
    }
    return fp;
}
}

TEST_CASE("similarity metric properties", "[property]") {
    std::mt19937_64 rng(20240101);

    for (int trial = 0; trial < 1000; ++trial) {
        const auto a = random_fp(rng, 10 + int(rng() % 90));
        const auto b = random_fp(rng, 10 + int(rng() % 90));

        const double t  = tanimoto(a, b);
        const double d  = dice(a, b);
        const double c  = cosine(a, b);

        INFO("trial " << trial);

        // 値域
        CHECK(t >= 0.0); CHECK(t <= 1.0);
        CHECK(d >= 0.0); CHECK(d <= 1.0);
        CHECK(c >= 0.0); CHECK(c <= 1.0);

        // 対称性
        CHECK_THAT(t, WithinAbs(tanimoto(b, a), 1e-12));
        CHECK_THAT(d, WithinAbs(dice(b, a),     1e-12));

        // 反射性
        CHECK_THAT(tanimoto(a, a), WithinAbs(1.0, 1e-12));

        // Dice >= Tanimoto （常に成り立つ）
        CHECK(d >= t - 1e-12);

        // Tversky(1,1) == Tanimoto
        CHECK_THAT(tversky(a, b, 1.0, 1.0), WithinAbs(t, 1e-12));
        // Tversky(0.5,0.5) == Dice
        CHECK_THAT(tversky(a, b, 0.5, 0.5), WithinAbs(d, 1e-12));

        // Hamming と popcount の関係
        const int inter = intersection(a, b);
        const int uni   = union_count(a, b);
        CHECK(hamming(a, b) == uni - inter);
        CHECK(popcount(a) + popcount(b) == uni + inter);
    }
}

TEST_CASE("fingerprint is invariant to SMILES form", "[property]") {
    // 同じ分子の異なるSMILES表記
    const std::vector<std::vector<std::string>> equivalent = {
        {"CCO", "OCC", "C(C)O"},
        {"c1ccccc1", "C1=CC=CC=C1"},                 // 芳香族 vs ケクレ
        {"CC(=O)O", "OC(C)=O", "O=C(O)C"},
    };

    for (const auto& group : equivalent) {
        std::optional<Fingerprint> reference;
        for (const auto& smi : group) {
            INFO("SMILES: " << smi);
            auto mol = parse_smiles(smi);
            REQUIRE(mol);
            const auto fp = morgan_fingerprint(*mol);
            if (!reference) reference = fp;
            // ★ ケクレ表記は現状の実装では一致しない（既知の制限）
            // else CHECK(fp == *reference);
        }
    }
}

TEST_CASE("parser never crashes on arbitrary input", "[property][fuzz]") {
    std::mt19937_64 rng(42);
    const std::string alphabet = "CNOScnos()[]=#12345+-@/\\.%HBrCl ";

    for (int trial = 0; trial < 10000; ++trial) {
        const std::size_t len = rng() % 50;
        std::string s;
        for (std::size_t i = 0; i < len; ++i)
            s += alphabet[rng() % alphabet.size()];

        INFO("input: " << s);
        // ★ クラッシュしないことだけを確認（結果は問わない）
        auto mol = parse_smiles(s);
        if (mol) {
            CHECK_NOTHROW(molecular_weight(*mol));
            CHECK_NOTHROW(morgan_fingerprint(*mol));
        }
    }
}
```

**ASan + このテストを組み合わせると、パーサのバグがほぼ全部見つかります。**

---

## 29.9 デバッグの実践的な手順

バグに遭遇したときの手順:

```
① 再現手順を最小化する
   → 100万分子で落ちる → どの1分子で落ちるか特定
   → git bisect でどのコミットから壊れたか特定

② サニタイザを有効にして再実行
   → ASan/UBSan で9割は原因が分かる

③ それでも分からなければデバッガ
   → bt でスタックトレース
   → watch で変数の変化を追う

④ 仮説を立てて、printf/assert で検証
   → 「ここで x は正のはず」を assert で確認

⑤ 修正したら、必ず回帰テストを書く
   → 同じバグを二度踏まない
```

### C++でよくあるバグと症状

| 症状 | 疑うべき原因 |
|---|---|
| **Segmentation fault** | nullptr、範囲外、解放後の使用、スタックオーバーフロー |
| **たまに落ちる** | 未初期化変数、データ競合、ダングリング参照 |
| **Debugでは動くがReleaseで落ちる** | 未定義動作、未初期化変数、strict aliasing |
| **数値が微妙に違う** | 浮動小数点の順序、`-ffast-math`、intの除算 |
| **メモリが増え続ける** | リーク、キャッシュの無制限成長、循環参照 |
| **突然遅くなる** | vector の再確保、キャッシュミス、スワップ |
| **`std::bad_alloc`** | メモリ不足、巨大なサイズ指定（符号の問題） |
| **無限ループ** | 符号なし整数の減算、イテレータの無効化 |

> 💡 **「Debugで動いてReleaseで落ちる」は、ほぼ必ず未定義動作です。**
> 最適化により、未定義動作の結果が変わるためです。
> UBSanを使ってください。

---

## 29.10 この章のまとめ

- **サニタイザがC++で最も価値のあるツール**
  - `-fsanitize=address,undefined` を開発中は常にON
  - 並列コードには `-fsanitize=thread`
- **`-Wall -Wextra` を常に付け、警告をゼロに保つ**
- Catch2 / doctest でテストを書く
  - `SECTION` で独立したセットアップ
  - `GENERATE` でパラメータ化
- **テストの種類:**
  - 既知の正解（ゴールデン）
  - 不変条件（プロパティ）
  - 往復変換（ラウンドトリップ）
  - 参照実装との一致（RDKit）
  - エッジケース、回帰テスト
- ファジングはパーサに有効
- gdb/lldb の `bt`, `p`, `watch` を覚える
- clang-tidy で静的解析を自動化
- **CIで複数OS × サニタイザ × 静的解析**を回す
- **「Debugで動くがReleaseで落ちる」は未定義動作**

> 📝 **練習問題 29-1**
>
> わざとバグのあるコードを書き、ASanが検出することを確認してください。
> ```cpp
> int main() {
>     int* p = new int[10];
>     p[10] = 1;             // 範囲外
>     return 0;              // リークも
> }
> ```

> 📝 **練習問題 29-2**
>
> 第25章のデータ競合のあるコードを、TSanで検出してください。

> 📝 **練習問題 29-3**
>
> 自作のSMILESパーサに対してファジングを行い、
> クラッシュする入力を見つけてください。見つかったら修正し、
> 回帰テストを追加してください。

> 📝 **練習問題 29-4**
>
> あなたのプロジェクトに GitHub Actions の CI を設定し、
> プッシュのたびにテスト + サニタイザが走るようにしてください。

---

**第III部はこれで終わりです。**

ここまでで、C++でケモインフォマティクスのコードを書き、
テストし、最適化し、Pythonから使うまでの全工程をカバーしました。

→ [付録A Python → C++ チートシート](appendix-a-cheatsheet.md)
