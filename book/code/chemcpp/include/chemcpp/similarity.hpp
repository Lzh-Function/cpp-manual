// 第22章: ビットベクタ演算と類似度指標
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// ★ 注意: __cpp_lib_bitops は <bit> (または <version>) を include して
//    初めて定義される。先に include してから判定しないと常に偽になる。
#if defined(__has_include)
  #if __has_include(<bit>)
    #include <bit>
  #endif
#endif

#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
  #define CHEMCPP_POPCOUNT(x) std::popcount(x)
#elif defined(__GNUC__) || defined(__clang__)
  #define CHEMCPP_POPCOUNT(x) static_cast<int>(__builtin_popcountll(x))
#elif defined(_MSC_VER)
  #include <intrin.h>
  #define CHEMCPP_POPCOUNT(x) static_cast<int>(__popcnt64(x))
#else
  // 移植用フォールバック（SWAR 法）
  inline int chemcpp_popcount_fallback(std::uint64_t x) {
      x = x - ((x >> 1) & 0x5555555555555555ULL);
      x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
      x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
      return static_cast<int>((x * 0x0101010101010101ULL) >> 56);
  }
  #define CHEMCPP_POPCOUNT(x) chemcpp_popcount_fallback(x)
#endif

namespace chemcpp {

inline constexpr std::size_t FP_BITS  = 2048;
inline constexpr std::size_t FP_WORDS = FP_BITS / 64;   // 32

using Fingerprint = std::array<std::uint64_t, FP_WORDS>;
static_assert(sizeof(Fingerprint) == 256, "Fingerprint must be 256 bytes");

// ---------------- ビット操作 ----------------
inline void set_bit(Fingerprint& fp, std::size_t bit) noexcept {
    fp[bit / 64] |= (1ULL << (bit % 64));
}
inline void clear_bit(Fingerprint& fp, std::size_t bit) noexcept {
    fp[bit / 64] &= ~(1ULL << (bit % 64));
}
inline bool get_bit(const Fingerprint& fp, std::size_t bit) noexcept {
    return ((fp[bit / 64] >> (bit % 64)) & 1ULL) != 0;
}

// ---------------- 基本統計量 ----------------
[[nodiscard]] inline int popcount(const Fingerprint& a) noexcept {
    int n = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) n += CHEMCPP_POPCOUNT(a[i]);
    return n;
}

[[nodiscard]] inline int intersection(const Fingerprint& a,
                                      const Fingerprint& b) noexcept {
    int n = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) n += CHEMCPP_POPCOUNT(a[i] & b[i]);
    return n;
}

[[nodiscard]] inline int union_count(const Fingerprint& a,
                                     const Fingerprint& b) noexcept {
    int n = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) n += CHEMCPP_POPCOUNT(a[i] | b[i]);
    return n;
}

// ---------------- 類似度指標 ----------------

/// Tanimoto (Jaccard): c / (a + b - c)
[[nodiscard]] inline double tanimoto(const Fingerprint& x,
                                     const Fingerprint& y) noexcept {
    int inter = 0, uni = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        inter += CHEMCPP_POPCOUNT(x[i] & y[i]);
        uni   += CHEMCPP_POPCOUNT(x[i] | y[i]);
    }
    return uni ? static_cast<double>(inter) / uni : 0.0;
}

/// popcount を事前計算済みの高速版（ループ内の popcount が半分になる）
[[nodiscard]] inline double tanimoto_fast(const Fingerprint& x, int cx,
                                          const Fingerprint& y, int cy) noexcept {
    int inter = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) inter += CHEMCPP_POPCOUNT(x[i] & y[i]);
    const int uni = cx + cy - inter;
    return uni ? static_cast<double>(inter) / uni : 0.0;
}

/// Dice (Sorensen): 2c / (a + b)
[[nodiscard]] inline double dice(const Fingerprint& x,
                                 const Fingerprint& y) noexcept {
    int inter = 0, total = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        inter += CHEMCPP_POPCOUNT(x[i] & y[i]);
        total += CHEMCPP_POPCOUNT(x[i]) + CHEMCPP_POPCOUNT(y[i]);
    }
    return total ? 2.0 * inter / total : 0.0;
}

/// Cosine (Ochiai): c / sqrt(a * b)
[[nodiscard]] inline double cosine(const Fingerprint& x,
                                   const Fingerprint& y) noexcept {
    int inter = 0, na = 0, nb = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        inter += CHEMCPP_POPCOUNT(x[i] & y[i]);
        na    += CHEMCPP_POPCOUNT(x[i]);
        nb    += CHEMCPP_POPCOUNT(y[i]);
    }
    const double denom = std::sqrt(static_cast<double>(na) * static_cast<double>(nb));
    return denom > 0.0 ? inter / denom : 0.0;
}

/// Tversky: c / (alpha*(a-c) + beta*(b-c) + c)
/// alpha=beta=1.0 で Tanimoto、alpha=beta=0.5 で Dice
[[nodiscard]] inline double tversky(const Fingerprint& x, const Fingerprint& y,
                                    double alpha, double beta) noexcept {
    int inter = 0, na = 0, nb = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        inter += CHEMCPP_POPCOUNT(x[i] & y[i]);
        na    += CHEMCPP_POPCOUNT(x[i]);
        nb    += CHEMCPP_POPCOUNT(y[i]);
    }
    const double denom = alpha * (na - inter) + beta * (nb - inter) + inter;
    return denom > 0.0 ? inter / denom : 0.0;
}

/// Hamming 距離（異なるビットの数）
[[nodiscard]] inline int hamming(const Fingerprint& x,
                                 const Fingerprint& y) noexcept {
    int n = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) n += CHEMCPP_POPCOUNT(x[i] ^ y[i]);
    return n;
}

/// x のビットがすべて y にも立っているか（部分構造スクリーニング用）
[[nodiscard]] inline bool is_subset(const Fingerprint& x,
                                    const Fingerprint& y) noexcept {
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        if (x[i] & ~y[i]) return false;
    }
    return true;
}

}  // namespace chemcpp
