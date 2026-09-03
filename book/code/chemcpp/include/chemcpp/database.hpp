// 第23章: 大規模フィンガープリントDB
//
// SoA（フラット配列）でメモリレイアウトを最適化し、
// popcount を事前計算しておく。
#pragma once

#include <chemcpp/similarity.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace chemcpp {

class FingerprintDB {
public:
    static constexpr std::size_t WORDS = FP_WORDS;   // 32

    void reserve(std::size_t n) {
        fps_.reserve(n * WORDS);
        counts_.reserve(n);
        ids_.reserve(n);
    }

    std::uint32_t add(const Fingerprint& fp, std::string id) {
        fps_.insert(fps_.end(), fp.begin(), fp.end());
        counts_.push_back(popcount(fp));
        ids_.push_back(std::move(id));
        return static_cast<std::uint32_t>(counts_.size() - 1);
    }

    std::size_t size() const noexcept { return counts_.size(); }
    bool        empty() const noexcept { return counts_.empty(); }

    /// 分子 i のフィンガープリント先頭ポインタ（コピーなし）
    const std::uint64_t* fp_ptr(std::size_t i) const noexcept {
        return fps_.data() + i * WORDS;
    }
    std::span<const std::uint64_t> fp_span(std::size_t i) const noexcept {
        return {fp_ptr(i), WORDS};
    }

    int                count(std::size_t i) const noexcept { return counts_[i]; }
    const std::string& id(std::size_t i)    const noexcept { return ids_[i]; }

    const std::vector<int>& counts() const noexcept { return counts_; }

    /// ビット数でソートして枝刈りを可能にする（構築後に1回呼ぶ）
    void sort_by_count();

    /// ビット数が [lo, hi] の範囲にあるインデックス区間 [begin, end)
    std::pair<std::size_t, std::size_t> count_range(int lo, int hi) const;

    /// バイナリ形式での保存・読み込み（再計算は数分、読み込みは数百ミリ秒）
    void save(const std::string& path) const;
    void load(const std::string& path);

private:
    std::vector<std::uint64_t> fps_;      // フラット配列: size() * WORDS
    std::vector<int>           counts_;   // 事前計算した popcount
    std::vector<std::string>   ids_;
};

}  // namespace chemcpp
