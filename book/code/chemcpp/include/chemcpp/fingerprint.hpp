// 第21章: Morgan (ECFP) フィンガープリント
#pragma once

#include <chemcpp/molecule.hpp>
#include <chemcpp/similarity.hpp>   // Fingerprint, set_bit

#include <cstdint>
#include <utility>
#include <vector>

namespace chemcpp {

struct MorganOptions {
    int  radius        = 2;                              // ECFP4 なら 2
    int  n_bits        = static_cast<int>(FP_BITS);      // 2048
    bool use_chirality = false;
    bool use_features  = false;                          // true なら FCFP
};

/// 作業用バッファ。ループで使い回すとヒープ確保が消える（第28章）
struct MorganWorkspace {
    std::vector<std::uint32_t> current;
    std::vector<std::uint32_t> next;
    std::vector<std::uint32_t> features;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> nbrs;
};

/// 生のハッシュ値（folding 前）を昇順・重複除去して返す
[[nodiscard]] std::vector<std::uint32_t>
morgan_hashes(const Molecule& mol, const MorganOptions& opt = {});

[[nodiscard]] std::vector<std::uint32_t>
morgan_hashes(const Molecule& mol, const MorganOptions& opt, MorganWorkspace& ws);

/// ビットベクタ版 Morgan フィンガープリント
[[nodiscard]] Fingerprint
morgan_fingerprint(const Molecule& mol, const MorganOptions& opt = {});

[[nodiscard]] Fingerprint
morgan_fingerprint(const Molecule& mol, const MorganOptions& opt,
                   MorganWorkspace& ws);

}  // namespace chemcpp
