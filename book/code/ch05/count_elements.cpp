// 第5章: SMILES から元素をカウントする
// g++ -std=c++20 -O2 -Wall -Wextra count_elements.cpp -o count_elements
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

/// 元素記号を原子番号に変換（主要な有機元素のみ）
int atomic_number(char c) {
    switch (c) {
        case 'C': case 'c': return 6;
        case 'N': case 'n': return 7;
        case 'O': case 'o': return 8;
        case 'S': case 's': return 16;
        case 'P': case 'p': return 15;
        case 'F':           return 9;
        case 'B': case 'b': return 5;
        case 'I':           return 53;
        default:            return 0;
    }
}

/// 括弧の対応が取れているか
bool brackets_balanced(const std::string& smiles) {
    int paren = 0, square = 0;
    for (char c : smiles) {
        if      (c == '(') ++paren;
        else if (c == ')') --paren;
        else if (c == '[') ++square;
        else if (c == ']') --square;
        if (paren < 0 || square < 0) return false;   // 閉じすぎ
    }
    return paren == 0 && square == 0;
}

/// 重原子数を数える（簡易版。ブラケット原子は1個と数える）
int count_heavy_atoms(const std::string& smiles) {
    int  count      = 0;
    bool in_bracket = false;

    for (std::size_t i = 0; i < smiles.size(); ++i) {
        const char c = smiles[i];

        if (c == '[') { in_bracket = true;  ++count; continue; }
        if (c == ']') { in_bracket = false;          continue; }
        if (in_bracket) continue;

        if (atomic_number(c) != 0) {
            // Cl, Br の2文字元素に対応
            if (c == 'C' && i + 1 < smiles.size() && smiles[i + 1] == 'l') ++i;
            if (c == 'B' && i + 1 < smiles.size() && smiles[i + 1] == 'r') ++i;
            ++count;
        }
    }
    return count;
}

/// 分岐の最大ネスト深さ
int max_branch_depth(const std::string& smiles) {
    int depth = 0, max_depth = 0;
    for (char c : smiles) {
        if      (c == '(') { ++depth; if (depth > max_depth) max_depth = depth; }
        else if (c == ')') --depth;
    }
    return max_depth;
}

/// 芳香族原子（小文字）の数
int count_aromatic(const std::string& smiles) {
    int n = 0;
    bool in_bracket = false;
    for (char c : smiles) {
        if (c == '[') { in_bracket = true;  continue; }
        if (c == ']') { in_bracket = false; continue; }
        if (in_bracket) continue;
        if (std::string_view("bcnops").find(c) != std::string_view::npos) ++n;
    }
    return n;
}

int main() {
    const std::vector<std::string> tests = {
        "CCO",                          // エタノール
        "c1ccccc1",                     // ベンゼン
        "CC(=O)Oc1ccccc1C(=O)O",        // アスピリン
        "Cn1cnc2c1c(=O)n(C)c(=O)n2C",   // カフェイン
        "CC(C)Cc1ccc(cc1)C(C)C(=O)O",   // イブプロフェン
        "ClCCBr",                       // 2文字元素
        "CC[NH3+]",                     // ブラケット原子
        "CC(=O)Oc1ccccc1C(=O)O)",       // 壊れたSMILES
    };

    for (const auto& s : tests) {
        std::cout << s << "\n";
        std::cout << "  balanced    : " << (brackets_balanced(s) ? "yes" : "NO") << "\n";
        std::cout << "  heavy atoms : " << count_heavy_atoms(s) << "\n";
        std::cout << "  aromatic    : " << count_aromatic(s) << "\n";
        std::cout << "  branch depth: " << max_branch_depth(s) << "\n\n";
    }
    return 0;
}
