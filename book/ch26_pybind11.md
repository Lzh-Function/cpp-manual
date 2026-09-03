# 第26章 Pythonから使う — pybind11 / nanobind

**この章がこの本のクライマックスです。**

C++で書いた高速なコードを、いつものPythonから `import` して使えるようにします。
これができれば、**Pythonの快適さを1ミリも捨てずに、100倍の速度**が手に入ります。

## 26.1 全体像

```
   your_module.cpp  (C++の実装 + バインディング定義)
          │
     [ pybind11 ]
          │
   your_module.so   (Windows なら .pyd)
          │
   >>> import your_module          ← Python から普通に使える
   >>> your_module.tanimoto(a, b)
```

**目標のコード:**

```python
import chemcpp
import numpy as np

# C++ で計算される（100倍速い）
fps = chemcpp.morgan_fingerprints(smiles_list, radius=2, n_bits=2048)  # numpy 配列
hits = chemcpp.search(fps, query_fp, threshold=0.7)

# 既存のPythonコードとシームレスに連携
import pandas as pd
df = pd.DataFrame({"smiles": smiles_list, "score": hits})
```

---

## 26.2 選択肢の比較

| ツール | 特徴 | おすすめ度 |
|---|---|---|
| **pybind11** | 最も普及。情報が豊富。ヘッダオンリー | ★★★ **本章のメイン** |
| **nanobind** | pybind11の作者による後継。**4倍速く、バイナリが小さい** | ★★★ 新規なら検討 |
| **Cython** | Python風の文法。C++の機能は限定的 | ★★☆ |
| **ctypes / cffi** | C API のみ。C++クラスは扱えない | ★☆☆ |
| **SWIG** | 多言語対応。設定が複雑 | ★☆☆ |
| **Python C API 直接** | 最も低レベル。冗長 | ☆ |

> 💡 **pybind11 vs nanobind**
> - pybind11: 情報が多く、枯れている。Python 3.6+ 対応
> - nanobind: **コンパイルが4倍速く、バイナリが5倍小さく、実行も速い**。
>   ただし Python 3.8+、C++17 必須。APIはpybind11とほぼ同じ
>
> RDKit、PyTorch、SciPy が pybind11 を使っています。
> 本章は pybind11 で説明し、nanobind への移行方法も示します。

---

## 26.3 最小の例

### インストール

```bash
pip install pybind11
```

### C++ 側

```cpp
// python/bindings.cpp
#include <pybind11/pybind11.h>

namespace py = pybind11;

int add(int a, int b) { return a + b; }

PYBIND11_MODULE(chemcpp, m) {
    m.doc() = "Fast cheminformatics primitives in C++";
    m.def("add", &add, "Add two numbers");
}
```

### ビルド（1コマンド）

```bash
g++ -O3 -Wall -shared -std=c++20 -fPIC \
    $(python3 -m pybind11 --includes) \
    bindings.cpp -o chemcpp$(python3-config --extension-suffix)
```

### 使う

```python
>>> import chemcpp
>>> chemcpp.add(3, 5)
8
>>> help(chemcpp.add)
add(arg0: int, arg1: int) -> int
    Add two numbers
```

**これだけです。** 拍子抜けするほど簡単。

---

## 26.4 CMake でビルドする（実用版）

```cmake
# python/CMakeLists.txt
include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.13.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pybind11)

pybind11_add_module(_chemcpp bindings.cpp)
target_link_libraries(_chemcpp PRIVATE chemcpp::chemcpp)
target_compile_features(_chemcpp PRIVATE cxx_std_20)

# 最適化を確実に有効化
target_compile_definitions(_chemcpp PRIVATE VERSION_INFO=${PROJECT_VERSION})
if(NOT MSVC)
    target_compile_options(_chemcpp PRIVATE -O3)
endif()
```

```bash
cmake -S . -B build -DCHEMCPP_BUILD_PYTHON=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

生成された `.so` / `.pyd` を Python パスに置けば `import` できます。

### `pyproject.toml` で pip install できるようにする

```toml
# pyproject.toml
[build-system]
requires = ["scikit-build-core>=0.10", "pybind11>=2.13"]
build-backend = "scikit_build_core.build"

[project]
name = "chemcpp"
version = "0.1.0"
description = "Fast cheminformatics primitives"
requires-python = ">=3.9"
dependencies = ["numpy>=1.20"]

[tool.scikit-build]
cmake.version = ">=3.20"
cmake.args = ["-DCHEMCPP_BUILD_PYTHON=ON", "-DCHEMCPP_BUILD_TESTS=OFF"]
wheel.packages = ["python/chemcpp"]
```

```bash
pip install .
# または開発モード
pip install -e .
```

**これで `pip install chemcpp` できるパッケージの完成です。**

---

## 26.5 関数とクラスをバインドする

```cpp
// python/bindings.cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>          // ★ std::vector <-> list の自動変換
#include <pybind11/numpy.h>        // ★ NumPy 連携
#include <pybind11/operators.h>    // 演算子のバインド

#include <chemcpp/smiles.hpp>
#include <chemcpp/fingerprint.hpp>
#include <chemcpp/descriptors.hpp>
#include <chemcpp/similarity.hpp>

namespace py = pybind11;
using namespace chemcpp;

PYBIND11_MODULE(_chemcpp, m) {
    m.doc() = "Fast cheminformatics primitives";

    // ==================== enum ====================
    py::enum_<BondOrder>(m, "BondOrder")
        .value("SINGLE",    BondOrder::Single)
        .value("DOUBLE",    BondOrder::Double)
        .value("TRIPLE",    BondOrder::Triple)
        .value("AROMATIC",  BondOrder::Aromatic)
        .export_values();

    // ==================== struct ====================
    py::class_<Atom>(m, "Atom")
        .def(py::init<>())
        .def_readwrite("atomic_num", &Atom::atomic_num)
        .def_readwrite("charge",     &Atom::charge)
        .def_readwrite("num_h",      &Atom::num_h)
        .def_readwrite("aromatic",   &Atom::aromatic)
        .def_readwrite("in_ring",    &Atom::in_ring)
        .def("__repr__", [](const Atom& a) {
            return "<Atom Z=" + std::to_string(a.atomic_num) +
                   " H=" + std::to_string(a.num_h) + ">";
        });

    py::class_<Descriptors>(m, "Descriptors")
        .def(py::init<>())
        .def_readonly("mw",            &Descriptors::mw)
        .def_readonly("logp",          &Descriptors::logp)
        .def_readonly("tpsa",          &Descriptors::tpsa)
        .def_readonly("hbd",           &Descriptors::hbd)
        .def_readonly("hba",           &Descriptors::hba)
        .def_readonly("rotatable",     &Descriptors::rotatable)
        .def_readonly("n_rings",       &Descriptors::n_rings)
        .def_readonly("heavy_atoms",   &Descriptors::heavy_atoms)
        .def_readonly("fraction_csp3", &Descriptors::fraction_csp3)
        // Python の辞書に変換するヘルパ
        .def("to_dict", [](const Descriptors& d) {
            py::dict out;
            out["mw"]            = d.mw;
            out["logp"]          = d.logp;
            out["tpsa"]          = d.tpsa;
            out["hbd"]           = d.hbd;
            out["hba"]           = d.hba;
            out["rotatable"]     = d.rotatable;
            out["n_rings"]       = d.n_rings;
            out["heavy_atoms"]   = d.heavy_atoms;
            out["fraction_csp3"] = d.fraction_csp3;
            return out;
        })
        .def("__repr__", [](const Descriptors& d) {
            return "<Descriptors MW=" + std::to_string(d.mw) +
                   " logP=" + std::to_string(d.logp) + ">";
        });

    // ==================== class ====================
    py::class_<Molecule>(m, "Molecule")
        .def(py::init<>())
        .def_property_readonly("num_atoms", &Molecule::num_atoms)
        .def_property_readonly("num_bonds", &Molecule::num_bonds)
        .def("atom", py::overload_cast<std::size_t>(&Molecule::atom, py::const_),
             py::return_value_policy::reference_internal)   // ★ 重要（後述）
        .def("degree", &Molecule::degree)
        .def("neighbors", [](const Molecule& m, std::size_t i) {
            std::vector<std::uint32_t> out;
            for (const auto& nb : m.neighbors(i)) out.push_back(nb.atom);
            return out;
        })
        .def("__len__", &Molecule::num_atoms)
        .def("__repr__", [](const Molecule& m) {
            return "<Molecule atoms=" + std::to_string(m.num_atoms()) +
                   " bonds=" + std::to_string(m.num_bonds()) + ">";
        });

    // ==================== 関数 ====================
    m.def("parse_smiles",
          [](const std::string& s) -> py::object {
              auto mol = parse_smiles(s);
              if (!mol) return py::none();          // ★ 失敗時は None を返す
              return py::cast(std::move(*mol));
          },
          py::arg("smiles"),
          "Parse a SMILES string. Returns Molecule or None.");

    m.def("compute_descriptors", &compute_descriptors,
          py::arg("mol"),
          "Compute molecular descriptors");

    m.def("molecular_weight", &molecular_weight, py::arg("mol"));
    m.def("molecular_formula", &molecular_formula, py::arg("mol"));

    // デフォルト引数とキーワード引数
    m.def("morgan_fingerprint",
          [](const Molecule& mol, int radius, int n_bits) {
              MorganOptions opt{.radius = radius, .n_bits = n_bits};
              auto fp = morgan_fingerprint(mol, opt);
              return std::vector<std::uint64_t>(fp.begin(), fp.end());
          },
          py::arg("mol"),
          py::arg("radius") = 2,                    // ★ デフォルト引数
          py::arg("n_bits") = 2048,
          "Compute Morgan (ECFP) fingerprint");
}
```

### Python から

```python
>>> import _chemcpp as cc
>>> mol = cc.parse_smiles("CC(=O)Oc1ccccc1C(=O)O")
>>> mol
<Molecule atoms=13 bonds=13>
>>> len(mol)
13
>>> d = cc.compute_descriptors(mol)
>>> d.mw
180.15899999999999
>>> d.to_dict()
{'mw': 180.159, 'logp': 1.31, 'tpsa': 63.6, 'hbd': 1, 'hba': 3, ...}
>>> cc.parse_smiles("invalid((")
None
>>> cc.morgan_fingerprint(mol, radius=3, n_bits=1024)
[123456789, ...]
```

---

## 26.6 ★NumPy 連携★ — ここが本番

Pythonユーザーが本当に欲しいのは、**NumPy配列で受け渡しできること**です。

### NumPy配列を受け取る

```cpp
#include <pybind11/numpy.h>

// C-contiguous な float 配列を受け取る
double sum_array(py::array_t<double, py::array::c_style | py::array::forcecast> arr) {
    auto buf = arr.request();               // バッファ情報を取得
    const double* ptr = static_cast<const double*>(buf.ptr);

    double sum = 0.0;
    for (py::ssize_t i = 0; i < buf.size; ++i) sum += ptr[i];
    return sum;
}
```

`py::array::forcecast` を付けると、**int配列を渡しても自動的にfloatに変換**されます。

### NumPy配列を返す

```cpp
py::array_t<double> compute_all_mw(const std::vector<std::string>& smiles_list) {
    const std::size_t n = smiles_list.size();
    auto result = py::array_t<double>(static_cast<py::ssize_t>(n));
    auto buf = result.mutable_unchecked<1>();    // ★ 高速アクセス（境界チェックなし）

    for (std::size_t i = 0; i < n; ++i) {
        auto mol = parse_smiles(smiles_list[i]);
        buf(i) = mol ? molecular_weight(*mol) : std::numeric_limits<double>::quiet_NaN();
    }
    return result;
}
```

### 2次元配列（フィンガープリント行列）

```cpp
/// SMILESのリストから、(N, n_bits/8) の uint8 行列を作る
/// RDKit の numpy 変換と互換のあるビット順（bit i は byte i/8 の bit i%8）
py::array_t<std::uint8_t>
fingerprint_matrix(const std::vector<std::string>& smiles_list,
                   int radius = 2, int n_bits = 2048) {
    const std::size_t n     = smiles_list.size();
    const std::size_t nbytes = static_cast<std::size_t>(n_bits) / 8;

    // (n, nbytes) の配列を確保
    auto result = py::array_t<std::uint8_t>({n, nbytes});
    auto buf = result.mutable_unchecked<2>();

    // ゼロ初期化
    std::memset(result.mutable_data(), 0, n * nbytes);

    MorganOptions opt{.radius = radius, .n_bits = n_bits};

    for (std::size_t i = 0; i < n; ++i) {
        auto mol = parse_smiles(smiles_list[i]);
        if (!mol) continue;
        const auto fp = morgan_fingerprint(*mol, opt);

        for (std::size_t bit = 0; bit < static_cast<std::size_t>(n_bits); ++bit) {
            if ((fp[bit / 64] >> (bit % 64)) & 1ULL) {
                buf(i, bit / 8) |= static_cast<std::uint8_t>(1u << (bit % 8));
            }
        }
    }
    return result;
}
```

Python から:

```python
>>> import numpy as np
>>> fps = cc.fingerprint_matrix(smiles_list, radius=2, n_bits=2048)
>>> fps.shape
(1000000, 256)
>>> fps.dtype
dtype('uint8')
>>> np.unpackbits(fps, axis=1).shape       # ビット展開
(1000000, 2048)
```

**これで scikit-learn や PyTorch にそのまま渡せます:**

```python
from sklearn.ensemble import RandomForestClassifier
X = np.unpackbits(fps, axis=1)
clf = RandomForestClassifier().fit(X, y)
```

```python
import torch
X = torch.from_numpy(np.unpackbits(fps, axis=1)).float()
```

### ゼロコピーで返す（上級）

```cpp
py::array_t<double> zero_copy_return(std::vector<double>&& data) {
    // vector をヒープに移し、その所有権を Python に渡す
    auto* vec = new std::vector<double>(std::move(data));

    // Python 側で解放されるときに vector を delete する
    py::capsule cleanup(vec, [](void* p) {
        delete reinterpret_cast<std::vector<double>*>(p);
    });

    return py::array_t<double>(
        {static_cast<py::ssize_t>(vec->size())},   // shape
        {sizeof(double)},                           // strides
        vec->data(),                                // ★ コピーせずポインタを渡す
        cleanup                                     // 解放処理
    );
}
```

**巨大な配列（数GB）を返すときは、これでコピーを回避できます。**

---

## 26.7 ★GILの解放★ — 並列化の鍵

Pythonには GIL（グローバルインタプリタロック）があり、
**同時に1つのスレッドしかPythonコードを実行できません**。

しかし、**C++のコードを実行している間はGILを解放できます**。

```cpp
py::array_t<std::uint8_t>
fingerprint_matrix_parallel(const std::vector<std::string>& smiles_list,
                            int radius = 2, int n_bits = 2048,
                            int n_threads = 0) {
    const std::size_t n      = smiles_list.size();
    const std::size_t nbytes = static_cast<std::size_t>(n_bits) / 8;

    auto result = py::array_t<std::uint8_t>({n, nbytes});
    std::uint8_t* data = result.mutable_data();
    std::memset(data, 0, n * nbytes);

    {
        // ★★★ ここで GIL を解放する ★★★
        py::gil_scoped_release release;

        // この中では Python API を一切呼んではいけない
        // → C++ のデータだけを触る

        unsigned nt = n_threads > 0 ? static_cast<unsigned>(n_threads)
                                    : std::thread::hardware_concurrency();
        if (nt == 0) nt = 4;

        std::atomic<std::size_t> next{0};
        constexpr std::size_t GRAIN = 256;
        MorganOptions opt{.radius = radius, .n_bits = n_bits};

        std::vector<std::jthread> threads;
        for (unsigned t = 0; t < nt; ++t) {
            threads.emplace_back([&] {
                for (;;) {
                    const std::size_t b = next.fetch_add(GRAIN,
                                              std::memory_order_relaxed);
                    if (b >= n) break;
                    const std::size_t e = std::min(b + GRAIN, n);

                    for (std::size_t i = b; i < e; ++i) {
                        auto mol = parse_smiles(smiles_list[i]);
                        if (!mol) continue;
                        const auto fp = morgan_fingerprint(*mol, opt);
                        std::uint8_t* row = data + i * nbytes;
                        for (int bit = 0; bit < n_bits; ++bit) {
                            if ((fp[bit / 64] >> (bit % 64)) & 1ULL) {
                                row[bit / 8] |= static_cast<std::uint8_t>(1u << (bit % 8));
                            }
                        }
                    }
                }
            });
        }
    }   // ★ GIL が再取得される

    return result;
}
```

> ⚠️ **GIL解放中の絶対ルール**
>
> `py::gil_scoped_release` のスコープ内では:
> - **Python オブジェクトに触ってはいけない**（`py::object`, `py::list` など）
> - **例外を Python に投げてはいけない**
> - **`py::print` などを呼んではいけない**
>
> 違反すると**クラッシュします**（しかも再現しにくい形で）。
>
> **対策: GIL解放前にすべてのデータをC++の型に変換しておく。**
> 上の例では `std::vector<std::string>` に変換済みなので安全です。

### 効果

```python
import time
smiles = [...]  # 100万件

t0 = time.time()
fps1 = cc.fingerprint_matrix(smiles)              # シングルスレッド
print(f"single: {time.time()-t0:.2f}s")

t0 = time.time()
fps2 = cc.fingerprint_matrix_parallel(smiles)     # 16スレッド
print(f"parallel: {time.time()-t0:.2f}s")
```

```
single: 5.32s
parallel: 0.41s          ← 13倍
```

**Pythonから呼んでいるのに16コアをフル活用できます。**
これは `multiprocessing` と違い、**プロセス間のデータコピーが不要**です。

> 💡 **これがC++バインディングの最大の価値です。**
> Pythonの `multiprocessing` は:
> - プロセス起動のオーバーヘッド
> - データのpickle化とコピー（100万分子で数GB）
> - メモリ使用量がプロセス数倍
>
> C++ + GIL解放なら、これらが全部なくなります。

---

## 26.8 例外の変換

C++の例外は、自動的にPythonの例外に変換されます。

| C++ | Python |
|---|---|
| `std::exception` | `RuntimeError` |
| `std::runtime_error` | `RuntimeError` |
| `std::invalid_argument` | `ValueError` |
| `std::domain_error` | `ValueError` |
| `std::out_of_range` | `IndexError` |
| `std::length_error` | `ValueError` |
| `std::bad_alloc` | `MemoryError` |
| `py::index_error` | `IndexError` |
| `py::key_error` | `KeyError` |
| `py::stop_iteration` | `StopIteration` |

カスタム例外の登録:

```cpp
PYBIND11_MODULE(_chemcpp, m) {
    py::register_exception<SmilesParseError>(m, "SmilesParseError");
    // または、既存のPython例外にマップ
    py::register_exception_translator([](std::exception_ptr p) {
        try { if (p) std::rethrow_exception(p); }
        catch (const SmilesError& e) {
            PyErr_SetString(PyExc_ValueError, e.what().c_str());
        }
    });
}
```

---

## 26.9 return_value_policy — 所有権の指定

**pybind11 で最もハマるポイントです。**

```cpp
.def("atom", &Molecule::atom, py::return_value_policy::reference_internal)
```

C++のオブジェクトへの参照を返すとき、
**「Pythonがそれを所有するのか、借りているだけなのか」**を指定する必要があります。

| ポリシー | 意味 | いつ使う |
|---|---|---|
| `automatic`（既定） | 戻り値が値ならコピー、ポインタなら所有 | 通常 |
| `copy` | 常にコピー | 安全。小さいオブジェクト |
| `move` | ムーブする | 大きいオブジェクトを返す |
| **`reference_internal`** | 親オブジェクトが生きている限り有効 | **メンバへの参照を返すとき** |
| `reference` | 参照だけ返す（C++側が所有） | 静的オブジェクト |
| `take_ownership` | Pythonが所有し、解放する | `new` したものを返す |

> ⚠️ **間違えると即クラッシュします。**
>
> ```cpp
> // ✗ 危険: mol が消えたら atom も無効なのに、Python は知らない
> .def("atom", &Molecule::atom)
>
> // ✓ reference_internal なら、Python は mol への参照を保持し続ける
> .def("atom", &Molecule::atom, py::return_value_policy::reference_internal)
> ```
>
> `reference_internal` は「**この戻り値が生きている間、親（self）も生かしておく**」
> という指定です。Pythonの参照カウントで管理されます。

---

## 26.10 完全な実装例

```cpp
// python/bindings.cpp （完全版）
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include <chemcpp/smiles.hpp>
#include <chemcpp/fingerprint.hpp>
#include <chemcpp/descriptors.hpp>
#include <chemcpp/database.hpp>
#include <chemcpp/search.hpp>

#include <atomic>
#include <cstring>
#include <thread>

namespace py = pybind11;
using namespace chemcpp;

namespace {

// ---- SMILESリスト → フィンガープリント行列（並列 + GIL解放）----
py::array_t<std::uint8_t>
fp_matrix(const std::vector<std::string>& smiles, int radius, int n_bits,
          int n_threads) {
    const std::size_t n      = smiles.size();
    const std::size_t nbytes = static_cast<std::size_t>(n_bits) / 8;

    auto result = py::array_t<std::uint8_t>(
        {n, nbytes},
        {static_cast<py::ssize_t>(nbytes), static_cast<py::ssize_t>(1)});
    std::uint8_t* data = result.mutable_data();
    std::memset(data, 0, n * nbytes);

    {
        py::gil_scoped_release release;

        unsigned nt = n_threads > 0 ? unsigned(n_threads)
                                    : std::thread::hardware_concurrency();
        if (nt == 0) nt = 4;

        std::atomic<std::size_t> next{0};
        constexpr std::size_t GRAIN = 256;
        const MorganOptions opt{.radius = radius, .n_bits = n_bits};

        std::vector<std::jthread> threads;
        threads.reserve(nt);
        for (unsigned t = 0; t < nt; ++t) {
            threads.emplace_back([&] {
                for (;;) {
                    const std::size_t b = next.fetch_add(GRAIN,
                                              std::memory_order_relaxed);
                    if (b >= n) break;
                    const std::size_t e = std::min(b + GRAIN, n);
                    for (std::size_t i = b; i < e; ++i) {
                        auto mol = parse_smiles(smiles[i]);
                        if (!mol) continue;
                        const auto fp = morgan_fingerprint(*mol, opt);
                        std::uint8_t* row = data + i * nbytes;
                        for (int bit = 0; bit < n_bits; ++bit)
                            if ((fp[bit / 64] >> (bit % 64)) & 1ULL)
                                row[bit / 8] |= std::uint8_t(1u << (bit % 8));
                    }
                }
            });
        }
    }
    return result;
}

// ---- 記述子行列 ----
py::array_t<double>
descriptor_matrix(const std::vector<std::string>& smiles, int n_threads) {
    constexpr std::size_t NDESC = 9;
    const std::size_t n = smiles.size();

    auto result = py::array_t<double>({n, NDESC});
    double* data = result.mutable_data();

    {
        py::gil_scoped_release release;
        unsigned nt = n_threads > 0 ? unsigned(n_threads)
                                    : std::thread::hardware_concurrency();
        if (nt == 0) nt = 4;

        std::atomic<std::size_t> next{0};
        constexpr std::size_t GRAIN = 512;
        const double nan = std::numeric_limits<double>::quiet_NaN();

        std::vector<std::jthread> threads;
        for (unsigned t = 0; t < nt; ++t) {
            threads.emplace_back([&] {
                for (;;) {
                    const std::size_t b = next.fetch_add(GRAIN,
                                              std::memory_order_relaxed);
                    if (b >= n) break;
                    const std::size_t e = std::min(b + GRAIN, n);
                    for (std::size_t i = b; i < e; ++i) {
                        double* row = data + i * NDESC;
                        auto mol = parse_smiles(smiles[i]);
                        if (!mol) { std::fill_n(row, NDESC, nan); continue; }
                        const auto d = compute_descriptors(*mol);
                        row[0] = d.mw;
                        row[1] = d.logp;
                        row[2] = d.tpsa;
                        row[3] = d.hbd;
                        row[4] = d.hba;
                        row[5] = d.rotatable;
                        row[6] = d.n_rings;
                        row[7] = d.heavy_atoms;
                        row[8] = d.fraction_csp3;
                    }
                }
            });
        }
    }
    return result;
}

// ---- Tanimoto 行列（クエリ × データベース）----
py::array_t<float>
tanimoto_matrix(py::array_t<std::uint8_t, py::array::c_style> queries,
                py::array_t<std::uint8_t, py::array::c_style> database,
                int n_threads) {
    auto qbuf = queries.request();
    auto dbuf = database.request();
    if (qbuf.ndim != 2 || dbuf.ndim != 2)
        throw std::invalid_argument("expected 2-D uint8 arrays");
    if (qbuf.shape[1] != dbuf.shape[1])
        throw std::invalid_argument("fingerprint length mismatch");

    const std::size_t nq     = static_cast<std::size_t>(qbuf.shape[0]);
    const std::size_t nd     = static_cast<std::size_t>(dbuf.shape[0]);
    const std::size_t nbytes = static_cast<std::size_t>(qbuf.shape[1]);

    auto result = py::array_t<float>({nq, nd});
    float* out = result.mutable_data();

    const std::uint8_t* qp = static_cast<const std::uint8_t*>(qbuf.ptr);
    const std::uint8_t* dp = static_cast<const std::uint8_t*>(dbuf.ptr);

    {
        py::gil_scoped_release release;

        // popcount を事前計算
        std::vector<int> dcount(nd, 0);
        for (std::size_t j = 0; j < nd; ++j) {
            int c = 0;
            const std::uint8_t* row = dp + j * nbytes;
            for (std::size_t k = 0; k < nbytes; ++k) c += std::popcount(row[k]);
            dcount[j] = c;
        }

        unsigned nt = n_threads > 0 ? unsigned(n_threads)
                                    : std::thread::hardware_concurrency();
        if (nt == 0) nt = 4;

        std::atomic<std::size_t> next{0};
        std::vector<std::jthread> threads;
        for (unsigned t = 0; t < nt; ++t) {
            threads.emplace_back([&] {
                for (;;) {
                    const std::size_t i = next.fetch_add(1,
                                              std::memory_order_relaxed);
                    if (i >= nq) break;

                    const std::uint8_t* q = qp + i * nbytes;
                    int qc = 0;
                    for (std::size_t k = 0; k < nbytes; ++k) qc += std::popcount(q[k]);

                    float* orow = out + i * nd;
                    for (std::size_t j = 0; j < nd; ++j) {
                        const std::uint8_t* d = dp + j * nbytes;
                        int inter = 0;
                        for (std::size_t k = 0; k < nbytes; ++k)
                            inter += std::popcount(static_cast<std::uint8_t>(q[k] & d[k]));
                        const int uni = qc + dcount[j] - inter;
                        orow[j] = uni ? float(inter) / float(uni) : 0.0f;
                    }
                }
            });
        }
    }
    return result;
}

}  // anonymous namespace

PYBIND11_MODULE(_chemcpp, m) {
    m.doc() = R"doc(
        chemcpp: fast cheminformatics primitives implemented in C++.

        All heavy functions release the GIL and use multiple threads,
        so they scale across cores even when called from Python.
    )doc";

    // ---- 基本 ----
    py::class_<Molecule>(m, "Molecule")
        .def_property_readonly("num_atoms", &Molecule::num_atoms)
        .def_property_readonly("num_bonds", &Molecule::num_bonds)
        .def("__len__", &Molecule::num_atoms)
        .def("__repr__", [](const Molecule& mol) {
            return "<Molecule atoms=" + std::to_string(mol.num_atoms()) + ">";
        });

    m.def("parse_smiles", [](const std::string& s) -> py::object {
        auto mol = parse_smiles(s);
        return mol ? py::cast(std::move(*mol)) : py::none();
    }, py::arg("smiles"));

    m.def("molecular_weight", &molecular_weight, py::arg("mol"));
    m.def("molecular_formula", &molecular_formula, py::arg("mol"));

    // ---- バッチ処理（本命）----
    m.def("fingerprint_matrix", &fp_matrix,
          py::arg("smiles"), py::arg("radius") = 2,
          py::arg("n_bits") = 2048, py::arg("n_threads") = 0,
          R"doc(
Compute Morgan fingerprints for a list of SMILES.

Returns
-------
numpy.ndarray of shape (len(smiles), n_bits // 8), dtype=uint8
    Packed bit vectors. Use ``numpy.unpackbits(result, axis=1)``
    to obtain a (N, n_bits) binary matrix.

Notes
-----
Releases the GIL and runs on ``n_threads`` threads (0 = all cores).
)doc");

    m.def("descriptor_matrix", &descriptor_matrix,
          py::arg("smiles"), py::arg("n_threads") = 0,
          R"doc(
Compute molecular descriptors for a list of SMILES.

Returns
-------
numpy.ndarray of shape (N, 9), dtype=float64
    Columns: mw, logp, tpsa, hbd, hba, rotatable, n_rings,
             heavy_atoms, fraction_csp3.
    Rows for unparseable SMILES are filled with NaN.
)doc");

    m.def("tanimoto_matrix", &tanimoto_matrix,
          py::arg("queries"), py::arg("database"), py::arg("n_threads") = 0,
          "Compute the Tanimoto similarity matrix between two packed "
          "fingerprint arrays.");

    m.attr("DESCRIPTOR_NAMES") = py::make_tuple(
        "mw", "logp", "tpsa", "hbd", "hba",
        "rotatable", "n_rings", "heavy_atoms", "fraction_csp3");

    m.attr("__version__") = "0.1.0";
}
```

### Python ラッパを被せる

```python
# python/chemcpp/__init__.py
"""chemcpp: fast cheminformatics primitives implemented in C++."""

from __future__ import annotations

import numpy as np
from . import _chemcpp
from ._chemcpp import (
    Molecule,
    parse_smiles,
    molecular_weight,
    molecular_formula,
    DESCRIPTOR_NAMES,
    __version__,
)

__all__ = [
    "Molecule", "parse_smiles", "molecular_weight", "molecular_formula",
    "fingerprints", "descriptors", "tanimoto_matrix", "DESCRIPTOR_NAMES",
]


def fingerprints(smiles, radius: int = 2, n_bits: int = 2048,
                 n_threads: int = 0, unpack: bool = False) -> np.ndarray:
    """Compute Morgan fingerprints for a sequence of SMILES.

    Parameters
    ----------
    smiles : sequence of str
    radius : int, default 2      (ECFP4)
    n_bits : int, default 2048
    n_threads : int, default 0   (0 = use all cores)
    unpack : bool, default False
        If True, return a (N, n_bits) uint8 matrix of 0/1 instead of
        the packed (N, n_bits // 8) representation.
    """
    packed = _chemcpp.fingerprint_matrix(list(smiles), radius, n_bits, n_threads)
    return np.unpackbits(packed, axis=1) if unpack else packed


def descriptors(smiles, n_threads: int = 0, as_dataframe: bool = False):
    """Compute molecular descriptors. Returns ndarray or pandas DataFrame."""
    arr = _chemcpp.descriptor_matrix(list(smiles), n_threads)
    if not as_dataframe:
        return arr
    import pandas as pd
    return pd.DataFrame(arr, columns=list(DESCRIPTOR_NAMES),
                        index=list(smiles))


def tanimoto_matrix(queries: np.ndarray, database: np.ndarray,
                    n_threads: int = 0) -> np.ndarray:
    """Tanimoto similarity between packed fingerprint matrices."""
    q = np.ascontiguousarray(queries, dtype=np.uint8)
    d = np.ascontiguousarray(database, dtype=np.uint8)
    if q.ndim == 1:
        q = q.reshape(1, -1)
    return _chemcpp.tanimoto_matrix(q, d, n_threads)
```

---

## 26.11 使ってみる

```python
import numpy as np
import pandas as pd
import chemcpp
import time

# 100万件のSMILES
smiles = open("chembl.smi").read().split("\n")[:1_000_000]

# ---- フィンガープリント ----
t0 = time.time()
fps = chemcpp.fingerprints(smiles, radius=2, n_bits=2048)
print(f"fingerprints: {time.time()-t0:.2f}s, shape={fps.shape}")
# fingerprints: 0.62s, shape=(1000000, 256)

# ---- RDKit と比較 ----
from rdkit import Chem
from rdkit.Chem import AllChem

t0 = time.time()
rd_fps = [AllChem.GetMorganFingerprintAsBitVect(Chem.MolFromSmiles(s), 2, 2048)
          for s in smiles[:10000]]
print(f"RDKit (10k): {time.time()-t0:.2f}s")
# RDKit (10k): 3.81s   →  100万件なら 381秒

# ★ 600倍速い

# ---- 記述子 ----
df = chemcpp.descriptors(smiles[:100000], as_dataframe=True)
print(df.describe())

# ---- 類似度検索 ----
query = chemcpp.fingerprints(["CC(=O)Oc1ccccc1C(=O)O"])
t0 = time.time()
sims = chemcpp.tanimoto_matrix(query, fps)
print(f"similarity: {time.time()-t0:.2f}s")
# similarity: 0.18s

top = np.argsort(-sims[0])[:10]
for i in top:
    print(f"{sims[0][i]:.4f}  {smiles[i]}")

# ---- 機械学習にそのまま渡す ----
from sklearn.ensemble import RandomForestRegressor
X = np.unpackbits(fps[:10000], axis=1)
y = np.random.rand(10000)
model = RandomForestRegressor(n_jobs=-1).fit(X, y)

# ---- PyTorch にも ----
import torch
X_torch = torch.from_numpy(np.unpackbits(fps[:10000], axis=1)).float()
```

---

## 26.12 型スタブ (.pyi) — IDE補完のために

```python
# python/chemcpp/_chemcpp.pyi
from typing import Sequence
import numpy as np
import numpy.typing as npt

__version__: str
DESCRIPTOR_NAMES: tuple[str, ...]

class Molecule:
    @property
    def num_atoms(self) -> int: ...
    @property
    def num_bonds(self) -> int: ...
    def __len__(self) -> int: ...

def parse_smiles(smiles: str) -> Molecule | None: ...
def molecular_weight(mol: Molecule) -> float: ...
def molecular_formula(mol: Molecule) -> str: ...

def fingerprint_matrix(
    smiles: Sequence[str],
    radius: int = 2,
    n_bits: int = 2048,
    n_threads: int = 0,
) -> npt.NDArray[np.uint8]: ...

def descriptor_matrix(
    smiles: Sequence[str],
    n_threads: int = 0,
) -> npt.NDArray[np.float64]: ...

def tanimoto_matrix(
    queries: npt.NDArray[np.uint8],
    database: npt.NDArray[np.uint8],
    n_threads: int = 0,
) -> npt.NDArray[np.float32]: ...
```

> 💡 `pybind11-stubgen` で自動生成もできます:
> ```bash
> pip install pybind11-stubgen
> pybind11-stubgen chemcpp -o python/
> ```

---

## 26.13 nanobind への移行

APIがほぼ同じなので、機械的に置換できます。

```cpp
// pybind11
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
namespace py = pybind11;
PYBIND11_MODULE(_chemcpp, m) { ... }

// nanobind
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/ndarray.h>
namespace nb = nanobind;
NB_MODULE(_chemcpp, m) { ... }
```

| pybind11 | nanobind |
|---|---|
| `py::class_` | `nb::class_` |
| `py::array_t<T>` | `nb::ndarray<T, nb::numpy>` |
| `py::gil_scoped_release` | `nb::gil_scoped_release` |
| `py::arg("x") = 1` | `nb::arg("x") = 1` |
| `#include <pybind11/stl.h>` | 型ごとに個別 include |

**効果:**

| 項目 | pybind11 | nanobind |
|---|---|---|
| コンパイル時間 | 100% | **~25%** |
| バイナリサイズ | 100% | **~20%** |
| 関数呼び出しオーバーヘッド | 100% | **~30%** |

大規模なバインディングでは劇的な差になります。

---

## 26.14 トラブルシューティング

### `ImportError: dynamic module does not define module export function`

モジュール名が一致していません。

```cpp
PYBIND11_MODULE(_chemcpp, m)   // ← ファイル名 _chemcpp.so と一致する必要がある
```

### `ImportError: undefined symbol: _ZN...`

C++ライブラリのリンク漏れです。

```cmake
target_link_libraries(_chemcpp PRIVATE chemcpp::chemcpp)
```

または、静的ライブラリを `-fPIC` でビルドしていない:

```cmake
set_property(TARGET chemcpp PROPERTY POSITION_INDEPENDENT_CODE ON)
```

### セグフォする

99%は所有権の問題です:
- `return_value_policy` を確認
- GIL解放中にPythonオブジェクトを触っていないか
- 参照を返している先が生きているか

### 遅い

- **`-O3` でビルドしているか確認**（Debugビルドは10倍遅い）
- **1要素ずつ呼んでいないか**（関数呼び出しのオーバーヘッドが支配的）
  ```python
  # ✗ 遅い: 100万回の関数呼び出し
  fps = [cc.morgan_fingerprint(cc.parse_smiles(s)) for s in smiles]

  # ✓ 速い: 1回の呼び出しでバッチ処理
  fps = cc.fingerprint_matrix(smiles)
  ```
- **GILを解放しているか**

> 💡 **バインディング設計の鉄則: バッチAPIを提供する。**
> Python ↔ C++ の境界を越えるコストは、1回あたり数マイクロ秒です。
> 100万回越えると数秒かかります。**1回で全部渡してください。**

---

## 26.15 この章のまとめ

- **pybind11 でC++の関数・クラスをPythonに公開できる**
- `pybind11_add_module` + `pyproject.toml` で `pip install` 可能なパッケージになる
- **NumPy配列で受け渡す**（`py::array_t`）のが実用上の要
- **`py::gil_scoped_release` でGILを解放**すれば、Pythonから呼んでもマルチコアが使える
  - GIL解放中はPythonオブジェクトに触らない
- `return_value_policy` を正しく指定する（`reference_internal` が頻出）
- C++の例外は自動的にPythonの例外に変換される
- **バッチAPIを提供する**（1要素ずつ呼ぶと境界越えのコストが支配的）
- `.pyi` スタブでIDE補完を提供する
- nanobind はほぼ同じAPIで、コンパイル4倍速・バイナリ5倍小

### 到達点

```python
import chemcpp
fps = chemcpp.fingerprints(million_smiles)     # 0.6秒（RDKit: 380秒）
```

**Pythonの書き心地のまま、600倍。** これがこの本のゴールです。

> 📝 **練習問題 26-1**
>
> 26.10 のバインディングを実際にビルドし、
> RDKitと速度・結果を比較してください。

> 📝 **練習問題 26-2**
>
> `search_topk` をバインドして、
> Python から `chemcpp.search(fps, query, k=100)` と呼べるようにしてください。

> 📝 **練習問題 26-3**
>
> GIL を解放する版としない版で、
> Python の `ThreadPoolExecutor` から呼んだときの性能を比較してください。

> 📝 **練習問題 26-4**
>
> nanobind に移植し、コンパイル時間とバイナリサイズを比較してください。

---

→ [第27章 RDKitのC++ API](ch27_RDKit_CppAPI.md)
