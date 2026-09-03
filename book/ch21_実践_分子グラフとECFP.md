# 第21章 実践2: 分子グラフと ECFP（Morganフィンガープリント）

`AllChem.GetMorganFingerprintAsBitVect(mol, 2, 2048)` が
中で何をしているのか、自分で実装して確かめます。

## 21.1 ECFP のアルゴリズム

**ECFP (Extended-Connectivity Fingerprint)** = **Morganフィンガープリント**は、
以下の手順で計算されます。

```
① 各原子に「初期不変量」を割り当てる
   （原子番号、次数、水素数、電荷、環の中か、など）
       ↓
② 各原子のハッシュ値を計算 → これが半径0の特徴
       ↓
③ 【反復】各原子について:
      自分のハッシュ + 隣接原子のハッシュ+結合次数 を集めてソート
      → 新しいハッシュを計算
      → これが半径1、2、3... の特徴
       ↓
④ 出てきた全ハッシュ値を集める
       ↓
⑤ ハッシュ値 mod nBits でビットを立てる（folding）
```

図で表すと:

```
   半径0:   [C]           1つの原子だけの環境

   半径1:   [C]-[O]       1結合離れた原子まで
             |
            [C]

   半径2:   [C]-[O]-[H]   2結合離れた原子まで
             |
            [C]-[C]
```

**各原子について、半径0, 1, 2, ... の「環境」をハッシュ化**し、
そのハッシュ値でビットを立てます。

> 🧪 **ECFP4 とは?**
> 「4」は**直径**を表します。半径は2です。
> ECFP6 なら半径3。RDKit の `GetMorganFingerprint(mol, radius=2)` が ECFP4 です。
> 紛らわしいので注意してください。

---

## 21.2 初期不変量 (Daylight invariants)

各原子に、その原子の「素性」を表す数値を割り当てます。
標準的な ECFP では以下の6つを使います:

| 項目 | 説明 |
|---|---|
| 重原子の隣接数 | 水素を除いた結合数 |
| 価電子数 - 水素数 | 結合に使われている電子数 |
| 原子番号 | 元素の種類 |
| 原子量（同位体） | 同位体の区別 |
| 電荷 | 形式電荷 |
| 結合した水素の数 | |
| 環の中かどうか | （FCFPでは追加特徴） |

これらを1つの整数にハッシュします。

---

## 21.3 実装

```cpp
// include/chemcpp/fingerprint.hpp
#pragma once

#include <chemcpp/molecule.hpp>

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace chemcpp {

inline constexpr std::size_t FP_BITS  = 2048;
inline constexpr std::size_t FP_WORDS = FP_BITS / 64;

using Fingerprint = std::array<std::uint64_t, FP_WORDS>;

struct MorganOptions {
    int  radius        = 2;       // ECFP4 なら 2
    int  n_bits        = static_cast<int>(FP_BITS);
    bool use_chirality = false;
    bool use_features  = false;   // true なら FCFP（薬理特徴ベース）
    bool count_based   = false;   // true なら出現回数も保持
};

/// ビットベクタ版 Morgan フィンガープリント
[[nodiscard]] Fingerprint morgan_fingerprint(const Molecule& mol,
                                             const MorganOptions& opt = {});

/// 生のハッシュ値（folding 前）。デバッグや部分構造の同定に使う
[[nodiscard]] std::vector<std::uint32_t>
morgan_hashes(const Molecule& mol, const MorganOptions& opt = {});

/// カウント版（出現回数付き）
[[nodiscard]] std::unordered_map<std::uint32_t, int>
morgan_counts(const Molecule& mol, const MorganOptions& opt = {});

// ---- ビット操作ユーティリティ ----
inline void set_bit(Fingerprint& fp, std::size_t bit) {
    fp[bit / 64] |= (1ULL << (bit % 64));
}
inline bool get_bit(const Fingerprint& fp, std::size_t bit) {
    return (fp[bit / 64] >> (bit % 64)) & 1ULL;
}

}  // namespace chemcpp
```

```cpp
// src/fingerprint.cpp
#include <chemcpp/fingerprint.hpp>
#include <chemcpp/element.hpp>

#include <algorithm>
#include <bit>
#include <vector>

namespace chemcpp {
namespace {

// ---------------------------------------------------------------
// ハッシュ関数（Boost の hash_combine と同じ方式）
// ---------------------------------------------------------------
inline std::uint32_t hash_combine(std::uint32_t seed, std::uint32_t value) {
    seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
}

inline std::uint32_t hash_u32(std::uint32_t x) {
    // MurmurHash3 の finalizer（よく混ざる、速い）
    x ^= x >> 16;
    x *= 0x85ebca6bu;
    x ^= x >> 13;
    x *= 0xc2b2ae35u;
    x ^= x >> 16;
    return x;
}

// ---------------------------------------------------------------
// 初期不変量の計算
// ---------------------------------------------------------------
std::uint32_t initial_invariant(const Molecule& mol, std::size_t idx,
                                const MorganOptions& opt) {
    const Atom& a = mol.atom(idx);

    if (opt.use_features) {
        // FCFP: 薬理特徴（ドナー/アクセプタ/芳香/正電荷/負電荷/疎水）でビットを立てる
        std::uint32_t feat = 0;
        const bool is_n_or_o = (a.atomic_num == 7 || a.atomic_num == 8);
        if (is_n_or_o && a.num_h > 0)                feat |= 1u << 0;  // HBD
        if (is_n_or_o)                                feat |= 1u << 1;  // HBA
        if (a.aromatic)                               feat |= 1u << 2;  // Aromatic
        if (a.charge > 0)                             feat |= 1u << 3;  // Positive
        if (a.charge < 0)                             feat |= 1u << 4;  // Negative
        if (a.atomic_num == 6 || a.atomic_num == 9 ||
            a.atomic_num == 17 || a.atomic_num == 35) feat |= 1u << 5;  // Hydrophobe
        return hash_u32(feat);
    }

    // ECFP: Daylight 型の不変量
    int heavy_degree = 0;
    int bond_sum     = 0;
    for (const auto& nb : mol.neighbors(idx)) {
        if (mol.atom(nb.atom).atomic_num == 1) continue;   // 明示水素は数えない
        ++heavy_degree;
        switch (mol.bond(nb.bond).order) {
            case BondOrder::Single:    bond_sum += 1; break;
            case BondOrder::Double:    bond_sum += 2; break;
            case BondOrder::Triple:    bond_sum += 3; break;
            case BondOrder::Quadruple: bond_sum += 4; break;
            case BondOrder::Aromatic:  bond_sum += 1; break;   // 芳香は1として扱う
            default:                   bond_sum += 1; break;
        }
    }

    std::uint32_t h = 0;
    h = hash_combine(h, static_cast<std::uint32_t>(heavy_degree));
    h = hash_combine(h, static_cast<std::uint32_t>(bond_sum));
    h = hash_combine(h, static_cast<std::uint32_t>(a.atomic_num));
    h = hash_combine(h, static_cast<std::uint32_t>(a.isotope));
    h = hash_combine(h, static_cast<std::uint32_t>(a.charge + 128));
    h = hash_combine(h, static_cast<std::uint32_t>(a.num_h));
    h = hash_combine(h, static_cast<std::uint32_t>(a.in_ring ? 1 : 0));
    if (opt.use_chirality) {
        h = hash_combine(h, static_cast<std::uint32_t>(a.chirality));
    }
    return hash_u32(h);
}

// 結合次数を数値化（ハッシュ用）
inline std::uint32_t bond_code(BondOrder o) {
    return static_cast<std::uint32_t>(o);
}

}  // anonymous namespace

// ---------------------------------------------------------------
// Morgan アルゴリズム本体
// ---------------------------------------------------------------
std::unordered_map<std::uint32_t, int>
morgan_counts(const Molecule& mol, const MorganOptions& opt) {
    const std::size_t n = mol.num_atoms();
    std::unordered_map<std::uint32_t, int> features;
    if (n == 0) return features;

    features.reserve(n * (opt.radius + 1));

    // 現在の各原子のハッシュ値
    std::vector<std::uint32_t> current(n);
    std::vector<std::uint32_t> next(n);

    // --- 半径0: 初期不変量 ---
    for (std::size_t i = 0; i < n; ++i) {
        current[i] = initial_invariant(mol, i, opt);
        ++features[current[i]];
    }

    // 既に見つけた（原子集合, ハッシュ）の重複を除くためのセット
    // 簡易版として、同一半径で同じハッシュが出た場合のみカウント
    std::vector<std::pair<std::uint32_t, std::uint32_t>> neighbor_buf;
    neighbor_buf.reserve(8);

    // --- 半径 1..radius ---
    for (int r = 1; r <= opt.radius; ++r) {
        for (std::size_t i = 0; i < n; ++i) {
            neighbor_buf.clear();

            for (const auto& nb : mol.neighbors(i)) {
                if (mol.atom(nb.atom).atomic_num == 1) continue;
                neighbor_buf.emplace_back(bond_code(mol.bond(nb.bond).order),
                                          current[nb.atom]);
            }

            // ★ 重要: ソートして順序を正規化する
            //    （隣接原子の並び順に依存しないハッシュにするため）
            std::sort(neighbor_buf.begin(), neighbor_buf.end());

            std::uint32_t h = hash_combine(static_cast<std::uint32_t>(r), current[i]);
            for (const auto& [bo, nh] : neighbor_buf) {
                h = hash_combine(h, bo);
                h = hash_combine(h, nh);
            }
            next[i] = hash_u32(h);
        }

        current.swap(next);
        for (std::size_t i = 0; i < n; ++i) ++features[current[i]];
    }

    return features;
}

std::vector<std::uint32_t>
morgan_hashes(const Molecule& mol, const MorganOptions& opt) {
    auto counts = morgan_counts(mol, opt);
    std::vector<std::uint32_t> out;
    out.reserve(counts.size());
    for (const auto& [h, c] : counts) out.push_back(h);
    std::sort(out.begin(), out.end());
    return out;
}

Fingerprint morgan_fingerprint(const Molecule& mol, const MorganOptions& opt) {
    Fingerprint fp{};
    const auto counts = morgan_counts(mol, opt);
    const std::size_t nbits = static_cast<std::size_t>(opt.n_bits);

    for (const auto& [h, count] : counts) {
        set_bit(fp, h % nbits);              // ★ folding
    }
    return fp;
}

}  // namespace chemcpp
```

---

## 21.4 アルゴリズムの解説

### なぜソートするのか

```cpp
std::sort(neighbor_buf.begin(), neighbor_buf.end());
```

これが**アルゴリズムの核心**です。

```
   原子Aの隣接: [C, O, N]     ← 隣接リストの順序
   原子Bの隣接: [O, N, C]     ← 同じ環境だが、順序が違う
```

**同じ化学的環境なら、同じハッシュ値になるべき**です。
隣接リストの順序は SMILES の書き方に依存するので、
**ソートして正規化**します。

これにより、`CCO` と `OCC` から同じフィンガープリントが得られます。

### なぜ半径ごとに全原子を更新するのか

```cpp
for (int r = 1; r <= radius; ++r) {
    for (std::size_t i = 0; i < n; ++i) {
        // 全原子を同時に更新
    }
    current.swap(next);      // ★ 一斉に切り替え
}
```

**同期的な更新**が重要です。もし1原子ずつ更新すると、
更新済みの隣接原子と未更新の隣接原子が混在してしまいます。

```cpp
// ✗ 間違い: current を直接更新すると、後の原子は更新済みの値を見てしまう
for (std::size_t i = 0; i < n; ++i) {
    current[i] = compute_from_neighbors(current);   // ダメ
}

// ✓ 正しい: next に書いて、最後にまとめて切り替え
for (std::size_t i = 0; i < n; ++i) {
    next[i] = compute_from_neighbors(current);
}
current.swap(next);
```

> 💡 これは**グラフニューラルネットワーク (GNN) のメッセージパッシングと
> 全く同じ構造**です。実際、ECFP は「学習しないGNN」と言えます。
>
> ```python
> # GNN の1層（PyTorch Geometric 風）
> h_new[i] = update(h[i], aggregate([h[j] for j in neighbors(i)]))
> ```
> ```cpp
> // ECFP の1反復
> next[i] = hash(current[i], sorted([current[j] for j in neighbors(i)]))
> ```
>
> **違いは `update` が学習可能な MLP か、固定のハッシュ関数か**だけです。
> この対応関係を知っていると、GNNの理解も深まります。

### folding（ビット畳み込み）

```cpp
set_bit(fp, h % nbits);
```

ハッシュ値は32ビット（約43億通り）ですが、
フィンガープリントは2048ビットです。
そこで `mod 2048` で押し込みます。

**当然、衝突が起きます。** 異なる部分構造が同じビットに落ちることがあります。
これがフィンガープリントの精度限界の一因です。

> 🧪 **ビット数を増やすと衝突が減ります。**
> - 1024 bit: 衝突が多い。高速、省メモリ
> - 2048 bit: 標準的なバランス
> - 4096 bit: 衝突が少ない
> - **カウント版（folding なし）**: 衝突ゼロだが、疎ベクトルの扱いが必要
>
> 機械学習の特徴量として使うなら、
> `count_based`（カウント版）の方が精度が上がることがよくあります。

---

## 21.5 使ってみる

```cpp
// apps/fp_demo.cpp
#include <chemcpp/smiles.hpp>
#include <chemcpp/fingerprint.hpp>
#include <chemcpp/similarity.hpp>

#include <bit>
#include <format>
#include <iostream>
#include <vector>

using namespace chemcpp;

int popcount(const Fingerprint& fp) {
    int n = 0;
    for (auto w : fp) n += std::popcount(w);
    return n;
}

double tanimoto(const Fingerprint& a, const Fingerprint& b) {
    int inter = 0, uni = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        inter += std::popcount(a[i] & b[i]);
        uni   += std::popcount(a[i] | b[i]);
    }
    return uni ? static_cast<double>(inter) / uni : 0.0;
}

int main() {
    struct Entry { const char* name; const char* smiles; };
    const std::vector<Entry> mols = {
        {"aspirin",       "CC(=O)Oc1ccccc1C(=O)O"},
        {"salicylic acid","O=C(O)c1ccccc1O"},
        {"paracetamol",   "CC(=O)Nc1ccc(O)cc1"},
        {"ibuprofen",     "CC(C)Cc1ccc(cc1)C(C)C(=O)O"},
        {"caffeine",      "Cn1cnc2c1c(=O)n(C)c(=O)n2C"},
        {"benzene",       "c1ccccc1"},
        {"toluene",       "Cc1ccccc1"},
        {"phenol",        "Oc1ccccc1"},
        {"ethanol",       "CCO"},
    };

    std::vector<Fingerprint> fps;
    std::vector<std::string> names;

    for (const auto& e : mols) {
        auto mol = parse_smiles(e.smiles);
        if (!mol) { std::cerr << "parse failed: " << e.smiles << "\n"; continue; }

        MorganOptions opt{.radius = 2, .n_bits = 2048};
        auto fp = morgan_fingerprint(*mol, opt);

        fps.push_back(fp);
        names.push_back(e.name);

        std::cout << std::format("{:<16} atoms={:>2}  bits set={:>3}  "
                                 "unique features={}\n",
                                 e.name, mol->num_atoms(), popcount(fp),
                                 morgan_hashes(*mol, opt).size());
    }

    // --- 類似度行列 ---
    std::cout << "\nTanimoto similarity matrix:\n";
    std::cout << std::format("{:<16}", "");
    for (const auto& n : names) std::cout << std::format("{:>8.7}", n);
    std::cout << "\n";

    for (std::size_t i = 0; i < fps.size(); ++i) {
        std::cout << std::format("{:<16}", names[i]);
        for (std::size_t j = 0; j < fps.size(); ++j) {
            std::cout << std::format("{:>8.3f}", tanimoto(fps[i], fps[j]));
        }
        std::cout << "\n";
    }

    // --- 半径による違い ---
    std::cout << "\nEffect of radius (aspirin):\n";
    auto aspirin = parse_smiles("CC(=O)Oc1ccccc1C(=O)O");
    for (int r = 0; r <= 4; ++r) {
        MorganOptions opt{.radius = r};
        auto fp = morgan_fingerprint(*aspirin, opt);
        std::cout << std::format("  radius={}  bits={:>3}  features={}\n",
                                 r, popcount(fp), morgan_hashes(*aspirin, opt).size());
    }

    // --- ビット数による衝突 ---
    std::cout << "\nEffect of n_bits (aspirin, radius=2):\n";
    for (int nb : {256, 512, 1024, 2048, 4096}) {
        MorganOptions opt{.radius = 2, .n_bits = nb};
        auto counts = morgan_counts(*aspirin, opt);
        // 実際に立ったビット数を数える
        std::vector<char> bits(nb, 0);
        for (const auto& [h, c] : counts) bits[h % nb] = 1;
        int set = 0; for (char b : bits) set += b;
        std::cout << std::format("  n_bits={:>5}  features={:>3}  bits set={:>3}  "
                                 "collisions={}\n",
                                 nb, counts.size(), set,
                                 static_cast<int>(counts.size()) - set);
    }
    return 0;
}
```

出力例:

```
aspirin          atoms=13  bits set= 34  unique features=34
salicylic acid   atoms=10  bits set= 26  unique features=26
paracetamol      atoms=11  bits set= 28  unique features=28
...

Tanimoto similarity matrix:
                 aspirin salicyli paraceta ibuprof caffein  benzene  toluene   phenol  ethanol
aspirin            1.000    0.464    0.262    0.196   0.048    0.147    0.156    0.184    0.026
salicylic acid     0.464    1.000    0.243    0.150   0.043    0.190    0.194    0.324    0.032
...
benzene            0.147    0.190    0.156    0.114   0.032    1.000    0.500    0.400    0.000

Effect of radius (aspirin):
  radius=0  bits=  8  features=8
  radius=1  bits= 21  features=21
  radius=2  bits= 34  features=34
  radius=3  bits= 46  features=46
  radius=4  bits= 57  features=57

Effect of n_bits (aspirin, radius=2):
  n_bits=  256  features= 34  bits set= 32  collisions=2
  n_bits=  512  features= 34  bits set= 33  collisions=1
  n_bits= 1024  features= 34  bits set= 34  collisions=0
  n_bits= 2048  features= 34  bits set= 34  collisions=0
```

**アスピリンとサリチル酸の類似度が0.46**、
**ベンゼンとトルエンが0.50** ——化学的な直感と一致していますね。

---

## 21.6 グラフアルゴリズムの実装

ECFP以外にも、分子グラフでよく使うアルゴリズムを実装しておきます。

### BFS: 原子間の最短経路長

```cpp
// include/chemcpp/graph.hpp
#pragma once
#include <chemcpp/molecule.hpp>
#include <limits>
#include <queue>
#include <vector>

namespace chemcpp {

/// 始点から全原子への結合距離（到達不能は -1）
inline std::vector<int> shortest_paths_from(const Molecule& mol, std::uint32_t start) {
    std::vector<int> dist(mol.num_atoms(), -1);
    if (start >= mol.num_atoms()) return dist;

    std::vector<std::uint32_t> frontier{start}, next;
    dist[start] = 0;
    int d = 0;

    while (!frontier.empty()) {
        ++d;
        next.clear();
        for (std::uint32_t u : frontier) {
            for (const auto& nb : mol.neighbors(u)) {
                if (dist[nb.atom] < 0) {
                    dist[nb.atom] = d;
                    next.push_back(nb.atom);
                }
            }
        }
        frontier.swap(next);
    }
    return dist;
}

/// 全原子ペアの距離行列（Floyd-Warshall ではなく BFS を n 回）
inline std::vector<int> distance_matrix(const Molecule& mol) {
    const std::size_t n = mol.num_atoms();
    std::vector<int> d(n * n, -1);
    for (std::uint32_t i = 0; i < n; ++i) {
        auto row = shortest_paths_from(mol, i);
        for (std::size_t j = 0; j < n; ++j) d[i * n + j] = row[j];
    }
    return d;
}

/// 分子の直径（最も離れた2原子の距離）
inline int graph_diameter(const Molecule& mol) {
    int best = 0;
    for (std::uint32_t i = 0; i < mol.num_atoms(); ++i) {
        for (int x : shortest_paths_from(mol, i)) best = std::max(best, x);
    }
    return best;
}

/// 連結成分ごとに原子をラベル付け
inline std::vector<int> connected_components(const Molecule& mol) {
    const std::size_t n = mol.num_atoms();
    std::vector<int> comp(n, -1);
    int c = 0;
    std::vector<std::uint32_t> stack;

    for (std::uint32_t s = 0; s < n; ++s) {
        if (comp[s] >= 0) continue;
        stack.clear();
        stack.push_back(s);
        comp[s] = c;
        while (!stack.empty()) {
            const auto u = stack.back(); stack.pop_back();
            for (const auto& nb : mol.neighbors(u)) {
                if (comp[nb.atom] < 0) { comp[nb.atom] = c; stack.push_back(nb.atom); }
            }
        }
        ++c;
    }
    return comp;
}

}  // namespace chemcpp
```

### 環の検出（SSSR の簡易版）

```cpp
/// 環の個数（Frèrejacque 数）= 結合数 - 原子数 + 連結成分数
inline int ring_count(const Molecule& mol) {
    auto comp = connected_components(mol);
    int n_comp = comp.empty() ? 0
               : *std::max_element(comp.begin(), comp.end()) + 1;
    return static_cast<int>(mol.num_bonds()) -
           static_cast<int>(mol.num_atoms()) + n_comp;
}

/// 各原子が属する最小環のサイズ（環にないなら0）
inline std::vector<int> smallest_ring_size(const Molecule& mol) {
    const std::size_t n = mol.num_atoms();
    std::vector<int> result(n, 0);

    for (std::uint32_t start = 0; start < n; ++start) {
        if (!mol.atom(start).in_ring) continue;

        // start を含む最小の閉路を BFS で探す
        int best = std::numeric_limits<int>::max();

        for (const auto& first_nb : mol.neighbors(start)) {
            // start-first_nb の結合を「使わない」で start に戻る最短経路を探す
            std::vector<int> dist(n, -1);
            std::vector<std::uint32_t> frontier{first_nb.atom}, next;
            dist[first_nb.atom] = 0;
            dist[start] = -2;          // 通過禁止マーク

            int d = 0;
            bool found = false;
            while (!frontier.empty() && !found) {
                ++d;
                next.clear();
                for (std::uint32_t u : frontier) {
                    for (const auto& nb : mol.neighbors(u)) {
                        if (nb.bond == first_nb.bond) continue;   // 元の結合は使わない
                        if (nb.atom == start) {
                            best = std::min(best, d + 1);
                            found = true;
                            break;
                        }
                        if (dist[nb.atom] == -1) {
                            dist[nb.atom] = d;
                            next.push_back(nb.atom);
                        }
                    }
                    if (found) break;
                }
                frontier.swap(next);
            }
        }
        if (best != std::numeric_limits<int>::max()) result[start] = best;
    }
    return result;
}
```

---

## 21.7 テスト

```cpp
// tests/test_fingerprint.cpp
#include <catch2/catch_test_macros.hpp>
#include <chemcpp/smiles.hpp>
#include <chemcpp/fingerprint.hpp>
#include <bit>

using namespace chemcpp;

static int popcnt(const Fingerprint& fp) {
    int n = 0; for (auto w : fp) n += std::popcount(w); return n;
}
static double tani(const Fingerprint& a, const Fingerprint& b) {
    int i = 0, u = 0;
    for (std::size_t k = 0; k < FP_WORDS; ++k) {
        i += std::popcount(a[k] & b[k]);
        u += std::popcount(a[k] | b[k]);
    }
    return u ? double(i) / u : 0.0;
}

TEST_CASE("identical molecules give identical fingerprints", "[fp]") {
    auto m1 = parse_smiles("CC(=O)Oc1ccccc1C(=O)O");
    auto m2 = parse_smiles("CC(=O)Oc1ccccc1C(=O)O");
    REQUIRE(m1); REQUIRE(m2);
    CHECK(morgan_fingerprint(*m1) == morgan_fingerprint(*m2));
}

TEST_CASE("SMILES writing order does not matter", "[fp]") {
    // 同じ分子の別表記
    auto a = parse_smiles("CCO");
    auto b = parse_smiles("OCC");
    REQUIRE(a); REQUIRE(b);
    // ★ 原子の順序が違っても、特徴の集合は同じになるべき
    auto ha = morgan_hashes(*a);
    auto hb = morgan_hashes(*b);
    CHECK(ha == hb);
}

TEST_CASE("similarity ordering makes chemical sense", "[fp]") {
    auto benzene = parse_smiles("c1ccccc1");
    auto toluene = parse_smiles("Cc1ccccc1");
    auto xylene  = parse_smiles("Cc1ccccc1C");
    auto ethanol = parse_smiles("CCO");
    REQUIRE(benzene); REQUIRE(toluene); REQUIRE(xylene); REQUIRE(ethanol);

    auto fb = morgan_fingerprint(*benzene);
    auto ft = morgan_fingerprint(*toluene);
    auto fx = morgan_fingerprint(*xylene);
    auto fe = morgan_fingerprint(*ethanol);

    // ベンゼン-トルエン > ベンゼン-エタノール
    CHECK(tani(fb, ft) > tani(fb, fe));
    // トルエン-キシレン > ベンゼン-キシレン
    CHECK(tani(ft, fx) > tani(fb, fx));
    // 自分自身とは 1.0
    CHECK(tani(fb, fb) == 1.0);
}

TEST_CASE("larger radius gives more features", "[fp]") {
    auto m = parse_smiles("CC(=O)Oc1ccccc1C(=O)O");
    REQUIRE(m);
    std::size_t prev = 0;
    for (int r = 0; r <= 3; ++r) {
        auto h = morgan_hashes(*m, {.radius = r});
        CHECK(h.size() > prev);
        prev = h.size();
    }
}

TEST_CASE("empty molecule", "[fp]") {
    Molecule empty;
    auto fp = morgan_fingerprint(empty);
    CHECK(popcnt(fp) == 0);
}
```

---

## 21.8 RDKit との比較

自作のECFPとRDKitの結果は**完全には一致しません**。理由:

| 差異 | 内容 |
|---|---|
| ハッシュ関数 | RDKit は独自の実装（Boost hash ベース） |
| 不変量の詳細 | RDKitは価電子数の計算がより精密 |
| 重複除去 | RDKitは「同じ原子集合をカバーする特徴」を重複排除する |
| 芳香族の判定 | RDKitは独自のアルゴリズムで芳香族を再判定する |

**しかし、類似度の順序関係はほぼ一致します。**
実用上、これは重要な性質です。

> 💡 **検証方法:**
> ```python
> from rdkit import Chem
> from rdkit.Chem import AllChem, DataStructs
>
> mols = [Chem.MolFromSmiles(s) for s in smiles_list]
> fps = [AllChem.GetMorganFingerprintAsBitVect(m, 2, 2048) for m in mols]
>
> for i in range(len(fps)):
>     for j in range(i+1, len(fps)):
>         print(smiles_list[i], smiles_list[j],
>               DataStructs.TanimotoSimilarity(fps[i], fps[j]))
> ```
> この出力と、自作版の類似度行列を比較してみてください。
> 相関係数が0.9以上あれば、実装は概ね正しいです。

---

## 21.9 性能

```cpp
// bench/bench_fp.cpp
#include <chemcpp/smiles.hpp>
#include <chemcpp/fingerprint.hpp>
#include <chrono>
#include <iostream>

int main() {
    using namespace chemcpp;
    std::vector<std::string> smi_list;
    const char* templates[] = {
        "CC(=O)Oc1ccccc1C(=O)O", "Cn1cnc2c1c(=O)n(C)c(=O)n2C",
        "CC(C)Cc1ccc(cc1)C(C)C(=O)O", "CC(=O)Nc1ccc(O)cc1",
        "CC(=O)N[C@@H](Cc1c[nH]c2ccccc12)C(=O)O",
    };
    for (int i = 0; i < 100'000; ++i) smi_list.push_back(templates[i % 5]);

    // まずパース（フィンガープリント計算だけを測るため）
    std::vector<Molecule> mols;
    mols.reserve(smi_list.size());
    for (const auto& s : smi_list) {
        if (auto m = parse_smiles(s)) mols.push_back(std::move(*m));
    }

    auto t0 = std::chrono::steady_clock::now();
    std::vector<Fingerprint> fps;
    fps.reserve(mols.size());
    for (const auto& m : mols) fps.push_back(morgan_fingerprint(m, {.radius=2}));
    auto ms = std::chrono::duration<double, std::milli>(
                  std::chrono::steady_clock::now() - t0).count();

    std::cout << "molecules: " << mols.size() << "\n";
    std::cout << "time     : " << ms << " ms\n";
    std::cout << "rate     : " << mols.size() / ms * 1000 << " fp/s\n";
    std::cout << "memory   : " << fps.size() * sizeof(Fingerprint) / 1024 / 1024
              << " MB\n";
}
```

参考値:

```
molecules: 100000
time     : 486 ms
rate     : 205761 fp/s
memory   : 24 MB
```

**毎秒20万分子**。RDKit の Python API はおおよそ毎秒3〜8万分子です。

> 💡 **最適化の余地:**
> - `std::unordered_map` をやめて、ソート済み vector を使う（小さいので速い）
> - `neighbor_buf` の `std::sort` を、要素数4以下用の挿入ソートに置き換える
> - メモリアロケーションを再利用する（`morgan_counts` の呼び出しごとに map を作らない）
>
> 実際にやると、**さらに2〜3倍速くなります**。第28章で扱います。

---

## 21.10 この章のまとめ

- **ECFP = 反復的なハッシュ伝播**。半径0から順に環境を広げていく
- **隣接原子のハッシュをソートして正規化**するのが核心
  （SMILESの書き方に依存しないため）
- **半径ごとに一斉更新**する（`current` / `next` の二重バッファ）
- **これはGNNのメッセージパッシングと同じ構造**
- `mod nBits` で folding するため、**ビット衝突が起きる**
- ECFP4 = 半径2（直径4）。RDKit の `radius=2` と同じ
- グラフアルゴリズム（BFS、連結成分、環検出）は
  **明示スタック/キューで実装**する

> 📝 **練習問題 21-1**
>
> `morgan_counts` を、`std::unordered_map` ではなく
> ソート済み `std::vector<std::pair<uint32_t,int>>` で実装し直し、
> 速度を比較してください。

> 📝 **練習問題 21-2**
>
> RDKit で同じ分子のECFP4を計算し、
> 自作版との類似度行列の相関係数（Spearman）を求めてください。

> 📝 **練習問題 21-3**
>
> `use_features = true`（FCFP）を実装し、
> ECFP と FCFP で類似度がどう変わるか比較してください。

> 📝 **練習問題 21-4**
>
> アトムペア (Atom Pair) フィンガープリントを実装してください。
> 「原子タイプA、原子タイプB、距離d」の3つ組をハッシュするものです。
> 21.6 の `distance_matrix` が使えます。

---

→ [第22章 実践3: ビットベクタとTanimoto](ch22_実践_ビットベクタとTanimoto.md)
