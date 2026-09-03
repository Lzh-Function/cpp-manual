# 第22章 実践3: ビットベクタと Tanimoto 係数

第1章で「11,000倍速くなる」と書いた、その中身を解剖します。
ビット演算とキャッシュを理解すれば、あなたも書けます。

## 22.1 ビット演算の基礎

C++のビット演算子:

| 演算子 | 意味 | 例 |
|---|---|---|
| `&` | AND（積集合） | `0b1100 & 0b1010 == 0b1000` |
| `\|` | OR（和集合） | `0b1100 \| 0b1010 == 0b1110` |
| `^` | XOR（対称差） | `0b1100 ^ 0b1010 == 0b0110` |
| `~` | NOT（補集合） | `~0b1100 == 0b...0011` |
| `<<` | 左シフト | `1 << 3 == 8` |
| `>>` | 右シフト | `8 >> 3 == 1` |

**集合演算とビット演算の対応:**

```
   A = {0, 2, 3}   →  0b1101   (ビット0,2,3が立っている)
   B = {1, 2}      →  0b0110

   A ∩ B  = A & B  =  0b0100  = {2}
   A ∪ B  = A | B  =  0b1111  = {0,1,2,3}
   A △ B  = A ^ B  =  0b1011  = {0,1,3}
```

**フィンガープリントは集合**です。だからビット演算がそのまま集合演算になります。

### ビットの立て方・読み方

```cpp
std::uint64_t x = 0;

x |= (1ULL << 5);           // ビット5を立てる
x &= ~(1ULL << 5);          // ビット5を落とす
x ^= (1ULL << 5);           // ビット5を反転
bool b = (x >> 5) & 1ULL;   // ビット5を読む
bool c = x & (1ULL << 5);   // 同じ（0以外なら true）
```

> ⚠️ **`1 << 63` は未定義動作です。**
> `1` は `int`（32bit）なので、63ビットシフトはオーバーフローします。
> **必ず `1ULL << 63` と書いてください**（`ULL` = unsigned long long）。
> これは実際によくあるバグです。

### 2048ビットを64ビット×32個で表す

```cpp
using Fingerprint = std::array<std::uint64_t, 32>;   // 32 * 64 = 2048 bit

inline void set_bit(Fingerprint& fp, std::size_t bit) {
    fp[bit / 64] |= (1ULL << (bit % 64));
}
inline bool get_bit(const Fingerprint& fp, std::size_t bit) {
    return (fp[bit / 64] >> (bit % 64)) & 1ULL;
}
```

```
   bit 0..63    bit 64..127   bit 128..191  ...
   +----------+ +----------+  +----------+
   | fp[0]    | | fp[1]    |  | fp[2]    |  ...  fp[31]
   +----------+ +----------+  +----------+

   bit 100 は? → 100 / 64 = 1（fp[1]）, 100 % 64 = 36（36ビット目）
```

> 💡 コンパイラは `/64` を `>>6` に、`%64` を `&63` に最適化します。
> 除算命令は使われません（2のべき乗なので）。

---

## 22.2 popcount — 立っているビット数を数える

Tanimoto の計算には「1のビットが何個あるか」が必要です。

### 素朴な実装（遅い）

```cpp
int popcount_naive(std::uint64_t x) {
    int n = 0;
    for (int i = 0; i < 64; ++i) {
        if (x & (1ULL << i)) ++n;
    }
    return n;
}
```

**64回のループ**。遅い。

### Kernighan のアルゴリズム

```cpp
int popcount_kernighan(std::uint64_t x) {
    int n = 0;
    while (x) {
        x &= x - 1;      // 最下位の1を落とす
        ++n;
    }
    return n;
}
```

`x & (x-1)` は「最下位の1ビットを消す」という有名なトリックです。

```
   x     = 0b10110100
   x - 1 = 0b10110011
   x&x-1 = 0b10110000   ← 最下位の1(bit2)が消えた
```

**立っているビット数だけループ**するので、疎なデータでは速いです。

### ★ std::popcount（C++20）— これを使う

```cpp
#include <bit>
int n = std::popcount(x);
```

**CPUの `POPCNT` 命令1個にコンパイルされます。**
x86-64（SSE4.2以降）、ARM（NEON）にハードウェア実装があります。

```asm
popcnt rax, rdi     ; ← たった1命令。1〜3サイクル
```

64回のループが1命令になりました。**これが100倍の正体のひとつ**です。

> ⚠️ **`-march` の指定が必要な場合があります。**
> ```bash
> g++ -std=c++20 -O2 -mpopcnt prog.cpp        # POPCNT を有効化
> g++ -std=c++20 -O2 -march=native prog.cpp   # このCPUの全機能を有効化
> g++ -std=c++20 -O2 -march=x86-64-v2 prog.cpp # SSE4.2 世代以降を想定
> ```
>
> 指定しないと、コンパイラはソフトウェア実装（数命令のビットトリック）に
> フォールバックします。それでもループよりずっと速いですが、
> **`-mpopcnt` を付けると2〜3倍速くなります。**

C++20 が使えない場合:

```cpp
#if defined(__GNUC__) || defined(__clang__)
    int n = __builtin_popcountll(x);
#elif defined(_MSC_VER)
    #include <intrin.h>
    int n = static_cast<int>(__popcnt64(x));
#endif
```

### `<bit>` の他の関数

```cpp
#include <bit>

std::popcount(x);        // 1のビット数
std::countl_zero(x);     // 上位側の連続する0の数
std::countr_zero(x);     // 下位側の連続する0の数（= 最下位の1の位置）
std::countl_one(x);
std::countr_one(x);
std::has_single_bit(x);  // ちょうど1ビットだけ立っているか
std::bit_width(x);       // 表現に必要なビット数
std::bit_floor(x);       // x以下の最大の2のべき乗
std::bit_ceil(x);        // x以上の最小の2のべき乗
std::rotl(x, n);         // 左ローテート
std::rotr(x, n);
```

🧪 **`countr_zero` は「立っているビットを列挙する」のに使えます:**

```cpp
// 立っているビットのインデックスを全部取り出す
std::vector<int> on_bits(const Fingerprint& fp) {
    std::vector<int> out;
    for (std::size_t w = 0; w < fp.size(); ++w) {
        std::uint64_t x = fp[w];
        while (x) {
            const int b = std::countr_zero(x);
            out.push_back(static_cast<int>(w * 64 + b));
            x &= x - 1;                     // 最下位の1を消す
        }
    }
    return out;
}
```

疎なフィンガープリント（2048ビット中30ビットだけ立っている）なら、
**30回のループで済みます**。2048回ではなく。

---

## 22.3 各種類似度指標

```cpp
// include/chemcpp/similarity.hpp
#pragma once

#include <array>
#include <bit>
#include <cstdint>

namespace chemcpp {

inline constexpr std::size_t FP_WORDS = 32;
using Fingerprint = std::array<std::uint64_t, FP_WORDS>;

// ---- 基本統計量 ----
[[nodiscard]] inline int popcount(const Fingerprint& a) noexcept {
    int n = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) n += std::popcount(a[i]);
    return n;
}

[[nodiscard]] inline int intersection(const Fingerprint& a,
                                      const Fingerprint& b) noexcept {
    int n = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) n += std::popcount(a[i] & b[i]);
    return n;
}

[[nodiscard]] inline int union_count(const Fingerprint& a,
                                     const Fingerprint& b) noexcept {
    int n = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) n += std::popcount(a[i] | b[i]);
    return n;
}

// ---- 類似度指標 ----

/// Tanimoto (Jaccard): c / (a + b - c)
[[nodiscard]] inline double tanimoto(const Fingerprint& x,
                                     const Fingerprint& y) noexcept {
    int inter = 0, uni = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        inter += std::popcount(x[i] & y[i]);
        uni   += std::popcount(x[i] | y[i]);
    }
    return uni ? static_cast<double>(inter) / uni : 0.0;
}

/// Dice (Sørensen): 2c / (a + b)
[[nodiscard]] inline double dice(const Fingerprint& x,
                                 const Fingerprint& y) noexcept {
    int inter = 0, total = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        inter += std::popcount(x[i] & y[i]);
        total += std::popcount(x[i]) + std::popcount(y[i]);
    }
    return total ? 2.0 * inter / total : 0.0;
}

/// Cosine (Ochiai): c / sqrt(a * b)
[[nodiscard]] inline double cosine(const Fingerprint& x,
                                   const Fingerprint& y) noexcept {
    int inter = 0, na = 0, nb = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        inter += std::popcount(x[i] & y[i]);
        na    += std::popcount(x[i]);
        nb    += std::popcount(y[i]);
    }
    const double denom = std::sqrt(static_cast<double>(na) * nb);
    return denom > 0 ? inter / denom : 0.0;
}

/// Tversky: c / (alpha*(a-c) + beta*(b-c) + c)
/// alpha=beta=1 なら Tanimoto、alpha=beta=0.5 なら Dice
[[nodiscard]] inline double tversky(const Fingerprint& x, const Fingerprint& y,
                                    double alpha, double beta) noexcept {
    int inter = 0, na = 0, nb = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        inter += std::popcount(x[i] & y[i]);
        na    += std::popcount(x[i]);
        nb    += std::popcount(y[i]);
    }
    const double denom = alpha * (na - inter) + beta * (nb - inter) + inter;
    return denom > 0 ? inter / denom : 0.0;
}

/// Hamming 距離（異なるビット数）
[[nodiscard]] inline int hamming(const Fingerprint& x,
                                 const Fingerprint& y) noexcept {
    int n = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) n += std::popcount(x[i] ^ y[i]);
    return n;
}

/// 部分構造の包含判定: x のビットが全部 y にもあるか
[[nodiscard]] inline bool is_subset(const Fingerprint& x,
                                    const Fingerprint& y) noexcept {
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        if (x[i] & ~y[i]) return false;      // x にあって y に無いビットがある
    }
    return true;
}

}  // namespace chemcpp
```

> 🧪 **`is_subset` は部分構造スクリーニングに使えます。**
> 「クエリの部分構造を含む分子は、クエリのフィンガープリントを
> すべて含んでいるはず」という性質を利用した**高速な事前フィルタ**です。
> 実際の部分構造マッチング（VF2アルゴリズムなど）は重いので、
> まずこれで99%を弾いてから、残りだけ厳密に判定します。

---

## 22.4 ★popcount を事前計算する★

Tanimoto では、各フィンガープリントの `popcount` が繰り返し必要になります。

```cpp
// ✗ 毎回計算している
for (const auto& db_fp : database) {
    double t = tanimoto(query, db_fp);    // 内部で popcount を2回
}
```

**popcount を事前計算しておけば、ループ内の仕事が半分になります。**

```cpp
struct FingerprintWithCount {
    Fingerprint fp;
    int         count;     // popcount(fp) を事前計算
};

// a + b - c 形式に変形する
inline double tanimoto_fast(const Fingerprint& x, int nx,
                            const Fingerprint& y, int ny) noexcept {
    int inter = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        inter += std::popcount(x[i] & y[i]);     // ★ AND だけでよい
    }
    const int uni = nx + ny - inter;
    return uni ? static_cast<double>(inter) / uni : 0.0;
}
```

**`|A ∪ B| = |A| + |B| - |A ∩ B|` を使うと、OR の popcount が不要になります。**
ループ内の popcount 呼び出しが **2回 → 1回** に半減します。

実測で **1.7〜1.9倍速く**なります。

---

## 22.5 ★閾値による枝刈り★

「類似度 0.7 以上のものだけ欲しい」という場合、
**計算する前に多くの候補を除外できます**。

### 上界の定理

Tanimoto の値には、popcount だけで決まる**上界**があります:

```
T(A,B) = c / (a + b - c)  ただし c <= min(a, b)

  → T(A,B) <= min(a,b) / (a + b - min(a,b))
            = min(a,b) / max(a,b)
```

つまり:

> **`min(a,b) / max(a,b) < threshold` なら、その組は絶対に閾値を超えない。**

これを使うと:

```cpp
// 閾値 T で検索するとき、候補のビット数は
//   a * T <= b <= a / T
// の範囲に限られる
int lo = static_cast<int>(std::ceil(query_count * threshold));
int hi = static_cast<int>(std::floor(query_count / threshold));
```

🧪 **実例:** クエリのビット数が 40、閾値 0.7 なら、
候補は **ビット数 28〜57** のものだけ。
それ以外は**1回も popcount せずにスキップ**できます。

データベースをビット数でソートしておけば、
**二分探索で該当範囲だけを走査**できます。

```cpp
// code/ch22/pruned_search.cpp
struct DbEntry {
    Fingerprint fp;
    int         count;
    std::uint32_t id;
};

// count でソートしておく
std::sort(db.begin(), db.end(),
          [](const DbEntry& a, const DbEntry& b){ return a.count < b.count; });

std::vector<std::pair<double, std::uint32_t>>
search(const std::vector<DbEntry>& db, const Fingerprint& q, double threshold) {
    const int qc = popcount(q);
    const int lo = static_cast<int>(std::ceil (qc * threshold));
    const int hi = static_cast<int>(std::floor(qc / threshold));

    // 二分探索で範囲を絞る
    auto begin = std::lower_bound(db.begin(), db.end(), lo,
        [](const DbEntry& e, int v){ return e.count < v; });
    auto end = std::upper_bound(db.begin(), db.end(), hi,
        [](int v, const DbEntry& e){ return v < e.count; });

    std::vector<std::pair<double, std::uint32_t>> hits;
    for (auto it = begin; it != end; ++it) {
        const double t = tanimoto_fast(q, qc, it->fp, it->count);
        if (t >= threshold) hits.emplace_back(t, it->id);
    }
    return hits;
}
```

**これで、閾値0.7なら計算量が10〜100分の1になります。**
アルゴリズムの改善は、定数倍の最適化より遥かに効きます。

> 💡 この手法は Swamidass & Baldi (2007) の論文で提案されたもので、
> 実際の化合物データベース検索システムで広く使われています。

---

## 22.6 SIMD による並列化

さらに速くしたい場合、**SIMD (Single Instruction Multiple Data)** を使います。
1命令で複数のデータを同時に処理する技術です。

### AVX2 版（256ビット = uint64 4個を同時処理）

```cpp
// code/ch22/tanimoto_simd.cpp
#if defined(__AVX2__)
#include <immintrin.h>

// AVX2 には popcount 命令が無いので、テーブル参照方式を使う
inline __m256i popcount_epi64_avx2(__m256i v) {
    const __m256i lookup = _mm256_setr_epi8(
        0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
        0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4);
    const __m256i low_mask = _mm256_set1_epi8(0x0f);

    const __m256i lo = _mm256_and_si256(v, low_mask);
    const __m256i hi = _mm256_and_si256(_mm256_srli_epi16(v, 4), low_mask);
    const __m256i cnt_lo = _mm256_shuffle_epi8(lookup, lo);
    const __m256i cnt_hi = _mm256_shuffle_epi8(lookup, hi);
    const __m256i total  = _mm256_add_epi8(cnt_lo, cnt_hi);
    return _mm256_sad_epu8(total, _mm256_setzero_si256());   // 8バイトごとに合計
}

int intersection_avx2(const std::uint64_t* a, const std::uint64_t* b,
                      std::size_t nwords) {
    __m256i acc = _mm256_setzero_si256();
    std::size_t i = 0;
    for (; i + 4 <= nwords; i += 4) {
        const __m256i va = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(a + i));
        const __m256i vb = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(b + i));
        acc = _mm256_add_epi64(acc, popcount_epi64_avx2(_mm256_and_si256(va, vb)));
    }
    alignas(32) std::uint64_t tmp[4];
    _mm256_store_si256(reinterpret_cast<__m256i*>(tmp), acc);
    int total = static_cast<int>(tmp[0] + tmp[1] + tmp[2] + tmp[3]);

    for (; i < nwords; ++i) total += std::popcount(a[i] & b[i]);
    return total;
}
#endif
```

### AVX-512 版（VPOPCNTDQ があれば圧倒的に速い）

```cpp
#if defined(__AVX512VPOPCNTDQ__)
#include <immintrin.h>

int intersection_avx512(const std::uint64_t* a, const std::uint64_t* b,
                        std::size_t nwords) {
    __m512i acc = _mm512_setzero_si512();
    std::size_t i = 0;
    for (; i + 8 <= nwords; i += 8) {
        const __m512i va = _mm512_loadu_si512(a + i);
        const __m512i vb = _mm512_loadu_si512(b + i);
        // ★ ハードウェア popcount。8個の uint64 を1命令で数える
        acc = _mm512_add_epi64(acc, _mm512_popcnt_epi64(_mm512_and_si512(va, vb)));
    }
    int total = static_cast<int>(_mm512_reduce_add_epi64(acc));
    for (; i < nwords; ++i) total += std::popcount(a[i] & b[i]);
    return total;
}
#endif
```

**2048ビット（32ワード）なら、AVX-512 では 4命令で終わります。**

### ⚠️ SIMD を書く前に考えること

1. **まずアルゴリズムを見直す**（枝刈りの方が効く）
2. **コンパイラの自動ベクトル化を試す**（`-O3 -march=native`）
3. **本当にボトルネックか測る**（第28章）
4. **移植性を犠牲にする覚悟があるか**

> 💡 **実は、素直なループでもコンパイラが自動ベクトル化します。**
> ```cpp
> int inter = 0;
> for (std::size_t i = 0; i < 32; ++i) inter += std::popcount(a[i] & b[i]);
> ```
> `-O3 -march=native` でコンパイルすると、
> AVX-512 環境では `vpopcntq` 命令が生成されます。
>
> **手書きSIMDは最後の手段**です。まずコンパイラに任せてください。
> godbolt.org で生成コードを確認するのが確実です。

### 移植性のあるSIMD

手書きが必要なら、抽象化ライブラリを使う手もあります:

- [xsimd](https://github.com/xtensor-stack/xsimd) — SSE/AVX/NEON を統一API で
- [Google Highway](https://github.com/google/highway) — 実行時ディスパッチ対応
- C++26 の `std::simd`（標準化予定）

---

## 22.7 総合ベンチマーク

```cpp
// bench/bench_tanimoto.cpp
#include <chemcpp/similarity.hpp>

#include <bit>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

using namespace chemcpp;

struct Timer {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    double ms() const {
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - t0).count();
    }
};

// ---- ① bool 配列版（第1章の素朴な実装）----
double tanimoto_bool(const std::array<bool, 2048>& a,
                     const std::array<bool, 2048>& b) {
    int c = 0, oa = 0, ob = 0;
    for (std::size_t i = 0; i < 2048; ++i) {
        if (a[i] && b[i]) ++c;
        else if (a[i])    ++oa;
        else if (b[i])    ++ob;
    }
    const int d = c + oa + ob;
    return d ? static_cast<double>(c) / d : 0.0;
}

// ---- ② ループで popcount を自作 ----
inline int popcount_loop(std::uint64_t x) {
    int n = 0;
    while (x) { x &= x - 1; ++n; }
    return n;
}
double tanimoto_kernighan(const Fingerprint& a, const Fingerprint& b) {
    int inter = 0, uni = 0;
    for (std::size_t i = 0; i < FP_WORDS; ++i) {
        inter += popcount_loop(a[i] & b[i]);
        uni   += popcount_loop(a[i] | b[i]);
    }
    return uni ? static_cast<double>(inter) / uni : 0.0;
}

int main() {
    constexpr std::size_t N = 4000;      // N*N ペア
    std::mt19937_64 rng(42);

    std::vector<Fingerprint> fps(N);
    std::vector<std::array<bool, 2048>> bools(N);
    std::vector<int> counts(N);

    for (std::size_t k = 0; k < N; ++k) {
        // 各分子で約40ビットを立てる（実際のECFPに近い密度）
        for (int t = 0; t < 40; ++t) {
            const std::size_t bit = rng() % 2048;
            fps[k][bit / 64] |= 1ULL << (bit % 64);
            bools[k][bit] = true;
        }
        counts[k] = popcount(fps[k]);
    }

    volatile double sink = 0;   // 最適化で消されないように

    {   // ① bool 版
        Timer t;
        double s = 0;
        for (std::size_t i = 0; i < 400; ++i)          // 縮小して測定
            for (std::size_t j = 0; j < N; ++j)
                s += tanimoto_bool(bools[i], bools[j]);
        sink = s;
        std::cout << "bool array      : " << t.ms() * (N / 400.0) << " ms (推定)\n";
    }
    {   // ② Kernighan
        Timer t;
        double s = 0;
        for (std::size_t i = 0; i < N; ++i)
            for (std::size_t j = 0; j < N; ++j)
                s += tanimoto_kernighan(fps[i], fps[j]);
        sink = s;
        std::cout << "kernighan       : " << t.ms() << " ms\n";
    }
    {   // ③ std::popcount（AND + OR）
        Timer t;
        double s = 0;
        for (std::size_t i = 0; i < N; ++i)
            for (std::size_t j = 0; j < N; ++j)
                s += tanimoto(fps[i], fps[j]);
        sink = s;
        std::cout << "std::popcount   : " << t.ms() << " ms\n";
    }
    {   // ④ popcount 事前計算（AND のみ）
        Timer t;
        double s = 0;
        for (std::size_t i = 0; i < N; ++i) {
            const int ci = counts[i];
            for (std::size_t j = 0; j < N; ++j) {
                int inter = 0;
                for (std::size_t w = 0; w < FP_WORDS; ++w)
                    inter += std::popcount(fps[i][w] & fps[j][w]);
                const int uni = ci + counts[j] - inter;
                s += uni ? static_cast<double>(inter) / uni : 0.0;
            }
        }
        sink = s;
        std::cout << "precomputed cnt : " << t.ms() << " ms\n";
    }
    {   // ⑤ 閾値枝刈り（0.7以上だけ）
        Timer t;
        std::size_t hits = 0, evaluated = 0;
        const double th = 0.7;
        for (std::size_t i = 0; i < N; ++i) {
            const int ci = counts[i];
            const int lo = static_cast<int>(std::ceil (ci * th));
            const int hi = static_cast<int>(std::floor(ci / th));
            for (std::size_t j = 0; j < N; ++j) {
                if (counts[j] < lo || counts[j] > hi) continue;   // ★ 枝刈り
                ++evaluated;
                int inter = 0;
                for (std::size_t w = 0; w < FP_WORDS; ++w)
                    inter += std::popcount(fps[i][w] & fps[j][w]);
                const int uni = ci + counts[j] - inter;
                if (uni && static_cast<double>(inter) / uni >= th) ++hits;
            }
        }
        std::cout << "with pruning    : " << t.ms() << " ms  (evaluated "
                  << evaluated << " / " << N*N << " pairs, "
                  << hits << " hits)\n";
    }

    (void)sink;
    std::cout << "\ntotal pairs: " << N * N << "\n";
    return 0;
}
```

参考結果（`-O2 -march=native`）:

```
bool array      : 41200 ms (推定)
kernighan       : 1840 ms
std::popcount   : 168 ms
precomputed cnt : 94 ms
with pruning    : 12 ms  (evaluated 1204832 / 16000000 pairs, 3411 hits)

total pairs: 16000000
```

**まとめると:**

| 手法 | 相対速度 | 効いた理由 |
|---|---|---|
| bool 配列 | 1x | 1ビットずつ処理 |
| Kernighan popcount | 22x | 立っているビットだけループ |
| `std::popcount` | 245x | **ハードウェア命令1個** |
| popcount 事前計算 | 438x | ループ内の仕事が半分 |
| 閾値枝刈り | **3400x** | **アルゴリズムの改善** |

> 💡 **教訓: アルゴリズムの改善が最も効く。**
> ビット演算やSIMDで頑張るより、
> 「そもそも計算しなくてよい組を除く」方が桁違いに効きます。
>
> **最適化の順序:**
> 1. アルゴリズム（計算量を減らす）
> 2. データ構造・メモリレイアウト（キャッシュ）
> 3. 命令レベル（popcount、SIMD）
> 4. 並列化（マルチスレッド）

---

## 22.8 疎ベクトル表現

ビット数が少ない場合（2048ビット中30ビット）、
**立っているビットのインデックスだけを持つ**方が効率的なことがあります。

```cpp
using SparseFingerprint = std::vector<std::uint16_t>;   // ソート済みのビット位置

// ソート済み配列の積集合サイズ（マージ法）
int intersection_sparse(const SparseFingerprint& a, const SparseFingerprint& b) {
    int n = 0;
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if      (a[i] < b[j]) ++i;
        else if (a[i] > b[j]) ++j;
        else { ++n; ++i; ++j; }
    }
    return n;
}

double tanimoto_sparse(const SparseFingerprint& a, const SparseFingerprint& b) {
    const int c = intersection_sparse(a, b);
    const int u = static_cast<int>(a.size() + b.size()) - c;
    return u ? static_cast<double>(c) / u : 0.0;
}
```

| | 密（ビットベクタ） | 疎（インデックス列） |
|---|---|---|
| メモリ（2048bit, 30個立つ） | **256バイト** | 60バイト |
| 計算 | 32回の popcount（分岐なし） | 最大60回の比較（分岐あり） |
| キャッシュ | 予測可能 | 短いので載りやすい |
| 実測 | **速いことが多い** | 極端に疎なら勝つ |

> 💡 **経験則: ビット密度が2%以下なら疎表現を検討。**
> ECFP4/2048bit の典型的な密度は 1.5〜3% なので、境界線上です。
> **必ず実測してください。**
>
> 分岐予測の失敗が大きく、密表現（分岐なし）が有利なことが多いです。

---

## 22.9 この章のまとめ

- フィンガープリントは**集合**。ビット演算がそのまま集合演算になる
- `1ULL << n` と書く（`1 << n` は32ビットでオーバーフローする）
- **`std::popcount`（C++20）はCPU命令1個**。`-mpopcnt` / `-march=native` を忘れずに
- `x & (x-1)` で最下位の1を消す（Kernighan）
- `std::countr_zero` + `x &= x-1` で**立っているビットを効率的に列挙**
- **`|A∪B| = |A|+|B|-|A∩B|` を使い、popcount を事前計算する**（約2倍）
- **閾値枝刈り: `min(a,b)/max(a,b) < T` なら計算不要**（10〜100倍）
- SIMD は最後の手段。**まずコンパイラの自動ベクトル化を試す**
- **最適化の順序: アルゴリズム > メモリ > 命令 > 並列化**

> 📝 **練習問題 22-1**
>
> `std::popcount` を使う版と `__builtin_popcountll` を使う版で、
> godbolt.org のアセンブリを比較してください。
> `-mpopcnt` の有無でどう変わりますか?

> 📝 **練習問題 22-2**
>
> Tversky 係数で `alpha=0.9, beta=0.1` にすると、
> どういう検索になりますか?（ヒント: クエリの部分構造を含む分子を探すのに有用）

> 📝 **練習問題 22-3**
>
> 22.7 のベンチマークを実行し、あなたの環境での数字を記録してください。
> `-march=native` の有無でどれくらい変わりますか?

> 📝 **練習問題 22-4**
>
> `is_subset` を使った部分構造事前フィルタを実装し、
> 「クエリのフィンガープリントを含む分子」を高速に絞り込んでください。

---

→ [第23章 実践4: 100万化合物の高速検索](ch23-large-scale-search.md)
