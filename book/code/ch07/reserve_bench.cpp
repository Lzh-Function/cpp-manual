// 第7章: reserve の効果と capacity の成長を観察する
// g++ -std=c++20 -O2 reserve_bench.cpp -o reserve_bench
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

struct Timer {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    double ms() const {
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - t0).count();
    }
};

int main() {
    constexpr std::size_t N = 10'000'000;
    std::cout << std::fixed << std::setprecision(2);

    // ---------- int の場合 ----------
    {
        Timer t;
        std::vector<int> v;
        for (std::size_t i = 0; i < N; ++i) v.push_back(static_cast<int>(i));
        std::cout << "int, without reserve   : " << std::setw(8) << t.ms() << " ms\n";
    }
    {
        Timer t;
        std::vector<int> v;
        v.reserve(N);
        for (std::size_t i = 0; i < N; ++i) v.push_back(static_cast<int>(i));
        std::cout << "int, with reserve      : " << std::setw(8) << t.ms() << " ms\n";
    }

    // ---------- std::string の場合（差がもっと大きい） ----------
    constexpr std::size_t M = 1'000'000;
    const std::string smiles = "CC(=O)Oc1ccccc1C(=O)O";   // SSO を超える長さ
    {
        Timer t;
        std::vector<std::string> v;
        for (std::size_t i = 0; i < M; ++i) v.push_back(smiles);
        std::cout << "string, without reserve: " << std::setw(8) << t.ms() << " ms\n";
    }
    {
        Timer t;
        std::vector<std::string> v;
        v.reserve(M);
        for (std::size_t i = 0; i < M; ++i) v.push_back(smiles);
        std::cout << "string, with reserve   : " << std::setw(8) << t.ms() << " ms\n";
    }

    // ---------- capacity の成長を観察 ----------
    std::cout << "\ncapacity growth:\n";
    std::vector<int> v;
    std::size_t last = 0;
    for (int i = 0; i < 200; ++i) {
        v.push_back(i);
        if (v.capacity() != last) {
            std::cout << "  size=" << std::setw(4) << v.size()
                      << "  capacity=" << std::setw(4) << v.capacity() << "\n";
            last = v.capacity();
        }
    }

    // ---------- reserve と resize の違い ----------
    std::cout << "\nreserve vs resize:\n";
    std::vector<int> a, b;
    a.reserve(10);
    b.resize(10);
    std::cout << "  after reserve(10): size=" << a.size()
              << " capacity=" << a.capacity() << "\n";
    std::cout << "  after resize(10) : size=" << b.size()
              << " capacity=" << b.capacity() << "\n";
    // a[0] = 1;   // ★ 未定義動作！ size は 0 なので要素は存在しない
    b[0] = 1;      // ✓ OK

    // ---------- SSO の確認 ----------
    std::cout << "\nsmall string optimization:\n";
    std::cout << "  sizeof(std::string) = " << sizeof(std::string) << "\n";
    for (const char* s : {"C", "CCO", "CC(=O)Oc1ccccc1", "CC(=O)Oc1ccccc1C(=O)O"}) {
        const std::string str(s);
        std::cout << "  len=" << std::setw(2) << str.size()
                  << " capacity=" << std::setw(3) << str.capacity()
                  << "  \"" << s << "\"\n";
    }
    return 0;
}
