// 第24章: 分子記述子の実装
#include <chemcpp/descriptors.hpp>
#include <chemcpp/element.hpp>
#include <chemcpp/graph.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace chemcpp {

// ---------------------------------------------------------------
double molecular_weight(const Molecule& m) {
    double mw = 0.0;
    for (const auto& a : m.atoms()) {
        mw += Element::weight(a.atomic_num);
        mw += a.num_h * Element::weight(1);
    }
    return mw;
}

// ---------------------------------------------------------------
std::string molecular_formula(const Molecule& m) {
    std::array<int, 119> counts{};
    for (const auto& a : m.atoms()) {
        if (a.atomic_num < counts.size()) ++counts[a.atomic_num];
        counts[1] += a.num_h;
    }

    std::string out;
    auto append = [&](std::size_t z) {
        if (counts[z] == 0) return;
        out += Element::symbol(static_cast<int>(z));
        if (counts[z] > 1) out += std::to_string(counts[z]);
    };

    // Hill 順序: C, H, その後アルファベット順
    const bool has_carbon = counts[6] > 0;
    if (has_carbon) { append(6); append(1); }

    std::vector<std::pair<std::string_view, int>> rest;
    for (std::size_t z = 1; z < counts.size(); ++z) {
        if (counts[z] == 0) continue;
        if (z == 6) continue;
        if (has_carbon && z == 1) continue;
        rest.emplace_back(Element::symbol(static_cast<int>(z)), counts[z]);
    }
    std::sort(rest.begin(), rest.end());
    for (const auto& [sym, n] : rest) {
        out += sym;
        if (n > 1) out += std::to_string(n);
    }
    return out;
}

// ---------------------------------------------------------------
int num_hbd(const Molecule& m) {
    int n = 0;
    for (const auto& a : m.atoms()) {
        if ((a.atomic_num == 7 || a.atomic_num == 8) && a.num_h > 0) ++n;
    }
    return n;
}

int num_hba(const Molecule& m) {
    int n = 0;
    for (const auto& a : m.atoms()) {
        if (a.atomic_num != 7 && a.atomic_num != 8) continue;
        if (a.charge > 0) continue;                        // 4級アンモニウムなど
        if (a.atomic_num == 7 && a.aromatic && a.num_h > 0) continue;  // [nH]
        ++n;
    }
    return n;
}

// ---------------------------------------------------------------
int num_rotatable_bonds(const Molecule& m) {
    int n = 0;
    for (const auto& b : m.bonds()) {
        if (b.order != BondOrder::Single) continue;
        if (b.in_ring) continue;
        if (m.heavy_degree(b.begin) < 2 || m.heavy_degree(b.end) < 2) continue;

        // アミド結合 C(=O)-N を除外
        auto is_amide = [&](std::uint32_t c_idx, std::uint32_t n_idx) {
            if (m.atom(c_idx).atomic_num != 6 || m.atom(n_idx).atomic_num != 7)
                return false;
            for (const auto& nb : m.neighbors(c_idx)) {
                if (m.atom(nb.atom).atomic_num == 8 &&
                    m.bond(nb.bond).order == BondOrder::Double) return true;
            }
            return false;
        };
        if (is_amide(b.begin, b.end) || is_amide(b.end, b.begin)) continue;

        ++n;
    }
    return n;
}

// ---------------------------------------------------------------
// TPSA (Ertl et al. 2000) の主要な原子タイプのみ
double tpsa(const Molecule& m) {
    double total = 0.0;

    for (std::size_t i = 0; i < m.num_atoms(); ++i) {
        const Atom& a = m.atom(i);
        if (a.atomic_num != 7 && a.atomic_num != 8) continue;

        int n_double = 0, n_triple = 0, n_aromatic = 0;
        for (const auto& nb : m.neighbors(i)) {
            switch (m.bond(nb.bond).order) {
                case BondOrder::Double:   ++n_double;   break;
                case BondOrder::Triple:   ++n_triple;   break;
                case BondOrder::Aromatic: ++n_aromatic; break;
                default: break;
            }
        }
        const int h = a.num_h;
        const int q = a.charge;

        double contrib = 0.0;
        if (a.atomic_num == 7) {
            if (a.aromatic) {
                if      (n_aromatic == 2 && h == 0 && q == 0) contrib = 12.89;
                else if (n_aromatic == 2 && h == 1 && q == 0) contrib = 15.79;
                else if (n_aromatic >= 3 && h == 0 && q == 0) contrib =  4.41;
                else if (q == 1)                              contrib = 14.14;
                else                                          contrib = 12.89;
            } else if (q > 0) {
                if      (h == 3) contrib = 27.64;
                else if (h == 2) contrib = 25.59;
                else if (h == 1) contrib = 19.03;
                else             contrib =  0.00;
            } else {
                if      (n_triple == 1)           contrib = 23.79;
                else if (n_double == 1 && h == 0) contrib = 12.36;
                else if (n_double == 1 && h == 1) contrib = 23.85;
                else if (h == 2)                  contrib = 26.02;
                else if (h == 1)                  contrib = 12.03;
                else                              contrib =  3.24;
            }
        } else {   // 酸素
            if      (a.aromatic)      contrib = 13.14;
            else if (q < 0)           contrib = 23.06;
            else if (n_double == 1)   contrib = 17.07;
            else if (h == 1)          contrib = 20.23;
            else                      contrib =  9.23;
        }
        total += contrib;
    }
    return total;
}

// ---------------------------------------------------------------
// Crippen logP（原子寄与法の簡易版）
double crippen_logp(const Molecule& m) {
    double logp = 0.0;

    for (std::size_t i = 0; i < m.num_atoms(); ++i) {
        const Atom& a = m.atom(i);
        double c = 0.0;

        switch (a.atomic_num) {
            case 6: {
                if (a.aromatic) {
                    bool hetero_nb = false;
                    for (const auto& nb : m.neighbors(i)) {
                        const auto z = m.atom(nb.atom).atomic_num;
                        if (z != 6 && z != 1) { hetero_nb = true; break; }
                    }
                    c = hetero_nb ? 0.1360 : 0.1581;
                } else {
                    int hetero = 0;
                    for (const auto& nb : m.neighbors(i)) {
                        const auto z = m.atom(nb.atom).atomic_num;
                        if (z != 6 && z != 1) ++hetero;
                    }
                    if      (hetero == 0) c =  0.1441;
                    else if (hetero == 1) c =  0.0000;
                    else                  c = -0.2035;
                }
                break;
            }
            case 7:
                if      (a.aromatic)   c = -0.3187;
                else if (a.num_h == 2) c = -1.0190;
                else if (a.num_h == 1) c = -0.7096;
                else                   c = -0.3187;
                if (a.charge > 0)      c -= 0.5;
                break;
            case 8:
                if      (a.aromatic)   c =  0.1552;
                else if (a.num_h == 1) c = -0.2893;
                else                   c =  0.1552;
                if (a.charge < 0)      c -= 1.0;
                break;
            case 9:  c = 0.4202; break;
            case 15: c = 0.8612; break;
            case 16: c = 0.6482; break;
            case 17: c = 0.6895; break;
            case 35: c = 0.8456; break;
            case 53: c = 0.8857; break;
            default: c = 0.0000; break;
        }
        logp += c;
        logp += a.num_h * 0.1230;      // 結合水素の寄与
    }
    return logp;
}

// ---------------------------------------------------------------
double fraction_csp3(const Molecule& m) {
    int n_carbon = 0, n_sp3 = 0;
    for (std::size_t i = 0; i < m.num_atoms(); ++i) {
        const Atom& a = m.atom(i);
        if (a.atomic_num != 6) continue;
        ++n_carbon;
        if (a.aromatic) continue;

        bool has_multiple = false;
        for (const auto& nb : m.neighbors(i)) {
            const auto o = m.bond(nb.bond).order;
            if (o == BondOrder::Double || o == BondOrder::Triple ||
                o == BondOrder::Aromatic) { has_multiple = true; break; }
        }
        if (!has_multiple) ++n_sp3;
    }
    return n_carbon ? static_cast<double>(n_sp3) / n_carbon : 0.0;
}

// ---------------------------------------------------------------
Descriptors compute_descriptors(const Molecule& m) {
    Descriptors d;

    for (const auto& a : m.atoms()) {
        if (a.atomic_num > 1) ++d.heavy_atoms;
        if (a.atomic_num != 6 && a.atomic_num != 1) ++d.hetero_atoms;
        if (a.chirality != Chirality::None) ++d.n_stereo;
        d.formal_charge += a.charge;
        d.heavy_mw      += Element::weight(a.atomic_num);
    }

    d.mw            = molecular_weight(m);
    d.hbd           = num_hbd(m);
    d.hba           = num_hba(m);
    d.rotatable     = num_rotatable_bonds(m);
    d.tpsa          = tpsa(m);
    d.logp          = crippen_logp(m);
    d.fraction_csp3 = fraction_csp3(m);
    d.n_rings       = ring_count(m);

    int arom_bonds = 0;
    for (const auto& b : m.bonds())
        if (b.order == BondOrder::Aromatic) ++arom_bonds;
    d.n_arom_rings = arom_bonds / 6;      // 6員環を仮定した粗い近似

    return d;
}

// ---------------------------------------------------------------
int lipinski_violations(const Descriptors& d) noexcept {
    int v = 0;
    if (d.mw   > 500.0) ++v;
    if (d.logp > 5.0)   ++v;
    if (d.hbd  > 5)     ++v;
    if (d.hba  > 10)    ++v;
    return v;
}

bool passes_ro5(const Descriptors& d) noexcept {
    return lipinski_violations(d) <= 1;
}

// ---------------------------------------------------------------
double qed_score(const Descriptors& d) {
    auto desirability = [](double x, double best, double lo, double hi) {
        const double s = (x < best) ? (best - lo) / 2.0 : (hi - best) / 2.0;
        if (s <= 0.0) return 0.0;
        const double t = (x - best) / s;
        return std::exp(-0.5 * t * t);
    };

    const double parts[] = {
        desirability(d.mw,                             300.0, 150.0, 500.0),
        desirability(d.logp,                             2.5,  -2.0,   6.0),
        desirability(static_cast<double>(d.hba),         4.0,   0.0,  12.0),
        desirability(static_cast<double>(d.hbd),         1.5,   0.0,   6.0),
        desirability(d.tpsa,                            70.0,   0.0, 150.0),
        desirability(static_cast<double>(d.rotatable),   4.0,   0.0,  12.0),
        desirability(static_cast<double>(d.n_arom_rings),2.0,   0.0,   5.0),
    };

    double prod = 1.0;
    for (double p : parts) prod *= p;
    return std::pow(prod, 1.0 / 7.0);
}

}  // namespace chemcpp
