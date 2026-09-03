// 第20章: SMILES パーサの公開インタフェース
//
// 外部には例外を漏らさず std::optional を返す。
// → 100万件の一括処理でも例外コストがかからない（第18章）
#pragma once

#include <chemcpp/molecule.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace chemcpp {

struct SmilesError {
    std::string message;
    std::size_t position = 0;

    std::string what() const {
        return message + " (at position " + std::to_string(position) + ")";
    }
};

/// SMILES をパースして分子グラフを構築する。
/// 失敗時は std::nullopt を返し、err が非nullならエラー詳細を書き込む。
[[nodiscard]] std::optional<Molecule>
parse_smiles(std::string_view smiles, SmilesError* err = nullptr);

}  // namespace chemcpp
