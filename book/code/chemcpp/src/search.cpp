// 第23章: 検索エンジンの実装
#include <chemcpp/search.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <thread>
#include <tuple>

#if defined(_MSC_VER)
  #define CHEM_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
  #define CHEM_RESTRICT __restrict__
#else
  #define CHEM_RESTRICT
#endif

namespace chemcpp {
namespace {

/// AND の popcount だけを計算するホットループ。
/// 単純なループにしておくと、-O3 -march=native で自動ベクトル化される。
inline int intersect(const std::uint64_t* CHEM_RESTRICT a,
                     const std::uint64_t* CHEM_RESTRICT b) noexcept {
    int n = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        n += CHEMCPP_POPCOUNT(a[i] & b[i]);
    }
    return n;
}

}  // anonymous namespace

std::vector<Hit> search_threshold(const FingerprintDB& db,
                                  const Fingerprint& query,
                                  double threshold, bool use_pruning) {
    std::vector<Hit> hits;
    const int qc = popcount(query);
    if (qc == 0 || db.empty()) return hits;

    std::size_t begin = 0, end = db.size();

    if (use_pruning && threshold > 0.0) {
        // T <= min(a,b)/max(a,b) より、候補は qc*T <= c <= qc/T の範囲だけ
        const int lo = static_cast<int>(std::ceil (qc * threshold));
        const int hi = static_cast<int>(std::floor(qc / threshold));
        std::tie(begin, end) = db.count_range(lo, hi);
    }

    const std::uint64_t* q = query.data();
    for (std::size_t i = begin; i < end; ++i) {
        const int c   = intersect(q, db.fp_ptr(i));
        const int uni = qc + db.count(i) - c;
        if (uni == 0) continue;
        const double t = static_cast<double>(c) / uni;
        if (t >= threshold) hits.push_back({t, static_cast<std::uint32_t>(i)});
    }

    // 決定的な順序にする（スコア降順、同点ならインデックス昇順）
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.index < b.index;
    });
    return hits;
}

std::vector<Hit> search_topk(const FingerprintDB& db,
                             const Fingerprint& query, std::size_t k) {
    if (k == 0 || db.empty()) return {};

    const int qc = popcount(query);
    if (qc == 0) return {};
    const std::uint64_t* q = query.data();

    // 最小ヒープ: top() が「現在の K 位」
    std::priority_queue<Hit, std::vector<Hit>, std::greater<Hit>> heap;

    for (std::size_t i = 0; i < db.size(); ++i) {
        const int dbc = db.count(i);
        if (dbc == 0) continue;

        // ★ 動的枝刈り: 上界が現在のK位以下なら計算不要
        if (heap.size() == k) {
            const int mn = std::min(qc, dbc);
            const int mx = std::max(qc, dbc);
            if (static_cast<double>(mn) / mx <= heap.top().score) continue;
        }

        const int c   = intersect(q, db.fp_ptr(i));
        const int uni = qc + dbc - c;
        if (uni == 0) continue;
        const double t = static_cast<double>(c) / uni;

        if (heap.size() < k) {
            heap.push({t, static_cast<std::uint32_t>(i)});
        } else if (t > heap.top().score) {
            heap.pop();
            heap.push({t, static_cast<std::uint32_t>(i)});
        }
    }

    std::vector<Hit> out;
    out.reserve(heap.size());
    while (!heap.empty()) { out.push_back(heap.top()); heap.pop(); }
    std::reverse(out.begin(), out.end());     // 降順にする
    return out;
}

std::vector<Hit> search_threshold_parallel(const FingerprintDB& db,
                                           const Fingerprint& query,
                                           double threshold,
                                           unsigned n_threads) {
    const int qc = popcount(query);
    if (qc == 0 || db.empty()) return {};

    if (n_threads == 0) n_threads = std::thread::hardware_concurrency();
    if (n_threads == 0) n_threads = 4;

    const int lo = static_cast<int>(std::ceil (qc * threshold));
    const int hi = static_cast<int>(std::floor(qc / threshold));
    const auto [begin, end] = db.count_range(lo, hi);

    const std::size_t total = end - begin;
    if (total < 10000) return search_threshold(db, query, threshold);

    // ★ スレッドごとに別の vector に集める（ロック競合を避ける）
    std::vector<std::vector<Hit>> local(n_threads);
    {
        std::vector<std::thread> threads;
        threads.reserve(n_threads);
        const std::size_t chunk = (total + n_threads - 1) / n_threads;

        for (unsigned t = 0; t < n_threads; ++t) {
            const std::size_t s = begin + static_cast<std::size_t>(t) * chunk;
            const std::size_t e = std::min(s + chunk, end);
            if (s >= e) break;

            threads.emplace_back([&db, &query, &local, qc, threshold, s, e, t] {
                const std::uint64_t* q = query.data();
                auto& out = local[t];
                for (std::size_t i = s; i < e; ++i) {
                    const int c   = intersect(q, db.fp_ptr(i));
                    const int uni = qc + db.count(i) - c;
                    if (uni == 0) continue;
                    const double sc = static_cast<double>(c) / uni;
                    if (sc >= threshold)
                        out.push_back({sc, static_cast<std::uint32_t>(i)});
                }
            });
        }
        for (auto& th : threads) th.join();
    }

    std::vector<Hit> hits;
    std::size_t n = 0;
    for (const auto& v : local) n += v.size();
    hits.reserve(n);
    for (const auto& v : local) hits.insert(hits.end(), v.begin(), v.end());

    // 決定的な順序にする（スコア降順、同点ならインデックス昇順）
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.index < b.index;
    });
    return hits;
}

}  // namespace chemcpp
