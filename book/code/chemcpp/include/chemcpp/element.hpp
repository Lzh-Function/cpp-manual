// 第20章: 元素データテーブル（すべて constexpr = 実行時コストゼロ）
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace chemcpp {

class Element {
public:
    static constexpr int MAX_Z = 118;

    static constexpr std::string_view symbol(int z) noexcept {
        return (z >= 0 && z <= MAX_Z) ? SYMBOLS[static_cast<std::size_t>(z)] : "??";
    }

    static constexpr double weight(int z) noexcept {
        return (z >= 0 && z <= MAX_Z) ? WEIGHTS[static_cast<std::size_t>(z)] : 0.0;
    }

    /// 元素記号から原子番号を得る。見つからなければ 0。
    static constexpr int number(std::string_view sym) noexcept {
        for (int z = 1; z <= MAX_Z; ++z) {
            if (SYMBOLS[static_cast<std::size_t>(z)] == sym) return z;
        }
        return 0;
    }

    /// 標準原子価（暗黙の水素数の計算に使う）
    static constexpr int default_valence(int z) noexcept {
        switch (z) {
            case 1:  return 1;   // H
            case 5:  return 3;   // B
            case 6:  return 4;   // C
            case 7:  return 3;   // N
            case 8:  return 2;   // O
            case 9:  return 1;   // F
            case 15: return 3;   // P
            case 16: return 2;   // S
            case 17: return 1;   // Cl
            case 35: return 1;   // Br
            case 53: return 1;   // I
            default: return 0;   // 不明なものには水素を付けない
        }
    }

    /// 有機部分集合（ブラケットなしで書ける元素）か
    static constexpr bool is_organic_subset(int z) noexcept {
        switch (z) {
            case 5: case 6: case 7: case 8: case 9:
            case 15: case 16: case 17: case 35: case 53:
                return true;
            default:
                return false;
        }
    }

private:
    static constexpr std::array<std::string_view, MAX_Z + 1> SYMBOLS = {
        "",
        "H",  "He", "Li", "Be", "B",  "C",  "N",  "O",  "F",  "Ne",
        "Na", "Mg", "Al", "Si", "P",  "S",  "Cl", "Ar", "K",  "Ca",
        "Sc", "Ti", "V",  "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn",
        "Ga", "Ge", "As", "Se", "Br", "Kr", "Rb", "Sr", "Y",  "Zr",
        "Nb", "Mo", "Tc", "Ru", "Rh", "Pd", "Ag", "Cd", "In", "Sn",
        "Sb", "Te", "I",  "Xe", "Cs", "Ba", "La", "Ce", "Pr", "Nd",
        "Pm", "Sm", "Eu", "Gd", "Tb", "Dy", "Ho", "Er", "Tm", "Yb",
        "Lu", "Hf", "Ta", "W",  "Re", "Os", "Ir", "Pt", "Au", "Hg",
        "Tl", "Pb", "Bi", "Po", "At", "Rn", "Fr", "Ra", "Ac", "Th",
        "Pa", "U",  "Np", "Pu", "Am", "Cm", "Bk", "Cf", "Es", "Fm",
        "Md", "No", "Lr", "Rf", "Db", "Sg", "Bh", "Hs", "Mt", "Ds",
        "Rg", "Cn", "Nh", "Fl", "Mc", "Lv", "Ts", "Og"
    };

    static constexpr std::array<double, MAX_Z + 1> WEIGHTS = {
        0.0,
        1.008,   4.0026,  6.94,    9.0122,  10.81,   12.011,  14.007,  15.999,
        18.998,  20.180,  22.990,  24.305,  26.982,  28.085,  30.974,  32.06,
        35.45,   39.948,  39.098,  40.078,  44.956,  47.867,  50.942,  51.996,
        54.938,  55.845,  58.933,  58.693,  63.546,  65.38,   69.723,  72.630,
        74.922,  78.971,  79.904,  83.798,  85.468,  87.62,   88.906,  91.224,
        92.906,  95.95,   98.0,    101.07,  102.91,  106.42,  107.87,  112.41,
        114.82,  118.71,  121.76,  127.60,  126.90,  131.29,  132.91,  137.33,
        138.91,  140.12,  140.91,  144.24,  145.0,   150.36,  151.96,  157.25,
        158.93,  162.50,  164.93,  167.26,  168.93,  173.05,  174.97,  178.49,
        180.95,  183.84,  186.21,  190.23,  192.22,  195.08,  196.97,  200.59,
        204.38,  207.2,   208.98,  209.0,   210.0,   222.0,   223.0,   226.0,
        227.0,   232.04,  231.04,  238.03,  237.0,   244.0,   243.0,   247.0,
        247.0,   251.0,   252.0,   257.0,   258.0,   259.0,   266.0,   267.0,
        268.0,   269.0,   270.0,   269.0,   278.0,   281.0,   282.0,   285.0,
        286.0,   289.0,   290.0,   293.0,   294.0,   294.0
    };
};

}  // namespace chemcpp
