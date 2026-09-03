// 第24章: ファイル入出力
//
// コールバック方式（ストリーミング）でメモリ使用量を一定に保つ。
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace chemcpp {

struct SmiRecord {
    std::string smiles;
    std::string id;
};

/// .smi ファイルを1行ずつコールバックに渡す（メモリに全部載せない）
void read_smi(const std::string& path,
              const std::function<void(std::string_view smiles,
                                       std::string_view id)>& callback,
              char delimiter = '\t', bool has_header = false);

/// 全部読んで vector に入れる（小さいファイル用）
[[nodiscard]] std::vector<SmiRecord>
read_smi_all(const std::string& path, char delimiter = '\t',
             bool has_header = false);

/// ファイル全体を1つの文字列として読む（一気読み。getline より数倍速い）
[[nodiscard]] std::string read_file(const std::string& path);

/// 文字列を行に分割（string_view なのでコピーゼロ。元の文字列が生きている必要あり）
[[nodiscard]] std::vector<std::string_view> split_lines(std::string_view text);

/// 区切り文字で分割（string_view 版。コピーなし）
[[nodiscard]] std::vector<std::string_view> split(std::string_view s, char delim);

/// 前後の空白を除去
[[nodiscard]] std::string_view trim(std::string_view s);

}  // namespace chemcpp
