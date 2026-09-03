# 第27章 RDKit の C++ API を使う

自作するのも学びですが、実務では既存の資産を使うのが正解です。
**RDKit は C++ で書かれており、Python API はその薄いラッパー**にすぎません。

C++ から直接使えば、Python層のオーバーヘッドが消えます。

## 27.1 なぜ RDKit を C++ から使うのか

| 理由 | 説明 |
|---|---|
| **速い** | Python のオブジェクト生成・GIL のオーバーヘッドがない |
| **並列化できる** | GILの制約なしに全コアを使える |
| **メモリ効率** | Python オブジェクトのラッパーが不要 |
| **完全な機能** | Python API に露出していない機能も使える |
| **既存C++コードとの統合** | シミュレーションコードなどに組み込める |

典型的には **Python API の3〜10倍**の速度が出ます。
マルチスレッドと組み合わせれば、さらに数十倍。

---

## 27.2 インストール

### 方法1: conda（最も簡単・推奨）

```bash
mamba create -n rdkit-cpp -c conda-forge \
    rdkit-dev cmake ninja cxx-compiler boost-cpp eigen
mamba activate rdkit-cpp
```

`rdkit-dev` にはC++のヘッダ (`include/rdkit/`) と共有ライブラリが入っています。

```bash
ls $CONDA_PREFIX/include/rdkit/GraphMol/
# ROMol.h  RWMol.h  Atom.h  Bond.h  SmilesParse/  Descriptors/  Fingerprints/ ...
```

### 方法2: Linux のパッケージ

```bash
sudo apt install librdkit-dev libboost-all-dev    # Debian/Ubuntu
```

（バージョンが古いことが多いです）

### 方法3: ソースからビルド

```bash
git clone https://github.com/rdkit/rdkit.git
cd rdkit && mkdir build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DRDK_BUILD_PYTHON_WRAPPER=OFF \
    -DRDK_BUILD_INCHI_SUPPORT=ON \
    -DRDK_INSTALL_INTREE=OFF \
    -DCMAKE_INSTALL_PREFIX=$HOME/.local
ninja -j$(nproc)
ninja install
```

ビルドには30分〜2時間かかります。Boost が必要です。

---

## 27.3 CMake の設定

```cmake
cmake_minimum_required(VERSION 3.20)
project(rdkit_demo CXX)

set(CMAKE_CXX_STANDARD 17)          # ★ RDKit は C++17 でビルドされている
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# conda 環境を見つける
if(DEFINED ENV{CONDA_PREFIX})
    list(APPEND CMAKE_PREFIX_PATH $ENV{CONDA_PREFIX})
    message(STATUS "Using conda prefix: $ENV{CONDA_PREFIX}")
endif()

find_package(RDKit REQUIRED)
find_package(Boost REQUIRED COMPONENTS system serialization iostreams)

add_executable(demo main.cpp)

target_link_libraries(demo PRIVATE
    RDKit::GraphMol
    RDKit::SmilesParse
    RDKit::FileParsers
    RDKit::Descriptors
    RDKit::Fingerprints
    RDKit::MolTransforms
    RDKit::SubstructMatch
    RDKit::DataStructs
    RDKit::RDGeneral
    Boost::system
)
```

> ⚠️ **RDKit のライブラリは細かく分かれています。**
> 使う機能に応じて `target_link_libraries` に追加してください。
> リンクエラーが出たら、対応するライブラリを探して追加します。
>
> 主なライブラリ:
> - `RDKit::GraphMol` — 分子の基本クラス（必須）
> - `RDKit::SmilesParse` — SMILES/SMARTS のパースと生成
> - `RDKit::FileParsers` — SDF/Mol/PDB などのI/O
> - `RDKit::Descriptors` — 記述子計算
> - `RDKit::Fingerprints` — フィンガープリント
> - `RDKit::SubstructMatch` — 部分構造検索
> - `RDKit::MolStandardize` — 構造の標準化
> - `RDKit::ForceFieldHelpers` — 力場計算
> - `RDKit::DistGeomHelpers` — 3D座標生成

> ⚠️ **ABI 互換性に注意**
> RDKit は特定のコンパイラ・C++標準でビルドされています。
> **同じコンパイラ、同じ標準（C++17）でビルドしてください。**
> C++20 でビルドすると、稀にリンクエラーや実行時クラッシュが起きます。

---

## 27.4 基本操作

```cpp
// main.cpp
#include <GraphMol/GraphMol.h>
#include <GraphMol/SmilesParse/SmilesParse.h>
#include <GraphMol/SmilesParse/SmilesWrite.h>
#include <GraphMol/Descriptors/MolDescriptors.h>
#include <GraphMol/Descriptors/Crippen.h>

#include <iostream>
#include <memory>

int main() {
    // ---- SMILES のパース ----
    // ★ 戻り値は生ポインタ。unique_ptr で受けるのが安全
    std::unique_ptr<RDKit::ROMol> mol(RDKit::SmilesToMol("CC(=O)Oc1ccccc1C(=O)O"));

    if (!mol) {
        std::cerr << "parse failed\n";
        return 1;
    }

    std::cout << "atoms: " << mol->getNumAtoms() << "\n";
    std::cout << "bonds: " << mol->getNumBonds() << "\n";
    std::cout << "rings: " << mol->getRingInfo()->numRings() << "\n";

    // ---- 記述子 ----
    std::cout << "MW    : " << RDKit::Descriptors::calcExactMW(*mol) << "\n";
    std::cout << "LogP  : " << RDKit::Descriptors::calcClogP(*mol) << "\n";
    std::cout << "TPSA  : " << RDKit::Descriptors::calcTPSA(*mol) << "\n";
    std::cout << "HBD   : " << RDKit::Descriptors::calcNumHBD(*mol) << "\n";
    std::cout << "HBA   : " << RDKit::Descriptors::calcNumHBA(*mol) << "\n";
    std::cout << "RotB  : " << RDKit::Descriptors::calcNumRotatableBonds(*mol) << "\n";
    std::cout << "Rings : " << RDKit::Descriptors::calcNumRings(*mol) << "\n";
    std::cout << "Formula: " << RDKit::Descriptors::calcMolFormula(*mol) << "\n";

    // ---- 正準SMILES ----
    std::cout << "canonical: " << RDKit::MolToSmiles(*mol) << "\n";

    // ---- 原子の走査 ----
    for (const auto atom : mol->atoms()) {              // C++11 range-based for
        std::cout << atom->getIdx() << ": "
                  << atom->getSymbol()
                  << " deg=" << atom->getDegree()
                  << " H=" << atom->getTotalNumHs()
                  << " arom=" << atom->getIsAromatic()
                  << " ring=" << mol->getRingInfo()->numAtomRings(atom->getIdx())
                  << "\n";
    }

    // ---- 結合の走査 ----
    for (const auto bond : mol->bonds()) {
        std::cout << bond->getBeginAtomIdx() << "-" << bond->getEndAtomIdx()
                  << " order=" << bond->getBondTypeAsDouble()
                  << " arom=" << bond->getIsAromatic() << "\n";
    }

    // ---- 隣接原子の走査 ----
    const RDKit::Atom* a0 = mol->getAtomWithIdx(0);
    for (const auto nbr : mol->atomNeighbors(a0)) {
        std::cout << "neighbor of 0: " << nbr->getIdx() << "\n";
    }

    return 0;
}
```

### ★ 所有権の注意

```cpp
// ✗ 危険: 解放し忘れる
RDKit::ROMol* mol = RDKit::SmilesToMol("CCO");
// ... delete mol; を忘れるとリーク

// ✓ 安全: unique_ptr で受ける（第13章）
std::unique_ptr<RDKit::ROMol> mol(RDKit::SmilesToMol("CCO"));
// スコープを抜けたら自動的に解放
```

**RDKit の C++ API は生ポインタを返す古いスタイル**です。
必ず `unique_ptr` か `shared_ptr` で受けてください。

```cpp
// RDKit には ROMOL_SPTR という型エイリアスもある
using ROMOL_SPTR = boost::shared_ptr<RDKit::ROMol>;
```

---

## 27.5 ROMol と RWMol

| クラス | 意味 | 用途 |
|---|---|---|
| **`ROMol`** | Read-Only Molecule | 読み取り専用。**通常はこれ** |
| **`RWMol`** | Read-Write Molecule | 原子・結合を追加/削除する |

```cpp
#include <GraphMol/RWMol.h>
#include <GraphMol/MolOps.h>

// RWMol で分子を組み立てる
RDKit::RWMol mol;

const unsigned int c1 = mol.addAtom(new RDKit::Atom(6), true, true);
const unsigned int c2 = mol.addAtom(new RDKit::Atom(6), true, true);
const unsigned int o  = mol.addAtom(new RDKit::Atom(8), true, true);

mol.addBond(c1, c2, RDKit::Bond::SINGLE);
mol.addBond(c2, o,  RDKit::Bond::SINGLE);

// ★ 重要: 構築後に sanitize が必要
RDKit::MolOps::sanitizeMol(mol);

std::cout << RDKit::MolToSmiles(mol) << "\n";   // "CCO"
```

> ⚠️ **`sanitizeMol` を忘れると、多くの機能が動きません。**
> sanitize は以下を行います:
> - 原子価のチェック
> - 芳香族の検出（Hückel則）
> - 環情報の計算（SSSR）
> - 暗黙の水素数の計算
> - 立体化学の割り当て
>
> `SmilesToMol` は内部で自動的に呼んでいます。
> `RWMol` で手動構築したときは、自分で呼ぶ必要があります。

```cpp
// sanitize を部分的にスキップして高速化する（大規模処理のとき）
unsigned int failed;
RDKit::MolOps::sanitizeMol(mol, failed,
    RDKit::MolOps::SANITIZE_ALL ^ RDKit::MolOps::SANITIZE_KEKULIZE
                                ^ RDKit::MolOps::SANITIZE_SETAROMATICITY);
```

### パースオプション

```cpp
#include <GraphMol/SmilesParse/SmilesParse.h>

RDKit::SmilesParserParams params;
params.sanitize      = true;    // sanitize するか
params.removeHs      = true;    // 明示水素を除去するか
params.debugParse    = 0;
params.allowCXSMILES = true;    // CXSMILES 拡張を許すか

std::unique_ptr<RDKit::ROMol> mol(RDKit::SmilesToMol(smiles, params));
```

> 💡 **速度が欲しいなら `sanitize = false`** にして、
> 必要な処理だけ手動で呼ぶ手があります。
> ただし、記述子計算やフィンガープリントには sanitize 済みが必要です。

---

## 27.6 フィンガープリント

```cpp
#include <GraphMol/Fingerprints/MorganFingerprints.h>
#include <GraphMol/Fingerprints/Fingerprints.h>
#include <GraphMol/Fingerprints/MorganGenerator.h>
#include <DataStructs/ExplicitBitVect.h>
#include <DataStructs/BitOps.h>

// ---- 旧API（まだ広く使われている）----
std::unique_ptr<ExplicitBitVect> fp(
    RDKit::MorganFingerprints::getFingerprintAsBitVect(*mol, 2, 2048));

std::cout << "bits set: " << fp->getNumOnBits() << "\n";
std::cout << "size: " << fp->getNumBits() << "\n";

// Tanimoto
std::unique_ptr<ExplicitBitVect> fp2(
    RDKit::MorganFingerprints::getFingerprintAsBitVect(*mol2, 2, 2048));
double sim = TanimotoSimilarity(*fp, *fp2);
double dsim = DiceSimilarity(*fp, *fp2);

// ---- 新API（RDKit 2022.09以降。こちらが推奨）----
#include <GraphMol/Fingerprints/FingerprintGenerator.h>

auto gen = std::unique_ptr<RDKit::FingerprintGenerator<std::uint32_t>>(
    RDKit::MorganFingerprint::getMorganGenerator<std::uint32_t>(2));  // radius=2

std::unique_ptr<ExplicitBitVect> fp3(gen->getFingerprintAsBitVect(*mol));

// カウント版
auto counts = std::unique_ptr<RDKit::SparseIntVect<std::uint32_t>>(
    gen->getCountFingerprint(*mol));
```

> 💡 **Generator API の利点: 生成器を1回作って使い回せる。**
> 内部のバッファが再利用されるので、大量処理では速くなります。

### ビットベクタの取り出し

```cpp
// 立っているビットのリスト
std::vector<int> on_bits;
fp->getOnBits(on_bits);

// 自前のフォーマットに変換
std::array<std::uint64_t, 32> my_fp{};
for (int b : on_bits) {
    my_fp[b / 64] |= 1ULL << (b % 64);
}
```

これで、第22章の高速 Tanimoto 実装が使えます。

> 🧪 **実用パターン: RDKitでフィンガープリントを計算し、
> 検索は自作の高速実装で行う。**
>
> RDKit の `TanimotoSimilarity` は汎用的で、
> ビット数が可変なため若干のオーバーヘッドがあります。
> 固定長 `std::array` に変換すれば、第22章の実装で数倍速くなります。

---

## 27.7 部分構造検索（SMARTS）

```cpp
#include <GraphMol/SmilesParse/SmartsWrite.h>
#include <GraphMol/Substruct/SubstructMatch.h>

// SMARTS パターン
std::unique_ptr<RDKit::ROMol> pattern(RDKit::SmartsToMol("c1ccccc1"));  // ベンゼン環
std::unique_ptr<RDKit::ROMol> mol(RDKit::SmilesToMol("Cc1ccccc1O"));

// マッチするか
RDKit::MatchVectType match;
bool found = RDKit::SubstructMatch(*mol, *pattern, match);

if (found) {
    std::cout << "matched atoms: ";
    for (const auto& [query_idx, mol_idx] : match) {
        std::cout << mol_idx << " ";
    }
    std::cout << "\n";
}

// すべてのマッチ
std::vector<RDKit::MatchVectType> matches;
unsigned int n = RDKit::SubstructMatch(*mol, *pattern, matches, /*uniquify=*/true);
std::cout << "found " << n << " matches\n";

// オプション付き
RDKit::SubstructMatchParameters params;
params.useChirality = true;
params.maxMatches   = 100;
params.numThreads   = 4;                   // ★ マルチスレッド対応
auto ms = RDKit::SubstructMatch(*mol, *pattern, params);
```

🧪 **よく使う SMARTS パターン:**

```cpp
const std::vector<std::pair<std::string, std::string>> alerts = {
    {"nitro",           "[N+](=O)[O-]"},
    {"aldehyde",        "[CX3H1](=O)[#6]"},
    {"michael acceptor","[CX3]=[CX3][CX3]=[OX1]"},
    {"epoxide",         "C1OC1"},
    {"azide",           "[N-]=[N+]=N"},
    {"thiol",           "[SX2H]"},
    {"carboxylic acid", "[CX3](=O)[OX2H1]"},
    {"primary amine",   "[NX3;H2;!$(NC=O)]"},
    {"amide",           "[NX3][CX3](=[OX1])"},
    {"sulfonamide",     "[SX4](=[OX1])(=[OX1])([NX3])"},
};

// 構造アラートのスクリーニング
for (const auto& [name, smarts] : alerts) {
    std::unique_ptr<RDKit::ROMol> pat(RDKit::SmartsToMol(smarts));
    RDKit::MatchVectType m;
    if (RDKit::SubstructMatch(*mol, *pat, m)) {
        std::cout << "ALERT: " << name << "\n";
    }
}
```

> 💡 **SMARTSパターンは使い回してください。**
> `SmartsToMol` は毎回パースするので重いです。
> プログラム開始時に1回だけ作り、`std::vector<std::unique_ptr<ROMol>>` に保持します。

---

## 27.8 ファイル I/O

```cpp
#include <GraphMol/FileParsers/MolSupplier.h>
#include <GraphMol/FileParsers/MolWriters.h>

// ---- SDF の読み込み ----
RDKit::SDMolSupplier supplier("compounds.sdf", /*sanitize=*/true,
                              /*removeHs=*/true, /*strictParsing=*/false);

while (!supplier.atEnd()) {
    std::unique_ptr<RDKit::ROMol> mol(supplier.next());
    if (!mol) continue;               // パース失敗はスキップ

    // プロパティを読む
    std::string id;
    if (mol->getPropIfPresent("CHEMBL_ID", id)) {
        std::cout << id << ": " << RDKit::MolToSmiles(*mol) << "\n";
    }
    double ic50 = 0.0;
    if (mol->getPropIfPresent("IC50", ic50)) {
        std::cout << "  IC50 = " << ic50 << "\n";
    }

    // 全プロパティ
    for (const auto& name : mol->getPropList()) {
        std::cout << "  " << name << " = "
                  << mol->getProp<std::string>(name) << "\n";
    }
}

// ---- ランダムアクセス版（インデックス付き）----
RDKit::SDMolSupplier rs("compounds.sdf");
std::cout << "total: " << rs.length() << "\n";
std::unique_ptr<RDKit::ROMol> m10(rs[10]);       // 10番目の分子

// ---- SMILES ファイル ----
RDKit::SmilesMolSupplier smi_supplier("compounds.smi", "\t", 0, 1, false, true);

// ---- 圧縮ファイル（.sdf.gz）----
#include <GraphMol/FileParsers/MolSupplier.h>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

std::ifstream file("compounds.sdf.gz", std::ios::binary);
boost::iostreams::filtering_istream in;
in.push(boost::iostreams::gzip_decompressor());
in.push(file);
RDKit::ForwardSDMolSupplier gz_supplier(&in, false);

// ---- 書き出し ----
RDKit::SDWriter writer("output.sdf");
mol->setProp("MW", RDKit::Descriptors::calcExactMW(*mol));
mol->setProp("_Name", "aspirin");
writer.write(*mol);
writer.close();

RDKit::SmilesWriter smi_writer("output.smi", " ", "Name");
smi_writer.write(*mol);
```

### マルチスレッド版のサプライヤ

```cpp
#ifdef RDK_BUILD_THREADSAFE_SSS
#include <GraphMol/FileParsers/MultithreadedSDMolSupplier.h>

RDKit::MultithreadedSDMolSupplier supplier(
    "huge.sdf", /*sanitize=*/true, /*removeHs=*/true,
    /*strictParsing=*/false, /*numWriterThreads=*/4);

while (!supplier.atEnd()) {
    std::unique_ptr<RDKit::ROMol> mol(supplier.next());
    if (mol) process(*mol);
}
#endif
```

**パースがマルチスレッド化されるので、巨大なSDFで劇的に速くなります。**

---

## 27.9 3D 構造と力場

```cpp
#include <GraphMol/DistGeomHelpers/Embedder.h>
#include <GraphMol/ForceFieldHelpers/MMFF/MMFF.h>
#include <GraphMol/ForceFieldHelpers/UFF/UFF.h>
#include <GraphMol/MolOps.h>
#include <GraphMol/Conformer.h>

std::unique_ptr<RDKit::ROMol> mol(RDKit::SmilesToMol("CCO"));

// ★ 3D生成には明示的な水素が必要
RDKit::RWMol rw(*mol);
RDKit::MolOps::addHs(rw);

// ---- コンフォマー生成 ----
RDKit::DGeomHelpers::EmbedParameters params = RDKit::DGeomHelpers::ETKDGv3;
params.randomSeed = 42;                 // ★ 再現性のために必須
params.numThreads = 4;
params.pruneRmsThresh = 0.5;            // 類似コンフォマーを除去

RDKit::INT_VECT cids;
RDKit::DGeomHelpers::EmbedMultipleConfs(rw, cids, /*numConfs=*/10, params);

std::cout << "generated " << cids.size() << " conformers\n";

// ---- MMFF94 最適化 ----
std::vector<std::pair<int, double>> results;
RDKit::MMFF::MMFFOptimizeMoleculeConfs(rw, results, /*numThreads=*/4,
                                       /*maxIters=*/1000);

for (std::size_t i = 0; i < results.size(); ++i) {
    std::cout << "conf " << cids[i]
              << ": converged=" << results[i].first
              << " energy=" << results[i].second << " kcal/mol\n";
}

// ---- 座標へのアクセス ----
const RDKit::Conformer& conf = rw.getConformer(cids[0]);
for (unsigned int i = 0; i < rw.getNumAtoms(); ++i) {
    const RDGeom::Point3D& pos = conf.getAtomPos(i);
    std::cout << rw.getAtomWithIdx(i)->getSymbol() << " "
              << pos.x << " " << pos.y << " " << pos.z << "\n";
}

// ---- 幾何量の計算 ----
#include <GraphMol/MolTransforms/MolTransforms.h>

double bond_len = RDKit::MolTransforms::getBondLength(conf, 0, 1);
double angle    = RDKit::MolTransforms::getAngleDeg(conf, 0, 1, 2);
double dihedral = RDKit::MolTransforms::getDihedralDeg(conf, 0, 1, 2, 3);
```

---

## 27.10 🧪 総合例: 高速スクリーニングツール

```cpp
// apps/rdkit_screen.cpp
#include <GraphMol/GraphMol.h>
#include <GraphMol/SmilesParse/SmilesParse.h>
#include <GraphMol/SmilesParse/SmilesWrite.h>
#include <GraphMol/Descriptors/MolDescriptors.h>
#include <GraphMol/Descriptors/Crippen.h>
#include <GraphMol/Fingerprints/MorganGenerator.h>
#include <GraphMol/Substruct/SubstructMatch.h>
#include <DataStructs/ExplicitBitVect.h>
#include <DataStructs/BitOps.h>

#include <atomic>
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace {

using FP = std::array<std::uint64_t, 32>;   // 2048 bit

// RDKit の ExplicitBitVect を、高速演算用の固定長配列に変換
FP to_fixed(const ExplicitBitVect& bv) {
    FP out{};
    std::vector<int> on;
    const_cast<ExplicitBitVect&>(bv).getOnBits(on);
    for (int b : on) out[b / 64] |= 1ULL << (b % 64);
    return out;
}

inline double tanimoto_fast(const FP& a, int ca, const FP& b, int cb) {
    int inter = 0;
    for (std::size_t i = 0; i < 32; ++i) inter += std::popcount(a[i] & b[i]);
    const int uni = ca + cb - inter;
    return uni ? static_cast<double>(inter) / uni : 0.0;
}

struct Record {
    std::string id;
    std::string smiles;
    FP          fp{};
    int         fp_count = 0;
    double      mw = 0, logp = 0, tpsa = 0;
    int         hbd = 0, hba = 0, rotb = 0;
    bool        valid = false;
    bool        has_alert = false;
};

}  // namespace

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    if (argc < 3) {
        std::cerr << "usage: rdkit_screen <library.smi> <query_smiles> "
                     "[threshold=0.6]\n";
        return 1;
    }
    const double threshold = (argc > 3) ? std::stod(argv[3]) : 0.6;

    // ---- ライブラリ読み込み ----
    std::vector<std::pair<std::string, std::string>> inputs;   // smiles, id
    {
        std::ifstream in(argv[1]);
        if (!in) { std::cerr << "cannot open " << argv[1] << "\n"; return 1; }
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            const auto tab = line.find_first_of(" \t");
            inputs.emplace_back(
                line.substr(0, tab),
                tab == std::string::npos ? std::to_string(inputs.size())
                                         : line.substr(tab + 1));
        }
    }
    std::cout << "loaded " << inputs.size() << " SMILES\n";

    // ---- 構造アラートのパターン（1回だけパース）----
    const std::vector<std::pair<std::string, std::string>> alert_smarts = {
        {"nitro",    "[N+](=O)[O-]"},
        {"aldehyde", "[CX3H1](=O)[#6]"},
        {"epoxide",  "C1OC1"},
        {"azide",    "[N-]=[N+]=N"},
    };
    std::vector<std::unique_ptr<RDKit::ROMol>> alerts;
    for (const auto& [name, sma] : alert_smarts)
        alerts.emplace_back(RDKit::SmartsToMol(sma));

    // ---- 並列処理 ----
    const unsigned nt = std::max(1u, std::thread::hardware_concurrency());
    std::vector<Record> records(inputs.size());
    std::atomic<std::size_t> next{0};

    auto t0 = std::chrono::steady_clock::now();
    {
        std::vector<std::thread> threads;
        for (unsigned t = 0; t < nt; ++t) {
            threads.emplace_back([&] {
                // ★ スレッドごとに Generator を持つ（共有すると競合する可能性）
                auto gen = std::unique_ptr<
                    RDKit::FingerprintGenerator<std::uint32_t>>(
                    RDKit::MorganFingerprint::getMorganGenerator<std::uint32_t>(2));

                constexpr std::size_t GRAIN = 128;
                for (;;) {
                    const std::size_t b = next.fetch_add(GRAIN,
                                              std::memory_order_relaxed);
                    if (b >= inputs.size()) break;
                    const std::size_t e = std::min(b + GRAIN, inputs.size());

                    for (std::size_t i = b; i < e; ++i) {
                        std::unique_ptr<RDKit::ROMol> mol(
                            RDKit::SmilesToMol(inputs[i].first));
                        if (!mol) continue;

                        Record& r = records[i];
                        r.id     = inputs[i].second;
                        r.smiles = inputs[i].first;
                        r.mw     = RDKit::Descriptors::calcExactMW(*mol);
                        r.logp   = RDKit::Descriptors::calcClogP(*mol);
                        r.tpsa   = RDKit::Descriptors::calcTPSA(*mol);
                        r.hbd    = RDKit::Descriptors::calcNumHBD(*mol);
                        r.hba    = RDKit::Descriptors::calcNumHBA(*mol);
                        r.rotb   = RDKit::Descriptors::calcNumRotatableBonds(*mol);

                        std::unique_ptr<ExplicitBitVect> bv(
                            gen->getFingerprintAsBitVect(*mol));
                        r.fp       = to_fixed(*bv);
                        r.fp_count = static_cast<int>(bv->getNumOnBits());

                        for (const auto& pat : alerts) {
                            RDKit::MatchVectType mv;
                            if (RDKit::SubstructMatch(*mol, *pat, mv)) {
                                r.has_alert = true;
                                break;
                            }
                        }
                        r.valid = true;
                    }
                }
            });
        }
        for (auto& th : threads) th.join();
    }
    const double proc_ms = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - t0).count();

    std::size_t n_valid = 0;
    for (const auto& r : records) n_valid += r.valid;
    std::cout << "processed " << n_valid << " molecules in "
              << proc_ms << " ms ("
              << (n_valid / proc_ms * 1000.0) << " mol/s, "
              << nt << " threads)\n";

    // ---- クエリ ----
    std::unique_ptr<RDKit::ROMol> qmol(RDKit::SmilesToMol(argv[2]));
    if (!qmol) { std::cerr << "invalid query\n"; return 1; }

    auto gen = std::unique_ptr<RDKit::FingerprintGenerator<std::uint32_t>>(
        RDKit::MorganFingerprint::getMorganGenerator<std::uint32_t>(2));
    std::unique_ptr<ExplicitBitVect> qbv(gen->getFingerprintAsBitVect(*qmol));
    const FP  qfp = to_fixed(*qbv);
    const int qc  = static_cast<int>(qbv->getNumOnBits());

    // ---- 検索（自作の高速実装）----
    t0 = std::chrono::steady_clock::now();
    std::vector<std::pair<double, std::size_t>> hits;
    const int lo = static_cast<int>(std::ceil (qc * threshold));
    const int hi = static_cast<int>(std::floor(qc / threshold));

    for (std::size_t i = 0; i < records.size(); ++i) {
        const Record& r = records[i];
        if (!r.valid || r.has_alert) continue;
        if (r.fp_count < lo || r.fp_count > hi) continue;      // ★ 枝刈り
        if (r.mw > 500 || r.logp > 5) continue;                // Ro5 フィルタ

        const double s = tanimoto_fast(qfp, qc, r.fp, r.fp_count);
        if (s >= threshold) hits.emplace_back(s, i);
    }
    std::sort(hits.begin(), hits.end(), std::greater<>{});

    const double search_ms = std::chrono::duration<double, std::milli>(
                                 std::chrono::steady_clock::now() - t0).count();

    std::cout << "\n" << hits.size() << " hits in " << search_ms << " ms\n\n";
    for (std::size_t k = 0; k < std::min<std::size_t>(hits.size(), 20); ++k) {
        const Record& r = records[hits[k].second];
        std::cout << "  " << (k + 1) << ". " << r.id
                  << "  T=" << hits[k].first
                  << "  MW=" << r.mw << "  logP=" << r.logp
                  << "  " << r.smiles << "\n";
    }
    return 0;
}
```

参考性能（16コア、100万分子）:

```
loaded 1000000 SMILES
processed 997159 molecules in 18420 ms (54134 mol/s, 16 threads)

3412 hits in 8.2 ms
```

**Python版（RDKit + multiprocessing）だと、同じ処理で約5〜8分**かかります。
C++版は**18秒**。20倍以上の差です。

---

## 27.11 RDKit C++ を使う上での注意点

### ① ヘッダのインクルードが多い

```cpp
#include <GraphMol/GraphMol.h>                          // ROMol, Atom, Bond
#include <GraphMol/SmilesParse/SmilesParse.h>           // SmilesToMol
#include <GraphMol/SmilesParse/SmilesWrite.h>           // MolToSmiles
#include <GraphMol/Descriptors/MolDescriptors.h>        // 記述子
#include <GraphMol/FileParsers/MolSupplier.h>           // SDF読み込み
```

**必要なヘッダを探すのが最初の障壁**です。
`$CONDA_PREFIX/include/rdkit/` を `grep` するのが確実:

```bash
grep -r "calcTPSA" $CONDA_PREFIX/include/rdkit/ | head
```

### ② ドキュメントが Python 中心

C++ APIのドキュメントは:
- [RDKit C++ Doxygen](https://www.rdkit.org/docs/cppapi/)
- ソースコードのヘッダファイル（**これが一番確実**）
- `Code/` 以下の `*Test*.cpp`（**使用例の宝庫**）

```bash
# 使い方を知りたい機能のテストコードを見る
find $RDKIT_SRC/Code -name "*test*.cpp" | xargs grep -l "MorganFingerprint"
```

### ③ 例外の扱い

```cpp
#include <RDGeneral/Invariant.h>

try {
    std::unique_ptr<RDKit::ROMol> mol(RDKit::SmilesToMol(smiles));
} catch (const RDKit::MolSanitizeException& e) {
    std::cerr << "sanitize failed: " << e.what() << "\n";
} catch (const RDKit::SmilesParseException& e) {
    std::cerr << "parse failed: " << e.what() << "\n";
} catch (const Invar::Invariant& e) {
    std::cerr << "invariant violation: " << e.what() << "\n";
}
```

> ⚠️ **`SmilesToMol` は失敗時に `nullptr` を返すことも例外を投げることもあります。**
> `sanitize=true` のときは例外、`false` のときは nullptr が多いです。
> **両方チェックしてください。**
>
> 100万分子のループでは、例外が数%発生すると遅くなります。
> `try/catch` をループの外に置かず、内側に置いて continue するのが基本です。

### ④ スレッドセーフ性

- **`ROMol` の読み取りは基本的にスレッドセーフ**
- **`RWMol` の変更、`sanitizeMol` はスレッドセーフでない**
- **`FingerprintGenerator` はスレッドごとに作る**のが安全
- RDKit を `RDK_BUILD_THREADSAFE_SSS=ON` でビルドする必要がある機能もある

### ⑤ Boost 依存

RDKit は Boost に強く依存しています。
バージョン不一致でリンクエラーが出やすいので、
**conda で揃えるのが最も確実**です。

---

## 27.12 自作 vs RDKit の使い分け

| 機能 | 自作 | RDKit |
|---|---|---|
| SMILESパース | 学習用 | **本番はこちら**（立体、芳香族、正規化が正確） |
| フィンガープリント | 実験的な独自FP | **標準的なFPはこちら** |
| Tanimoto計算 | **自作が速い**（固定長最適化） | 汎用的だが少し遅い |
| 記述子 | 簡易版 | **こちら**（数百種類、検証済み） |
| 部分構造検索 | 実装が大変 | **こちら**（SMARTS、VF2） |
| 3D生成・力場 | 現実的でない | **こちら** |
| 大規模検索エンジン | **自作**（データ構造を最適化） | 素材として使う |

> 💡 **最適な戦略:**
> 1. **RDKitで正確な前処理**（パース、標準化、フィンガープリント生成）
> 2. **自作の高速データ構造に変換**（固定長配列、SoA）
> 3. **自作の検索エンジンで高速処理**（枝刈り、SIMD、並列化）
>
> 本章の 27.10 がまさにこのパターンです。

---

## 27.13 この章のまとめ

- RDKit は C++ ライブラリ。Python API はその薄いラッパー
- **conda (`rdkit-dev`) でのインストールが最も簡単**
- `find_package(RDKit)` + 個別のライブラリをリンク
- **C++17 でビルドする**（RDKitのABIに合わせる）
- **`SmilesToMol` は生ポインタを返す → `unique_ptr` で受ける**
- `ROMol`（読み取り専用）と `RWMol`（変更可能）
- **`RWMol` を手動構築したら `sanitizeMol` を呼ぶ**
- `FingerprintGenerator` はスレッドごとに作る
- **RDKitで前処理 → 自作の高速構造に変換 → 自作エンジンで検索** が最強
- ドキュメントが薄いので、**ヘッダとテストコードを読む**

> 📝 **練習問題 27-1**
>
> RDKit の C++ API で、SDFファイルを読んで
> 全分子の記述子をCSVに出力するプログラムを書き、
> Python版と速度を比較してください。

> 📝 **練習問題 27-2**
>
> 27.10 のツールに、SMARTSパターンをコマンドラインから
> 指定できる機能を追加してください。

> 📝 **練習問題 27-3**
>
> RDKit の `MolStandardize` を使って、
> 塩の除去・電荷の中和・互変異性体の正規化を行うツールを書いてください。

> 📝 **練習問題 27-4**
>
> RDKit の Morgan FP と、第21章で自作した Morgan FP の
> Tanimoto 類似度を1000ペアで比較し、相関を見てください。

---

→ [第28章 プロファイリングと最適化](ch28-profiling-and-optimization.md)
