// 第3章: ヘッダ（宣言）
#pragma once
#include <string>

/// SMILES 中の大文字を数えて重原子数を粗く推定する（超簡易版）
int count_heavy_atoms(const std::string& smiles);
