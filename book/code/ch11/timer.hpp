// 第11章 / 第28章: RAII によるスコープタイマーとプロファイラ
//
// 使い方:
//   void f() {
//       CHEM_PROFILE("f");
//       ...
//   }   // ← ここで自動的に時間が記録される
//
//   int main() {
//       f();
//       chemcpp::Profiler::instance().report();
//   }
//
// -DCHEMCPP_ENABLE_PROFILING を付けたときだけ有効になる。
#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace chemcpp {

class Profiler {
public:
    static Profiler& instance() {
        static Profiler p;          // C++11 以降、静的ローカル変数の初期化はスレッドセーフ
        return p;
    }

    void record(const std::string& name, double ms) {
        const std::lock_guard<std::mutex> lock(mutex_);
        auto& e = entries_[name];
        e.total_ms += ms;
        e.count    += 1;
        if (ms > e.max_ms) e.max_ms = ms;
    }

    void report(std::ostream& os = std::cerr) const {
        const std::lock_guard<std::mutex> lock(mutex_);
        double grand_total = 0.0;
        for (const auto& [k, v] : entries_) grand_total += v.total_ms;

        os << "\n=== Profile ===\n";
        os << std::left << std::setw(26) << "name" << std::right
           << std::setw(9)  << "calls"
           << std::setw(14) << "total(ms)"
           << std::setw(11) << "avg(ms)"
           << std::setw(11) << "max(ms)"
           << std::setw(8)  << "%" << "\n";

        os << std::fixed;
        for (const auto& [name, e] : entries_) {
            os << std::left << std::setw(26) << name << std::right
               << std::setw(9)  << e.count
               << std::setw(14) << std::setprecision(2) << e.total_ms
               << std::setw(11) << (e.count ? e.total_ms / e.count : 0.0)
               << std::setw(11) << e.max_ms
               << std::setw(8)  << std::setprecision(1)
               << (grand_total > 0.0 ? e.total_ms / grand_total * 100.0 : 0.0)
               << "\n";
        }
    }

    void clear() {
        const std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

private:
    struct Entry {
        double    total_ms = 0.0;
        double    max_ms   = 0.0;
        long long count    = 0;
    };
    mutable std::mutex           mutex_;
    std::map<std::string, Entry> entries_;
};

/// RAII タイマー: コンストラクタで計測開始、デストラクタで記録
class ScopedTimer {
public:
    explicit ScopedTimer(std::string name)
        : name_(std::move(name)),
          start_(std::chrono::steady_clock::now()) {}

    ~ScopedTimer() {
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - start_).count();
        Profiler::instance().record(name_, ms);
    }

    // コピー・ムーブを禁止（2回発火してしまうため）
    ScopedTimer(const ScopedTimer&)            = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&)                 = delete;
    ScopedTimer& operator=(ScopedTimer&&)      = delete;

private:
    std::string                           name_;
    std::chrono::steady_clock::time_point start_;
};

}  // namespace chemcpp

// マクロの中で __LINE__ を展開するための二段構え
#define CHEM_CONCAT_INNER(a, b) a##b
#define CHEM_CONCAT(a, b) CHEM_CONCAT_INNER(a, b)

#ifdef CHEMCPP_ENABLE_PROFILING
  #define CHEM_PROFILE(name) \
      ::chemcpp::ScopedTimer CHEM_CONCAT(_chem_timer_, __LINE__)(name)
#else
  #define CHEM_PROFILE(name) ((void)0)
#endif
