// 第3章: 分子情報の表示
// g++ -std=c++20 -O2 -Wall mol_info.cpp -o mol_info
#include <iostream>
#include <string>
#include <vector>

int main() {
    const std::vector<std::string> names = {
        "Ethanol", "Benzene", "Aspirin", "Caffeine"
    };
    const std::vector<std::string> smiles = {
        "CCO",
        "c1ccccc1",
        "CC(=O)Oc1ccccc1C(=O)O",
        "Cn1cnc2c1c(=O)n(C)c(=O)n2C"
    };

    std::cout << "Name        SMILES                        Len  Rings\n";
    std::cout << "----------  ----------------------------  ---  -----\n";

    for (std::size_t i = 0; i < names.size(); ++i) {
        int ring_digits = 0;
        for (char c : smiles[i]) {
            if (c >= '1' && c <= '9') ++ring_digits;
        }

        std::cout << names[i];
        for (std::size_t k = names[i].size(); k < 12; ++k) std::cout << ' ';
        std::cout << smiles[i];
        for (std::size_t k = smiles[i].size(); k < 30; ++k) std::cout << ' ';
        std::cout << smiles[i].size() << "    " << ring_digits / 2 << "\n";
    }
    return 0;
}
