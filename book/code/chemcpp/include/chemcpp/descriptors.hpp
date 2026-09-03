// 第24章: 分子記述子
//
// ⚠️ 教育目的の簡易実装です。本番の解析には RDKit を使ってください。
//    RDKit の Crippen logP は110種類の原子タイプを持ち、はるかに精密です。
#pragma once

#include <chemcpp/molecule.hpp>

#include <string>

namespace chemcpp {

struct Descriptors {
    double mw            = 0.0;
    double heavy_mw      = 0.0;
    int    heavy_atoms   = 0;
    int    hetero_atoms  = 0;
    int    n_rings       = 0;
    int    n_arom_rings  = 0;
    int    hbd           = 0;
    int    hba           = 0;
    int    rotatable     = 0;
    double tpsa          = 0.0;
    double logp          = 0.0;
    double fraction_csp3 = 0.0;
    int    n_stereo      = 0;
    int    formal_charge = 0;
};

[[nodiscard]] double      molecular_weight(const Molecule& m);
[[nodiscard]] std::string molecular_formula(const Molecule& m);
[[nodiscard]] int         num_hbd(const Molecule& m);
[[nodiscard]] int         num_hba(const Molecule& m);
[[nodiscard]] int         num_rotatable_bonds(const Molecule& m);
[[nodiscard]] double      tpsa(const Molecule& m);
[[nodiscard]] double      crippen_logp(const Molecule& m);
[[nodiscard]] double      fraction_csp3(const Molecule& m);

[[nodiscard]] Descriptors compute_descriptors(const Molecule& m);

/// Lipinski の Rule of Five（違反数）
[[nodiscard]] int  lipinski_violations(const Descriptors& d) noexcept;
[[nodiscard]] bool passes_ro5(const Descriptors& d) noexcept;

/// QED の簡易版（0〜1。1に近いほど薬らしい）
[[nodiscard]] double qed_score(const Descriptors& d);

}  // namespace chemcpp
