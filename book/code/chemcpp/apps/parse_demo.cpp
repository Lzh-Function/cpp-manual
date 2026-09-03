// 第20章: SMILESパーサのデモ
//
// ビルド:  cmake --build build && ./build/parse_demo
//
// std::format は環境によって使えないので、ここでは iostream を使う。
#include <chemcpp/smiles.hpp>
#include <chemcpp/element.hpp>
#include <chemcpp/descriptors.hpp>
#include <chemcpp/graph.hpp>

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace chemcpp;

namespace {

const char* order_str(BondOrder o) {
    switch (o) {
        case BondOrder::Single:    return "-";
        case BondOrder::Double:    return "=";
        case BondOrder::Triple:    return "#";
        case BondOrder::Quadruple: return "$";
        case BondOrder::Aromatic:  return ":";
        default:                   return "?";
    }
}

void dump(const std::string& smiles, bool verbose) {
    SmilesError err;
    const auto mol = parse_smiles(smiles, &err);

    std::cout << "=== " << smiles << " ===\n";
    if (!mol) {
        std::cout << "  PARSE ERROR: " << err.what() << "\n\n";
        return;
    }

    std::cout << "  atoms      : " << mol->num_atoms() << "\n";
    std::cout << "  bonds      : " << mol->num_bonds() << "\n";
    std::cout << "  components : " << num_components(*mol) << "\n";
    std::cout << "  rings      : " << ring_count(*mol) << "\n";
    std::cout << "  formula    : " << molecular_formula(*mol) << "\n";
    std::cout << "  MW         : " << std::fixed << std::setprecision(3)
              << molecular_weight(*mol) << "\n";

    const auto d = compute_descriptors(*mol);
    std::cout << "  logP       : " << std::setprecision(2) << d.logp << "\n";
    std::cout << "  TPSA       : " << d.tpsa << "\n";
    std::cout << "  HBD/HBA    : " << d.hbd << " / " << d.hba << "\n";
    std::cout << "  RotB       : " << d.rotatable << "\n";
    std::cout << "  Ro5 viol.  : " << lipinski_violations(d) << "\n";
    std::cout << "  QED        : " << std::setprecision(3) << qed_score(d) << "\n";

    if (verbose) {
        std::cout << "  atoms:\n";
        for (std::size_t i = 0; i < mol->num_atoms(); ++i) {
            const auto& a = mol->atom(i);
            std::cout << "    [" << std::setw(2) << i << "] "
                      << std::left << std::setw(3) << Element::symbol(a.atomic_num)
                      << std::right
                      << " H" << static_cast<int>(a.num_h)
                      << " q" << std::showpos << static_cast<int>(a.charge)
                      << std::noshowpos
                      << (a.aromatic ? " arom" : "     ")
                      << (a.in_ring  ? " ring" : "     ")
                      << " deg=" << mol->degree(i) << "\n";
        }
        std::cout << "  bonds:\n";
        for (std::size_t i = 0; i < mol->num_bonds(); ++i) {
            const auto& b = mol->bond(i);
            std::cout << "    " << b.begin << order_str(b.order) << b.end
                      << (b.in_ring ? "  (ring)" : "") << "\n";
        }
    }
    std::cout << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);

    // 引数があればそれをパースする
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) dump(argv[i], true);
        return 0;
    }

    const std::vector<std::string> tests = {
        "CCO",                                     // エタノール
        "c1ccccc1",                                // ベンゼン
        "CC(=O)Oc1ccccc1C(=O)O",                   // アスピリン
        "Cn1cnc2c1c(=O)n(C)c(=O)n2C",              // カフェイン
        "CC(C)Cc1ccc(cc1)C(C)C(=O)O",              // イブプロフェン
        "CC(=O)Nc1ccc(O)cc1",                      // パラセタモール
        "C[C@@H](N)C(=O)O",                        // L-アラニン
        "[Na+].[Cl-]",                             // 塩化ナトリウム
        "c1ccc2ccccc2c1",                          // ナフタレン
        "C1CC2CCC1CC2",                            // ビシクロ[2.2.2]オクタン
        "O=C(O)c1ccccc1O",                         // サリチル酸
        "CC(=O)N[C@@H](Cc1c[nH]c2ccccc12)C(=O)O",  // N-アセチルトリプトファン
        "ClCCBr",                                  // 2文字元素
        "[NH4+]",                                  // アンモニウム
        "[13CH4]",                                 // 同位体
        // --- エラーになるはずの入力 ---
        "CC(=O",                                   // 括弧が閉じていない
        "CCO)",                                    // 余分な閉じ括弧
        "c1cccc",                                  // 環が閉じていない
        "CCXO",                                    // 未知の文字
        "[Xyz]",                                   // 未知の元素
    };

    for (const auto& s : tests) dump(s, false);
    return 0;
}
