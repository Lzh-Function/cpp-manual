// 第21章 + 第28章: Morgan フィンガープリント（最適化済み実装）
//
// 第21章の素朴な実装から、
//   - unordered_map → ソート済み vector
//   - 作業バッファの使い回し
//   - 小さい配列には挿入ソート
// を適用したもの。手元では約4倍速い。
#include <chemcpp/fingerprint.hpp>
#include <chemcpp/element.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace chemcpp {
namespace {

/// Boost の hash_combine と同じ方式
inline std::uint32_t hash_combine(std::uint32_t seed, std::uint32_t value) noexcept {
    seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
}

/// MurmurHash3 の finalizer（よく混ざる、速い）
inline std::uint32_t hash_u32(std::uint32_t x) noexcept {
    x ^= x >> 16;
    x *= 0x85ebca6bu;
    x ^= x >> 13;
    x *= 0xc2b2ae35u;
    x ^= x >> 16;
    return x;
}

/// 隣接原子は通常 1〜4 個。std::sort より挿入ソートが速い
inline void sort_small(std::vector<std::pair<std::uint32_t, std::uint32_t>>& v) {
    for (std::size_t i = 1; i < v.size(); ++i) {
        const auto key = v[i];
        std::size_t j = i;
        while (j > 0 && key < v[j - 1]) { v[j] = v[j - 1]; --j; }
        v[j] = key;
    }
}

std::uint32_t initial_invariant(const Molecule& mol, std::size_t idx,
                                const MorganOptions& opt) {
    const Atom& a = mol.atom(idx);

    if (opt.use_features) {
        // FCFP: 薬理特徴でビットを立てる
        std::uint32_t feat = 0;
        const bool n_or_o = (a.atomic_num == 7 || a.atomic_num == 8);
        if (n_or_o && a.num_h > 0)                    feat |= 1u << 0;  // HBD
        if (n_or_o)                                   feat |= 1u << 1;  // HBA
        if (a.aromatic)                               feat |= 1u << 2;  // Aromatic
        if (a.charge > 0)                             feat |= 1u << 3;  // Positive
        if (a.charge < 0)                             feat |= 1u << 4;  // Negative
        if (a.atomic_num == 6  || a.atomic_num == 9 ||
            a.atomic_num == 17 || a.atomic_num == 35) feat |= 1u << 5;  // Hydrophobe
        return hash_u32(feat);
    }

    // ECFP: Daylight 型の不変量
    int heavy_deg = 0;
    int bond_sum  = 0;
    for (const auto& nb : mol.neighbors(idx)) {
        if (mol.atom(nb.atom).atomic_num == 1) continue;
        ++heavy_deg;
        switch (mol.bond(nb.bond).order) {
            case BondOrder::Single:    bond_sum += 1; break;
            case BondOrder::Double:    bond_sum += 2; break;
            case BondOrder::Triple:    bond_sum += 3; break;
            case BondOrder::Quadruple: bond_sum += 4; break;
            case BondOrder::Aromatic:  bond_sum += 1; break;
            default:                   bond_sum += 1; break;
        }
    }

    std::uint32_t h = 0;
    h = hash_combine(h, static_cast<std::uint32_t>(heavy_deg));
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

}  // anonymous namespace

std::vector<std::uint32_t>
morgan_hashes(const Molecule& mol, const MorganOptions& opt, MorganWorkspace& ws) {
    const std::size_t n = mol.num_atoms();
    ws.features.clear();
    if (n == 0) return {};

    ws.current.assign(n, 0);
    ws.next.assign(n, 0);
    ws.features.reserve(n * static_cast<std::size_t>(opt.radius + 1));

    // --- 半径0: 初期不変量 ---
    for (std::size_t i = 0; i < n; ++i) {
        ws.current[i] = initial_invariant(mol, i, opt);
        ws.features.push_back(ws.current[i]);
    }

    // --- 半径 1..radius ---
    for (int r = 1; r <= opt.radius; ++r) {
        for (std::size_t i = 0; i < n; ++i) {
            ws.nbrs.clear();
            for (const auto& nb : mol.neighbors(i)) {
                if (mol.atom(nb.atom).atomic_num == 1) continue;
                ws.nbrs.emplace_back(
                    static_cast<std::uint32_t>(mol.bond(nb.bond).order),
                    ws.current[nb.atom]);
            }
            // ★ 核心: 隣接の順序を正規化する（SMILES の書き方に依存させない）
            sort_small(ws.nbrs);

            std::uint32_t h = hash_combine(static_cast<std::uint32_t>(r),
                                           ws.current[i]);
            for (const auto& [bo, nh] : ws.nbrs) {
                h = hash_combine(h, bo);
                h = hash_combine(h, nh);
            }
            ws.next[i] = hash_u32(h);
        }
        // ★ 一斉更新（1原子ずつ更新すると値が混ざる）
        ws.current.swap(ws.next);
        for (std::size_t i = 0; i < n; ++i) ws.features.push_back(ws.current[i]);
    }

    std::sort(ws.features.begin(), ws.features.end());
    ws.features.erase(std::unique(ws.features.begin(), ws.features.end()),
                      ws.features.end());
    return ws.features;
}

std::vector<std::uint32_t>
morgan_hashes(const Molecule& mol, const MorganOptions& opt) {
    MorganWorkspace ws;
    return morgan_hashes(mol, opt, ws);
}

Fingerprint morgan_fingerprint(const Molecule& mol, const MorganOptions& opt,
                               MorganWorkspace& ws) {
    Fingerprint fp{};
    const auto hashes = morgan_hashes(mol, opt, ws);
    const std::size_t nbits = (opt.n_bits > 0)
                            ? static_cast<std::size_t>(opt.n_bits) : FP_BITS;
    for (std::uint32_t h : hashes) {
        set_bit(fp, h % nbits);          // folding（ビット衝突がありうる）
    }
    return fp;
}

Fingerprint morgan_fingerprint(const Molecule& mol, const MorganOptions& opt) {
    MorganWorkspace ws;
    return morgan_fingerprint(mol, opt, ws);
}

}  // namespace chemcpp
