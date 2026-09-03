// 第21〜22章: フィンガープリントと類似度のデモ
#include <chemcpp/smiles.hpp>
#include <chemcpp/fingerprint.hpp>
#include <chemcpp/similarity.hpp>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace chemcpp;

int main() {
    std::ios::sync_with_stdio(false);

    struct Entry { const char* name; const char* smiles; };
    const std::vector<Entry> mols = {
        {"aspirin",        "CC(=O)Oc1ccccc1C(=O)O"},
        {"salicylic",      "O=C(O)c1ccccc1O"},
        {"paracetamol",    "CC(=O)Nc1ccc(O)cc1"},
        {"ibuprofen",      "CC(C)Cc1ccc(cc1)C(C)C(=O)O"},
        {"caffeine",       "Cn1cnc2c1c(=O)n(C)c(=O)n2C"},
        {"benzene",        "c1ccccc1"},
        {"toluene",        "Cc1ccccc1"},
        {"phenol",         "Oc1ccccc1"},
        {"ethanol",        "CCO"},
    };

    std::vector<Fingerprint> fps;
    std::vector<std::string> names;
    MorganWorkspace ws;

    std::cout << "name             atoms  bits  features\n";
    std::cout << "---------------- -----  ----  --------\n";

    for (const auto& e : mols) {
        const auto mol = parse_smiles(e.smiles);
        if (!mol) { std::cerr << "parse failed: " << e.smiles << "\n"; continue; }

        const MorganOptions opt{.radius = 2, .n_bits = 2048};
        const auto fp = morgan_fingerprint(*mol, opt, ws);

        fps.push_back(fp);
        names.emplace_back(e.name);

        std::cout << std::left << std::setw(17) << e.name << std::right
                  << std::setw(5) << mol->num_atoms()
                  << std::setw(6) << popcount(fp)
                  << std::setw(10) << morgan_hashes(*mol, opt, ws).size() << "\n";
    }

    // --- 類似度行列 ---
    std::cout << "\nTanimoto similarity matrix\n";
    std::cout << std::setw(17) << "";
    for (const auto& n : names) std::cout << std::setw(9) << n.substr(0, 8);
    std::cout << "\n";

    std::cout << std::fixed << std::setprecision(3);
    for (std::size_t i = 0; i < fps.size(); ++i) {
        std::cout << std::left << std::setw(17) << names[i] << std::right;
        for (std::size_t j = 0; j < fps.size(); ++j) {
            std::cout << std::setw(9) << tanimoto(fps[i], fps[j]);
        }
        std::cout << "\n";
    }

    // --- 指標の比較 ---
    std::cout << "\naspirin vs salicylic acid\n";
    std::cout << "  tanimoto        : " << tanimoto(fps[0], fps[1]) << "\n";
    std::cout << "  dice            : " << dice(fps[0], fps[1]) << "\n";
    std::cout << "  cosine          : " << cosine(fps[0], fps[1]) << "\n";
    std::cout << "  tversky(.9,.1)  : " << tversky(fps[0], fps[1], 0.9, 0.1) << "\n";
    std::cout << "  tversky(.1,.9)  : " << tversky(fps[0], fps[1], 0.1, 0.9) << "\n";
    std::cout << "  hamming         : " << hamming(fps[0], fps[1]) << "\n";

    // --- 半径の効果 ---
    std::cout << "\neffect of radius (aspirin)\n";
    const auto aspirin = parse_smiles("CC(=O)Oc1ccccc1C(=O)O");
    if (!aspirin) { std::cerr << "unexpected parse failure\n"; return 1; }
    for (int r = 0; r <= 4; ++r) {
        const MorganOptions opt{.radius = r};
        const auto fp = morgan_fingerprint(*aspirin, opt, ws);
        std::cout << "  radius=" << r
                  << "  bits set=" << std::setw(3) << popcount(fp)
                  << "  features=" << morgan_hashes(*aspirin, opt, ws).size() << "\n";
    }

    // --- ビット数と衝突 ---
    std::cout << "\neffect of n_bits (aspirin, radius=2)\n";
    for (int nb : {256, 512, 1024, 2048, 4096}) {
        const MorganOptions opt{.radius = 2, .n_bits = nb};
        const auto hashes = morgan_hashes(*aspirin, opt, ws);
        std::vector<char> bits(static_cast<std::size_t>(nb), 0);
        for (std::uint32_t h : hashes) bits[h % static_cast<std::size_t>(nb)] = 1;
        int set = 0;
        for (char b : bits) set += b;
        std::cout << "  n_bits=" << std::setw(5) << nb
                  << "  features=" << std::setw(3) << hashes.size()
                  << "  bits set=" << std::setw(3) << set
                  << "  collisions=" << (static_cast<int>(hashes.size()) - set)
                  << "\n";
    }
    return 0;
}
