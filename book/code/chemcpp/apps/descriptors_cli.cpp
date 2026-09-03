// 第24章: 記述子計算CLI
//
// 使い方:
//   ./descriptors_cli <input.smi> [output.csv]
#include <chemcpp/smiles.hpp>
#include <chemcpp/descriptors.hpp>
#include <chemcpp/io.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace chemcpp;

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);

    if (argc < 2) {
        std::cerr << "usage: descriptors_cli <input.smi> [output.csv]\n";
        return 1;
    }
    const std::string in_path  = argv[1];
    const std::string out_path = (argc > 2) ? argv[2] : "descriptors.csv";

    std::ofstream out(out_path);
    if (!out) { std::cerr << "cannot write " << out_path << "\n"; return 1; }

    out << "ID,SMILES,Formula,MW,HeavyAtoms,HeteroAtoms,LogP,TPSA,"
           "HBD,HBA,RotB,Rings,AromRings,FracCsp3,Charge,Ro5Violations,QED\n";
    out << std::fixed;

    const auto t0 = std::chrono::steady_clock::now();
    std::size_t n_ok = 0, n_fail = 0;

    try {
        read_smi(in_path, [&](std::string_view smi, std::string_view id) {
            const auto mol = parse_smiles(smi);
            if (!mol) { ++n_fail; return; }

            const auto d = compute_descriptors(*mol);
            out << (id.empty() ? ("MOL" + std::to_string(n_ok)) : std::string(id))
                << ',' << smi
                << ',' << molecular_formula(*mol)
                << ',' << std::setprecision(3) << d.mw
                << ',' << d.heavy_atoms
                << ',' << d.hetero_atoms
                << ',' << std::setprecision(3) << d.logp
                << ',' << std::setprecision(2) << d.tpsa
                << ',' << d.hbd
                << ',' << d.hba
                << ',' << d.rotatable
                << ',' << d.n_rings
                << ',' << d.n_arom_rings
                << ',' << std::setprecision(3) << d.fraction_csp3
                << ',' << d.formal_charge
                << ',' << lipinski_violations(d)
                << ',' << std::setprecision(3) << qed_score(d)
                << '\n';                       // ★ std::endl は使わない（遅い）
            ++n_ok;
        });
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0).count();

    std::cerr << std::fixed << std::setprecision(1)
              << "processed " << n_ok << " molecules (" << n_fail
              << " failed) in " << ms << " ms\n";
    if (ms > 0) {
        std::cerr << "rate: " << std::setprecision(0) << (n_ok / ms * 1000.0)
                  << " mol/s\n";
    }
    std::cerr << "output: " << out_path << "\n";
    return 0;
}
