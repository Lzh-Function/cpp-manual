// 第3章: 最初のプログラム
// g++ -std=c++20 -O2 -Wall -Wextra hello.cpp -o hello
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::vector<std::string> smiles = {
        "CCO",                     // エタノール
        "c1ccccc1",                // ベンゼン
        "CC(=O)Oc1ccccc1C(=O)O"    // アスピリン
    };

    for (const auto& s : smiles) {
        std::cout << s << "  (length = " << s.size() << ")\n";
    }
    return 0;
}
