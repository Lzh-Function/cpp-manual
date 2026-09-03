// 第23章: FingerprintDB の実装
#include <chemcpp/database.hpp>

#include <algorithm>
#include <fstream>
#include <numeric>
#include <stdexcept>

namespace chemcpp {
namespace {

constexpr std::uint32_t MAGIC   = 0x43464442u;   // "CFDB"
constexpr std::uint32_t VERSION = 1u;

struct Header {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t n_words;
    std::uint32_t reserved;
    std::uint64_t n_molecules;
};
static_assert(sizeof(Header) == 24, "header layout changed; bump VERSION");

}  // anonymous namespace

void FingerprintDB::sort_by_count() {
    const std::size_t n = size();
    if (n < 2) return;

    std::vector<std::uint32_t> order(n);
    std::iota(order.begin(), order.end(), 0u);
    std::sort(order.begin(), order.end(),
              [this](std::uint32_t a, std::uint32_t b) {
                  return counts_[a] < counts_[b];
              });

    std::vector<std::uint64_t> new_fps(n * WORDS);
    std::vector<int>           new_counts(n);
    std::vector<std::string>   new_ids(n);

    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t src = order[k];
        std::copy_n(fps_.data() + src * WORDS, WORDS, new_fps.data() + k * WORDS);
        new_counts[k] = counts_[src];
        new_ids[k]    = std::move(ids_[src]);
    }

    fps_    = std::move(new_fps);
    counts_ = std::move(new_counts);
    ids_    = std::move(new_ids);
}

std::pair<std::size_t, std::size_t>
FingerprintDB::count_range(int lo, int hi) const {
    const auto b = std::lower_bound(counts_.begin(), counts_.end(), lo);
    const auto e = std::upper_bound(counts_.begin(), counts_.end(), hi);
    return {static_cast<std::size_t>(b - counts_.begin()),
            static_cast<std::size_t>(e - counts_.begin())};
}

void FingerprintDB::save(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot open for write: " + path);

    const Header h{MAGIC, VERSION, static_cast<std::uint32_t>(WORDS), 0,
                   static_cast<std::uint64_t>(size())};
    out.write(reinterpret_cast<const char*>(&h), sizeof(h));

    out.write(reinterpret_cast<const char*>(fps_.data()),
              static_cast<std::streamsize>(fps_.size() * sizeof(std::uint64_t)));
    out.write(reinterpret_cast<const char*>(counts_.data()),
              static_cast<std::streamsize>(counts_.size() * sizeof(int)));

    for (const auto& id : ids_) {
        const auto len = static_cast<std::uint16_t>(
            std::min<std::size_t>(id.size(), 65535));
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        out.write(id.data(), len);
    }
    if (!out) throw std::runtime_error("write failed: " + path);
}

void FingerprintDB::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open: " + path);

    Header h{};
    in.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (!in)                  throw std::runtime_error("truncated header: " + path);
    if (h.magic != MAGIC)     throw std::runtime_error("bad magic: " + path);
    if (h.version != VERSION) throw std::runtime_error("unsupported version");
    if (h.n_words != WORDS)   throw std::runtime_error("fingerprint size mismatch");

    const auto n = static_cast<std::size_t>(h.n_molecules);
    fps_.assign(n * WORDS, 0);
    counts_.assign(n, 0);
    ids_.clear();
    ids_.reserve(n);

    in.read(reinterpret_cast<char*>(fps_.data()),
            static_cast<std::streamsize>(fps_.size() * sizeof(std::uint64_t)));
    in.read(reinterpret_cast<char*>(counts_.data()),
            static_cast<std::streamsize>(counts_.size() * sizeof(int)));

    std::string buf;
    for (std::size_t i = 0; i < n; ++i) {
        std::uint16_t len = 0;
        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        buf.assign(len, '\0');
        if (len) in.read(buf.data(), len);
        ids_.push_back(buf);
    }
    if (!in) throw std::runtime_error("read failed: " + path);
}

}  // namespace chemcpp
