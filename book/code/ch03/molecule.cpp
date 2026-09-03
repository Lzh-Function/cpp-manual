// 第3章: 実装（定義）
#include "molecule.hpp"
#include <cctype>

int count_heavy_atoms(const std::string& smiles) {
    int n = 0;
    for (char c : smiles) {
        if (std::isupper(static_cast<unsigned char>(c))) ++n;
    }
    return n;
}
