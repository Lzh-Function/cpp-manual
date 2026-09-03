// 第8章: string_view による SMILES トークナイザ（コピーゼロ）
// g++ -std=c++20 -O2 -Wall smiles_tokens.cpp -o smiles_tokens
#include <cctype>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

enum class TokenKind {
    Atom, Bond, RingBond, BranchOpen, BranchClose, Dot, Unknown
};

struct Token {
    TokenKind        kind;
    std::string_view text;   // 元文字列への「窓」。コピーしていない
};

const char* kind_name(TokenKind k) {
    switch (k) {
        case TokenKind::Atom:        return "Atom";
        case TokenKind::Bond:        return "Bond";
        case TokenKind::RingBond:    return "Ring";
        case TokenKind::BranchOpen:  return "(";
        case TokenKind::BranchClose: return ")";
        case TokenKind::Dot:         return ".";
        default:                     return "?";
    }
}

bool is_two_char_element(char a, char b) {
    return (a == 'C' && b == 'l') || (a == 'B' && b == 'r');
}

/// ⚠️ 戻り値の string_view は smiles を指しているので、
///    smiles が生きている間だけ有効（ダングリングに注意）
std::vector<Token> tokenize(std::string_view smiles) {
    std::vector<Token> tokens;
    tokens.reserve(smiles.size());          // 上限は文字数

    std::size_t i = 0;
    while (i < smiles.size()) {
        const char c = smiles[i];

        // --- ブラケット原子 [nH], [Fe+2], [13C] ---
        if (c == '[') {
            const std::size_t close = smiles.find(']', i);
            if (close == std::string_view::npos) {
                tokens.push_back({TokenKind::Unknown, smiles.substr(i)});
                break;
            }
            tokens.push_back({TokenKind::Atom, smiles.substr(i, close - i + 1)});
            i = close + 1;
            continue;
        }

        // --- 2文字元素 ---
        if (i + 1 < smiles.size() && is_two_char_element(c, smiles[i + 1])) {
            tokens.push_back({TokenKind::Atom, smiles.substr(i, 2)});
            i += 2;
            continue;
        }

        // --- 有機部分集合の1文字原子 ---
        if (std::string_view("BCNOPSFI").find(c) != std::string_view::npos ||
            std::string_view("bcnops").find(c)   != std::string_view::npos) {
            tokens.push_back({TokenKind::Atom, smiles.substr(i, 1)});
            ++i;
            continue;
        }

        // --- 結合記号 ---
        if (std::string_view("-=#$:/\\").find(c) != std::string_view::npos) {
            tokens.push_back({TokenKind::Bond, smiles.substr(i, 1)});
            ++i;
            continue;
        }

        // --- 環結合番号（%NN 形式も対応）---
        if (c == '%') {
            std::size_t n = 1;
            while (i + n < smiles.size() &&
                   std::isdigit(static_cast<unsigned char>(smiles[i + n]))) ++n;
            tokens.push_back({TokenKind::RingBond, smiles.substr(i, n)});
            i += n;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back({TokenKind::RingBond, smiles.substr(i, 1)});
            ++i;
            continue;
        }

        // --- 構造記号 ---
        if (c == '(') { tokens.push_back({TokenKind::BranchOpen,  smiles.substr(i,1)}); ++i; continue; }
        if (c == ')') { tokens.push_back({TokenKind::BranchClose, smiles.substr(i,1)}); ++i; continue; }
        if (c == '.') { tokens.push_back({TokenKind::Dot,         smiles.substr(i,1)}); ++i; continue; }

        tokens.push_back({TokenKind::Unknown, smiles.substr(i, 1)});
        ++i;
    }
    return tokens;
}

int main() {
    const std::vector<std::string> tests = {
        "CCO",
        "CC(=O)Oc1ccccc1C(=O)O",
        "Cn1cnc2c1c(=O)n(C)c(=O)n2C",
        "[Na+].[Cl-]",
        "C[C@H](N)C(=O)O",
        "ClCCBr",
        "c1ccc2c(c1)cccc2",
        "C%10CCCC%10",
    };

    for (const auto& s : tests) {
        std::cout << "=== " << s << " ===\n";
        const auto tokens = tokenize(s);        // s は生きているので安全
        for (const auto& t : tokens) {
            std::cout << "  " << std::left << std::setw(5) << kind_name(t.kind)
                      << std::right << " : " << t.text << "\n";
        }
        std::cout << "  total tokens: " << tokens.size() << "\n\n";
    }
    return 0;
}
