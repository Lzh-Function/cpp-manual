// 第23章: 類似度検索エンジン
#pragma once

#include <chemcpp/database.hpp>

#include <cstdint>
#include <vector>

namespace chemcpp {

struct Hit {
    double        score;
    std::uint32_t index;

    bool operator<(const Hit& o) const noexcept { return score < o.score; }
    bool operator>(const Hit& o) const noexcept { return score > o.score; }
};

/// 閾値検索: score >= threshold のものを、スコア降順で全部返す。
/// use_pruning=true なら min(a,b)/max(a,b) の上界で候補を絞る
/// （事前に db.sort_by_count() を呼んでおくこと）。
[[nodiscard]] std::vector<Hit>
search_threshold(const FingerprintDB& db, const Fingerprint& query,
                 double threshold, bool use_pruning = true);

/// Top-K 検索（動的枝刈り付き。O(n log K)、メモリ O(K)）
[[nodiscard]] std::vector<Hit>
search_topk(const FingerprintDB& db, const Fingerprint& query, std::size_t k);

/// マルチスレッド版の閾値検索（n_threads=0 で全コア）
[[nodiscard]] std::vector<Hit>
search_threshold_parallel(const FingerprintDB& db, const Fingerprint& query,
                          double threshold, unsigned n_threads = 0);

}  // namespace chemcpp
