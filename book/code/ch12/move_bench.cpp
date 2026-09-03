// 第12章: コピーとムーブ、そして noexcept の効果を実測する
// g++ -std=c++20 -O2 move_bench.cpp -o move_bench
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <utility>
#include <vector>

struct Timer {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    double ms() const {
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - t0).count();
    }
};

// ---------------------------------------------------------------
// 大きなデータを持つ型
// ---------------------------------------------------------------
struct Fingerprint {
    std::vector<std::uint64_t> bits;
    explicit Fingerprint(std::size_t n = 32)
        : bits(n, 0xDEADBEEFCAFEBABEULL) {}
};

// ---------------------------------------------------------------
// ムーブが noexcept かどうかで vector の再確保が変わることを示す
// ---------------------------------------------------------------
struct WithNoexcept {
    std::vector<int> v = std::vector<int>(100);
    WithNoexcept() = default;
    WithNoexcept(const WithNoexcept&) = default;
    WithNoexcept& operator=(const WithNoexcept&) = default;
    WithNoexcept(WithNoexcept&& o) noexcept : v(std::move(o.v)) {}
    WithNoexcept& operator=(WithNoexcept&& o) noexcept {
        v = std::move(o.v); return *this;
    }
};

struct WithoutNoexcept {
    std::vector<int> v = std::vector<int>(100);
    WithoutNoexcept() = default;
    WithoutNoexcept(const WithoutNoexcept&) = default;
    WithoutNoexcept& operator=(const WithoutNoexcept&) = default;
    WithoutNoexcept(WithoutNoexcept&& o) : v(std::move(o.v)) {}   // ★ noexcept なし
    WithoutNoexcept& operator=(WithoutNoexcept&& o) {
        v = std::move(o.v); return *this;
    }
};

// ---------------------------------------------------------------
// コピー/ムーブの回数を数える型
// ---------------------------------------------------------------
struct Counted {
    static inline int copies = 0;
    static inline int moves  = 0;
    std::vector<int> data;

    explicit Counted(std::size_t n = 1000) : data(n, 1) {}
    Counted(const Counted& o) : data(o.data)            { ++copies; }
    Counted(Counted&& o) noexcept : data(std::move(o.data)) { ++moves; }
    Counted& operator=(const Counted& o) { data = o.data;            ++copies; return *this; }
    Counted& operator=(Counted&& o) noexcept { data = std::move(o.data); ++moves;  return *this; }

    static void reset() { copies = moves = 0; }
    static void report(const char* label) {
        std::cout << "  " << std::left << std::setw(28) << label << std::right
                  << " copies=" << copies << " moves=" << moves << "\n";
    }
};

Counted make_counted() {
    Counted c(1000);
    return c;                    // ★ NRVO により、コピーもムーブも起きない
}

int main() {
    std::cout << std::fixed << std::setprecision(2);
    constexpr std::size_t N = 200'000;

    // ---------- コピー vs ムーブ ----------
    std::cout << "copy vs move (" << N << " Fingerprints)\n";
    {
        std::vector<Fingerprint> dst;
        dst.reserve(N);
        const Fingerprint fp;
        Timer t;
        for (std::size_t i = 0; i < N; ++i) dst.push_back(fp);         // コピー
        std::cout << "  copy : " << std::setw(8) << t.ms() << " ms\n";
    }
    {
        std::vector<Fingerprint> src(N);
        std::vector<Fingerprint> dst;
        dst.reserve(N);
        Timer t;
        for (std::size_t i = 0; i < N; ++i)
            dst.push_back(std::move(src[i]));                          // ムーブ
        std::cout << "  move : " << std::setw(8) << t.ms() << " ms\n";
    }

    // ---------- noexcept の有無 ----------
    std::cout << "\nvector reallocation (100k elements, no reserve)\n";
    {
        Timer t;
        std::vector<WithNoexcept> v;
        for (std::size_t i = 0; i < 100'000; ++i) v.emplace_back();
        std::cout << "  with noexcept    : " << std::setw(8) << t.ms() << " ms\n";
    }
    {
        Timer t;
        std::vector<WithoutNoexcept> v;
        for (std::size_t i = 0; i < 100'000; ++i) v.emplace_back();
        std::cout << "  without noexcept : " << std::setw(8) << t.ms() << " ms\n";
    }

    // ---------- コピー/ムーブの回数 ----------
    std::cout << "\noperation counts\n";
    {
        Counted::reset();
        std::vector<Counted> v;
        v.reserve(3);
        Counted a(1000);
        v.push_back(a);                     // コピー
        Counted::report("push_back(lvalue)");
    }
    {
        Counted::reset();
        std::vector<Counted> v;
        v.reserve(3);
        Counted a(1000);
        v.push_back(std::move(a));          // ムーブ
        Counted::report("push_back(std::move)");
    }
    {
        Counted::reset();
        std::vector<Counted> v;
        v.reserve(3);
        v.push_back(Counted(1000));         // 一時オブジェクト → ムーブ
        Counted::report("push_back(temporary)");
    }
    {
        Counted::reset();
        std::vector<Counted> v;
        v.reserve(3);
        v.emplace_back(1000);               // 直接構築 → 0回
        Counted::report("emplace_back");
    }
    {
        Counted::reset();
        const Counted c = make_counted();   // NRVO → 0回
        (void)c;
        Counted::report("return value (NRVO)");
    }
    return 0;
}
