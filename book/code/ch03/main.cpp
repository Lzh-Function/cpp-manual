// 第3章: 複数ファイルのビルド
// g++ -std=c++20 -O2 main.cpp molecule.cpp -o mol
#include "molecule.hpp"
#include <iostream>

int main() {
    const std::string aspirin = "CC(=O)Oc1ccccc1C(=O)O";
    std::cout << "heavy atoms (approx) = "
              << count_heavy_atoms(aspirin) << "\n";
    return 0;
}
