// 第24章: ファイル入出力の実装
#include <chemcpp/io.hpp>

#include <cctype>
#include <fstream>
#include <stdexcept>

namespace chemcpp {

void read_smi(const std::string& path,
              const std::function<void(std::string_view, std::string_view)>& cb,
              char delimiter, bool has_header) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);

    // ★ 大きなバッファでシステムコールを減らす（デフォルトは数KB）
    std::vector<char> buffer(1u << 20);   // 1MB
    in.rdbuf()->pubsetbuf(buffer.data(),
                          static_cast<std::streamsize>(buffer.size()));

    std::string line;
    line.reserve(256);

    if (has_header) std::getline(in, line);

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();   // CRLF
        if (line.empty() || line[0] == '#') continue;

        const std::size_t pos = line.find(delimiter);
        if (pos == std::string::npos) {
            cb(line, std::string_view{});
        } else {
            const std::string_view sv(line);
            cb(sv.substr(0, pos), trim(sv.substr(pos + 1)));
        }
    }
}

std::vector<SmiRecord> read_smi_all(const std::string& path,
                                    char delimiter, bool has_header) {
    std::vector<SmiRecord> out;
    out.reserve(1024);
    read_smi(path, [&](std::string_view smi, std::string_view id) {
        out.push_back({std::string(smi), std::string(id)});
    }, delimiter, has_header);
    return out;
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("cannot open " + path);

    const std::streamsize size = in.tellg();
    if (size < 0) throw std::runtime_error("cannot determine size of " + path);

    std::string content(static_cast<std::size_t>(size), '\0');
    in.seekg(0);
    in.read(content.data(), size);
    return content;
}

std::vector<std::string_view> split_lines(std::string_view text) {
    std::vector<std::string_view> lines;
    lines.reserve(text.size() / 40 + 1);      // 平均40文字と仮定

    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) end = text.size();
        std::size_t len = end - start;
        if (len > 0 && text[start + len - 1] == '\r') --len;    // CRLF
        lines.push_back(text.substr(start, len));
        start = end + 1;
    }
    return lines;
}

std::vector<std::string_view> split(std::string_view s, char delim) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t pos = s.find(delim, start);
        if (pos == std::string_view::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

std::string_view trim(std::string_view s) {
    auto is_space = [](char c) {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    };
    while (!s.empty() && is_space(s.front())) s.remove_prefix(1);
    while (!s.empty() && is_space(s.back()))  s.remove_suffix(1);
    return s;
}

}  // namespace chemcpp
