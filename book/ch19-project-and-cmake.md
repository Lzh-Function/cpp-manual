# 第19章 プロジェクトを作る — CMake とディレクトリ構成

第III部へようこそ。ここからは実際に動くプロジェクトを作っていきます。
まずは土台となるビルド環境からです。

## 19.1 なぜCMakeが必要なのか

ファイルが3つを超えたら、`g++ a.cpp b.cpp c.cpp -o prog` は限界です。

- どのファイルが変更されたか手で管理できない
- Windows / Linux / macOS でコマンドが違う
- ライブラリのパスを毎回書くのは無理
- デバッグビルドとリリースビルドを切り替えたい
- IDEに読み込ませたい

**CMake は「ビルド手順の設計図」を書くツール**です。
CMake自体はビルドせず、Makefile や Ninja、Visual Studio のプロジェクトを**生成**します。

```
  CMakeLists.txt  ──[cmake]──>  build.ninja / Makefile / .vcxproj
                                        │
                                  [ninja / make / msbuild]
                                        │
                                        ▼
                                   実行ファイル
```

> 🐍 **Pythonとの対比**
> `pyproject.toml` に相当します。
> 「何をビルドし、何に依存するか」を宣言的に書きます。

---

## 19.2 最小のCMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(chemcpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)              # -std=c++20 であって -std=gnu++20 ではない
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)      # clangd 用の compile_commands.json を出す

add_executable(hello src/main.cpp)
```

ビルド:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/hello
```

| コマンド | 意味 |
|---|---|
| `cmake -S . -B build` | 設定（configure）。`build/` に生成物を置く |
| `cmake --build build` | ビルド |
| `-j` | 並列ビルド（コア数分） |
| `-G Ninja` | ジェネレータをNinjaに（**速いのでおすすめ**） |
| `-DCMAKE_BUILD_TYPE=Release` | 最適化ON |

### ビルドタイプ

| タイプ | 付くフラグ | 用途 |
|---|---|---|
| `Debug` | `-g -O0` | デバッグ |
| `Release` | `-O3 -DNDEBUG` | 本番 |
| `RelWithDebInfo` | `-O2 -g -DNDEBUG` | **プロファイリング（おすすめ）** |
| `MinSizeRel` | `-Os -DNDEBUG` | サイズ最小化 |

> ⚠️ **ビルドタイプを指定し忘れると、最適化なしでビルドされます**（Makefile系の場合）。
> 「C++にしたのに遅い」の原因第2位です。必ず指定してください。

---

## 19.3 実用的なプロジェクト構成

```
chemcpp/
├── CMakeLists.txt              # トップレベル
├── README.md
├── .clang-format               # コード整形の設定
├── .gitignore
├── include/
│   └── chemcpp/                # ★ 名前空間ごとのディレクトリ
│       ├── molecule.hpp
│       ├── smiles.hpp
│       ├── fingerprint.hpp
│       ├── similarity.hpp
│       └── descriptors.hpp
├── src/
│   ├── molecule.cpp
│   ├── smiles.cpp
│   ├── fingerprint.cpp
│   ├── similarity.cpp
│   └── descriptors.cpp
├── apps/
│   ├── CMakeLists.txt
│   ├── screen.cpp              # 実行ファイル1: 類似度検索
│   └── descriptors_cli.cpp     # 実行ファイル2: 記述子計算
├── tests/
│   ├── CMakeLists.txt
│   ├── test_smiles.cpp
│   ├── test_fingerprint.cpp
│   └── test_similarity.cpp
├── bench/
│   ├── CMakeLists.txt
│   └── bench_tanimoto.cpp
├── python/                     # pybind11 バインディング（第26章）
│   ├── CMakeLists.txt
│   └── bindings.cpp
└── data/
    └── test_molecules.smi
```

> 💡 **`include/chemcpp/` という2段構成にする理由**
>
> ```cpp
> #include <chemcpp/molecule.hpp>    // ★ どのライブラリのヘッダか一目で分かる
> #include <molecule.hpp>            // ✗ 他のライブラリと衝突するかも
> ```
> インクルードパスに `include/` を追加すれば、上のように書けます。
> RDKit も `#include <GraphMol/ROMol.h>` という形式です。

---

## 19.4 ライブラリとして構成する

トップレベルの `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(chemcpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# デフォルトのビルドタイプ（指定し忘れ対策）
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(CHEMCPP_BUILD_TESTS  "Build tests"      ON)
option(CHEMCPP_BUILD_BENCH  "Build benchmarks" OFF)
option(CHEMCPP_BUILD_PYTHON "Build Python bindings" OFF)

# ---------- ライブラリ本体 ----------
add_library(chemcpp
    src/molecule.cpp
    src/smiles.cpp
    src/fingerprint.cpp
    src/similarity.cpp
    src/descriptors.cpp
)
add_library(chemcpp::chemcpp ALIAS chemcpp)     # 名前空間付きエイリアス

target_include_directories(chemcpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(chemcpp PUBLIC cxx_std_20)

# 警告設定（コンパイラごとに分岐）
if(MSVC)
    target_compile_options(chemcpp PRIVATE /W4)
else()
    target_compile_options(chemcpp PRIVATE
        -Wall -Wextra -Wpedantic
        -Wshadow                    # 変数の隠蔽
        -Wconversion                # 暗黙の型変換
        -Wnon-virtual-dtor          # 仮想デストラクタ忘れ
    )
endif()

# ---------- サブディレクトリ ----------
add_subdirectory(apps)

if(CHEMCPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CHEMCPP_BUILD_BENCH)
    add_subdirectory(bench)
endif()

if(CHEMCPP_BUILD_PYTHON)
    add_subdirectory(python)
endif()
```

`apps/CMakeLists.txt`:

```cmake
add_executable(screen screen.cpp)
target_link_libraries(screen PRIVATE chemcpp::chemcpp)

add_executable(descriptors_cli descriptors_cli.cpp)
target_link_libraries(descriptors_cli PRIVATE chemcpp::chemcpp)
```

**たったこれだけ**で、`chemcpp` ライブラリの include パスと
コンパイルオプションが自動的に `screen` にも伝わります。

---

## 19.5 ★重要★ PUBLIC / PRIVATE / INTERFACE

CMakeで最も混乱するポイントです。

```cmake
target_link_libraries(mylib PUBLIC    fmt::fmt)     # mylib もその利用者も使う
target_link_libraries(mylib PRIVATE   zlib)         # mylib の内部だけで使う
target_link_libraries(mylib INTERFACE header_only)  # 利用者だけが使う
```

判定法:

| ヘッダに出てくる? | 実装で使う? | 指定 |
|---|---|---|
| Yes | Yes | **PUBLIC** |
| No | Yes | **PRIVATE** |
| Yes | No | **INTERFACE** |

```cpp
// molecule.hpp （公開ヘッダ）
#include <fmt/format.h>        // ← ヘッダに出てくる → fmt は PUBLIC

// molecule.cpp （実装）
#include <zlib.h>              // ← 実装だけ → zlib は PRIVATE
```

> 💡 **迷ったら PRIVATE。** 依存を最小限にするのが良い設計です。
> PUBLIC にすると、利用者にも依存が伝播します。

---

## 19.6 外部ライブラリの導入

### 方法1: FetchContent（推奨。pip install 感覚）

```cmake
include(FetchContent)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        11.0.2                 # ★ 必ずタグを固定する
    GIT_SHALLOW    TRUE                   # 履歴をDLしない（速い）
)
FetchContent_MakeAvailable(fmt)

target_link_libraries(chemcpp PUBLIC fmt::fmt)
```

初回の `cmake -S . -B build` でGitHubから自動DLされます。

> ⚠️ `GIT_TAG main` にしないでください。ビルドの再現性が失われます。
> 必ずタグかコミットハッシュを指定します。

### 方法2: find_package（システムにインストール済みのもの）

```cmake
find_package(Threads REQUIRED)
find_package(OpenMP)
find_package(Boost 1.75 REQUIRED COMPONENTS system filesystem)

target_link_libraries(chemcpp PUBLIC Threads::Threads)
if(OpenMP_CXX_FOUND)
    target_link_libraries(chemcpp PUBLIC OpenMP::OpenMP_CXX)
endif()
```

### 方法3: ヘッダオンリーを直接置く

```
third_party/
└── nlohmann/
    └── json.hpp        # ← 1ファイルコピーするだけ
```

```cmake
target_include_directories(chemcpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
```

**最も簡単で確実**です。小さなライブラリならこれで十分。

### よく使うライブラリ一覧

| ライブラリ | 用途 | 導入 |
|---|---|---|
| [fmt](https://github.com/fmtlib/fmt) | 書式化（`std::format` の代替） | FetchContent |
| [Catch2](https://github.com/catchorg/Catch2) | テスト | FetchContent |
| [doctest](https://github.com/doctest/doctest) | テスト（超軽量、1ヘッダ） | コピー |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON | コピー（1ヘッダ） |
| [Eigen](https://eigen.tuxfamily.org/) | 線形代数（NumPy相当） | FetchContent / find_package |
| [pybind11](https://github.com/pybind/pybind11) | Pythonバインディング | FetchContent |
| [nanobind](https://github.com/wjakob/nanobind) | 同上（より軽量・高速） | FetchContent |
| [spdlog](https://github.com/gabime/spdlog) | ロギング | FetchContent |
| [CLI11](https://github.com/CLIUtils/CLI11) | コマンドライン引数（argparse相当） | コピー（1ヘッダ） |
| [xsimd](https://github.com/xtensor-stack/xsimd) | SIMD抽象化 | FetchContent |
| [Google Benchmark](https://github.com/google/benchmark) | ベンチマーク | FetchContent |
| [RDKit](https://github.com/rdkit/rdkit) | ケモインフォマティクス | conda / find_package |

---

## 19.7 テストの組み込み

`tests/CMakeLists.txt`:

```cmake
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(chemcpp_tests
    test_smiles.cpp
    test_fingerprint.cpp
    test_similarity.cpp
)
target_link_libraries(chemcpp_tests PRIVATE chemcpp::chemcpp Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(chemcpp_tests)      # テストを CTest に自動登録
```

実行:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

テストの書き方は第29章で詳しくやります。

---

## 19.8 便利な設定いろいろ

### サニタイザを簡単に有効化する

```cmake
option(CHEMCPP_SANITIZE "Enable sanitizers" OFF)

if(CHEMCPP_SANITIZE AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()
```

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCHEMCPP_SANITIZE=ON
```

### ネイティブ最適化（実験用）

```cmake
option(CHEMCPP_NATIVE "Optimize for this machine" OFF)

if(CHEMCPP_NATIVE AND NOT MSVC)
    target_compile_options(chemcpp PRIVATE -march=native)
endif()
```

> ⚠️ `-march=native` でビルドしたバイナリは、**別のCPUで動きません**
> （Illegal instruction でクラッシュ）。配布用には使わないでください。
> 手元での性能実験には非常に有効です（AVX2/AVX-512 が有効になる）。

### LTO（リンク時最適化）

```cmake
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_ok)
if(ipo_ok)
    set_property(TARGET chemcpp PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()
```

ファイルをまたいだインライン展開ができるようになり、数％〜数十％速くなります。
その代わりリンクが遅くなります。

### ccache でリビルドを高速化

```cmake
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()
```

同じファイルの再コンパイルをキャッシュから返します。**劇的に速くなります。**

### CMakePresets.json（設定を保存）

```json
{
  "version": 4,
  "configurePresets": [
    {
      "name": "release",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CHEMCPP_BUILD_TESTS": "ON"
      }
    },
    {
      "name": "debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CHEMCPP_SANITIZE": "ON"
      }
    },
    {
      "name": "bench",
      "inherits": "release",
      "binaryDir": "${sourceDir}/build/bench",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "RelWithDebInfo",
        "CHEMCPP_NATIVE": "ON",
        "CHEMCPP_BUILD_BENCH": "ON"
      }
    }
  ],
  "buildPresets": [
    { "name": "release", "configurePreset": "release" },
    { "name": "debug",   "configurePreset": "debug" },
    { "name": "bench",   "configurePreset": "bench" }
  ]
}
```

```bash
cmake --preset release
cmake --build --preset release
```

**これは本当に便利です。** チームで設定を共有でき、
VSCode の CMake Tools も認識してくれます。

---

## 19.9 ヘッダの書き方（実践）

```cpp
// include/chemcpp/fingerprint.hpp
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace chemcpp {                 // ★ 名前空間で囲む

inline constexpr std::size_t DEFAULT_FP_BITS  = 2048;
inline constexpr std::size_t DEFAULT_FP_WORDS = DEFAULT_FP_BITS / 64;

using Fingerprint = std::array<std::uint64_t, DEFAULT_FP_WORDS>;

struct MorganOptions {
    int  radius        = 2;
    int  n_bits        = static_cast<int>(DEFAULT_FP_BITS);
    bool use_features   = false;
    bool use_chirality  = false;
};

/// SMILES から Morgan フィンガープリントを計算する。
/// @param smiles 入力SMILES
/// @param opt    オプション
/// @return 失敗時は全ビット0のフィンガープリント
[[nodiscard]] Fingerprint morgan_fingerprint(std::string_view smiles,
                                             const MorganOptions& opt = {});

[[nodiscard]] int popcount(const Fingerprint& fp) noexcept;

[[nodiscard]] double tanimoto(const Fingerprint& a, const Fingerprint& b) noexcept;

}  // namespace chemcpp
```

**ポイント:**
- `#pragma once`
- **名前空間で囲む**（グローバル名前空間を汚染しない）
- `inline constexpr` で定数をヘッダに書ける（C++17）
- `[[nodiscard]]` で戻り値の無視を防ぐ
- Doxygen形式のコメント
- **必要最小限の `#include`**（コンパイル時間に直結）

### 名前空間の使い方

```cpp
namespace chemcpp {
namespace detail {              // 内部実装用（利用者は使わない）
    int helper();
}
}

// C++17 なら入れ子を短く書ける
namespace chemcpp::detail {
    int helper();
}
```

利用側:

```cpp
#include <chemcpp/fingerprint.hpp>

int main() {
    auto fp = chemcpp::morgan_fingerprint("CCO");

    // 長いなら短縮エイリアス
    namespace cc = chemcpp;
    auto fp2 = cc::morgan_fingerprint("CCC");

    // 関数の中でだけ using
    using chemcpp::tanimoto;
    double t = tanimoto(fp, fp2);
}
```

---

## 19.10 .clang-format — コード整形

```yaml
# .clang-format
---
Language: Cpp
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
AccessModifierOffset: -4
PointerAlignment: Left            # int* p （int *p ではなく）
AllowShortFunctionsOnASingleLine: Inline
BreakBeforeBraces: Attach
SortIncludes: CaseSensitive
IncludeBlocks: Regroup
NamespaceIndentation: None
```

```bash
clang-format -i src/*.cpp include/chemcpp/*.hpp
```

VSCodeなら保存時に自動整形できます:

```json
// .vscode/settings.json
{
  "editor.formatOnSave": true,
  "[cpp]": { "editor.defaultFormatter": "llvm-vs-code-extensions.vscode-clangd" }
}
```

> 💡 **整形はツールに任せてください。** インデントやスペースの議論は
> 生産性の無駄です。`.clang-format` を1つ決めて、それに従うのが最善です。

---

## 19.11 .gitignore

```text
build/
build-*/
cmake-build-*/
*.o
*.so
*.a
*.exe
*.dll
compile_commands.json
.cache/
CMakeUserPresets.json
__pycache__/
*.pyc
```

---

## 19.12 🧪 実践: プロジェクトを一から作る

コピペで動く最小構成です。

```bash
mkdir -p chemcpp/{include/chemcpp,src,apps,tests}
cd chemcpp
```

`include/chemcpp/similarity.hpp`:

```cpp
#pragma once
#include <array>
#include <cstdint>

namespace chemcpp {

inline constexpr std::size_t FP_WORDS = 32;
using Fingerprint = std::array<std::uint64_t, FP_WORDS>;

[[nodiscard]] int    popcount(const Fingerprint& fp) noexcept;
[[nodiscard]] double tanimoto(const Fingerprint& a, const Fingerprint& b) noexcept;
[[nodiscard]] double dice(const Fingerprint& a, const Fingerprint& b) noexcept;

}  // namespace chemcpp
```

`src/similarity.cpp`:

```cpp
#include <chemcpp/similarity.hpp>
#include <bit>

namespace chemcpp {

int popcount(const Fingerprint& fp) noexcept {
    int n = 0;
    for (std::uint64_t w : fp) n += std::popcount(w);
    return n;
}

double tanimoto(const Fingerprint& a, const Fingerprint& b) noexcept {
    int inter = 0, uni = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        inter += std::popcount(a[i] & b[i]);
        uni   += std::popcount(a[i] | b[i]);
    }
    return uni ? static_cast<double>(inter) / uni : 0.0;
}

double dice(const Fingerprint& a, const Fingerprint& b) noexcept {
    int inter = 0, total = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        inter += std::popcount(a[i] & b[i]);
        total += std::popcount(a[i]) + std::popcount(b[i]);
    }
    return total ? 2.0 * inter / total : 0.0;
}

}  // namespace chemcpp
```

`apps/demo.cpp`:

```cpp
#include <chemcpp/similarity.hpp>
#include <iostream>

int main() {
    chemcpp::Fingerprint a{}, b{};
    for (std::size_t i = 0; i < 2048; i += 3) a[i/64] |= 1ULL << (i%64);
    for (std::size_t i = 0; i < 2048; i += 5) b[i/64] |= 1ULL << (i%64);

    std::cout << "|a| = " << chemcpp::popcount(a) << "\n";
    std::cout << "|b| = " << chemcpp::popcount(b) << "\n";
    std::cout << "tanimoto = " << chemcpp::tanimoto(a, b) << "\n";
    std::cout << "dice     = " << chemcpp::dice(a, b) << "\n";
}
```

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(chemcpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_library(chemcpp src/similarity.cpp)
add_library(chemcpp::chemcpp ALIAS chemcpp)
target_include_directories(chemcpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(chemcpp PUBLIC cxx_std_20)

if(NOT MSVC)
  target_compile_options(chemcpp PRIVATE -Wall -Wextra)
endif()

add_executable(demo apps/demo.cpp)
target_link_libraries(demo PRIVATE chemcpp::chemcpp)
```

ビルド・実行:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/demo
```

```
|a| = 683
|b| = 410
tanimoto = 0.126
dice     = 0.224
```

**これがC++プロジェクトの標準形です。** 以降の章はこの構成を前提にします。

---

## 19.13 トラブルシューティング

### `CMake Error: Could not find a package configuration file provided by "X"`

`find_package(X)` が失敗しています。
- ライブラリがインストールされているか確認
- `CMAKE_PREFIX_PATH` を指定: `cmake -DCMAKE_PREFIX_PATH=/path/to/lib`
- conda環境なら: `-DCMAKE_PREFIX_PATH=$CONDA_PREFIX`

### 変更したのに再ビルドされない

```bash
rm -rf build
cmake -S . -B build ...
```

CMakeのキャッシュが古い可能性があります。**困ったら `build/` を消す**。

### clangd がヘッダを見つけられない

`CMAKE_EXPORT_COMPILE_COMMANDS ON` を設定し、
`compile_commands.json` をプロジェクトルートにリンクします:

```bash
ln -sf build/compile_commands.json .
# Windows なら
cmake -E copy build/compile_commands.json .
```

### リンクエラー `undefined reference`

- `.cpp` ファイルを `add_library` / `add_executable` に追加し忘れ
- `target_link_libraries` を忘れ
- 宣言と定義のシグネチャ不一致（`const` の有無など）

---

## 19.14 この章のまとめ

- CMakeは**ビルド手順の設計図**。Makefile や Ninja を生成する
- 基本コマンド:
  ```bash
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j
  ```
- **ビルドタイプの指定を忘れない**（`Release` / `RelWithDebInfo`）
- `add_library` + `target_link_libraries` でモジュール化
- **PUBLIC / PRIVATE** は「ヘッダに出てくるか」で判断
- 外部ライブラリは **FetchContent** が最も楽
- `include/プロジェクト名/` の2段構成にする
- **CMakePresets.json** で設定を共有
- `.clang-format` で整形を自動化
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` で clangd が動く

> 📝 **練習問題 19-1**
>
> 19.12 のプロジェクトを実際に作り、ビルドしてください。
> その後、`dice` の実装にわざとタイポを入れて、
> どんなエラーが出るか確認してください。

> 📝 **練習問題 19-2**
>
> FetchContent で fmt を導入し、`demo.cpp` の出力を
> `fmt::print("{:.4f}\n", tanimoto(a,b))` に書き換えてください。

> 📝 **練習問題 19-3**
>
> CMakePresets.json を作り、`debug`（サニタイザ有効）と
> `release` の2つのプリセットを定義してください。

---

→ [第20章 実践1: SMILESパーサ](ch20-smiles-parser.md)
