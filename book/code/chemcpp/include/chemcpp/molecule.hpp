// 第20章: 分子グラフのデータ構造
//
// 設計方針:
//   - すべて std::vector で連続配置（キャッシュ効率）
//   - ポインタではなくインデックス（uint32_t）で参照
//     → 循環参照なし、再確保に強い、メモリ半分、そのままバイナリ保存できる
//   - enum class + uint8_t で型安全 + 省メモリ
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace chemcpp {

enum class BondOrder : std::uint8_t {
    Unspecified = 0,
    Single      = 1,
    Double      = 2,
    Triple      = 3,
    Quadruple   = 4,
    Aromatic    = 5,
};

enum class Chirality : std::uint8_t { None = 0, CW = 1, CCW = 2 };

struct Atom {
    std::uint8_t atomic_num = 0;
    std::int8_t  charge     = 0;
    std::uint8_t num_h      = 0;      // 明示 or 暗黙の水素数
    std::uint8_t isotope    = 0;
    bool         aromatic   = false;
    bool         in_ring    = false;
    bool         explicit_h = false;  // [CH3] のように水素数が明示されたか
    Chirality    chirality  = Chirality::None;
};
static_assert(sizeof(Atom) == 8, "Atom should stay compact");

struct Bond {
    std::uint32_t begin   = 0;
    std::uint32_t end     = 0;
    BondOrder     order   = BondOrder::Single;
    bool          in_ring = false;
};

class Molecule {
public:
    struct Neighbor {
        std::uint32_t atom;   // 隣接原子のインデックス
        std::uint32_t bond;   // その結合のインデックス
    };

    // ---- 構築 ----
    std::uint32_t add_atom(const Atom& a) {
        atoms_.push_back(a);
        adj_.emplace_back();
        return static_cast<std::uint32_t>(atoms_.size() - 1);
    }

    void add_bond(std::uint32_t i, std::uint32_t j, BondOrder order) {
        const auto bi = static_cast<std::uint32_t>(bonds_.size());
        bonds_.push_back({i, j, order, false});
        adj_[i].push_back({j, bi});
        adj_[j].push_back({i, bi});
    }

    void reserve(std::size_t n_atoms, std::size_t n_bonds) {
        atoms_.reserve(n_atoms);
        adj_.reserve(n_atoms);
        bonds_.reserve(n_bonds);
    }

    // ---- 参照 ----
    std::size_t num_atoms() const noexcept { return atoms_.size(); }
    std::size_t num_bonds() const noexcept { return bonds_.size(); }

    const Atom& atom(std::size_t i) const noexcept { return atoms_[i]; }
    Atom&       atom(std::size_t i)       noexcept { return atoms_[i]; }
    const Bond& bond(std::size_t i) const noexcept { return bonds_[i]; }
    Bond&       bond(std::size_t i)       noexcept { return bonds_[i]; }

    const std::vector<Atom>& atoms() const noexcept { return atoms_; }
    const std::vector<Bond>& bonds() const noexcept { return bonds_; }

    const std::vector<Neighbor>& neighbors(std::size_t i) const noexcept {
        return adj_[i];
    }
    std::size_t degree(std::size_t i) const noexcept { return adj_[i].size(); }

    /// 水素以外の隣接原子の数
    int heavy_degree(std::size_t i) const noexcept {
        int d = 0;
        for (const auto& nb : adj_[i]) {
            if (atoms_[nb.atom].atomic_num > 1) ++d;
        }
        return d;
    }

    const std::string& name() const noexcept { return name_; }
    void set_name(std::string n) { name_ = std::move(n); }

private:
    std::string                        name_;
    std::vector<Atom>                  atoms_;
    std::vector<Bond>                  bonds_;
    std::vector<std::vector<Neighbor>> adj_;   // 隣接リスト
};

/// 結合次数を数値に（芳香族は 1.5）
inline double order_value(BondOrder o) noexcept {
    switch (o) {
        case BondOrder::Single:    return 1.0;
        case BondOrder::Double:    return 2.0;
        case BondOrder::Triple:    return 3.0;
        case BondOrder::Quadruple: return 4.0;
        case BondOrder::Aromatic:  return 1.5;
        default:                   return 1.0;
    }
}

}  // namespace chemcpp
