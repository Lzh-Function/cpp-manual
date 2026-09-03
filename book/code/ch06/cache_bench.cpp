// 第6章: メモリレイアウトが速度に与える影響を実測する
//
// g++ -std=c++20 -O2 cache_bench.cpp -o cache_bench
// ★ -O0 で測ってはいけません（意味のない数字が出ます）
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

struct Timer {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    double ms() const {
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - t0).count();
    }
};

int main() {
    constexpr std::size_t N = 5'000'000;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "elements: " << N << "\n\n";

    // ---------- (1) 連続した値の配列 ----------
    std::vector<double> flat(N);
    std::iota(flat.begin(), flat.end(), 1.0);

    // ---------- (2) ポインタの配列（Python のリスト相当） ----------
    std::vector<std::unique_ptr<double>> scattered;
    scattered.reserve(N);
    for (std::size_t i = 0; i < N; ++i) {
        scattered.push_back(std::make_unique<double>(static_cast<double>(i + 1)));
    }
    // シャッフルして、さらにキャッシュに不利にする
    std::mt19937_64 rng(42);
    std::shuffle(scattered.begin(), scattered.end(), rng);

    // ---------- (3) 二次元 vector（行ごとにヒープが飛ぶ） ----------
    constexpr std::size_t ROWS = 2000, COLS = 2000;
    std::vector<std::vector<double>> nested(ROWS, std::vector<double>(COLS, 1.0));
    std::vector<double>              flat2d(ROWS * COLS, 1.0);

    volatile double sink = 0.0;   // 最適化で消されないように

    {
        Timer t;
        double sum = 0.0;
        for (double x : flat) sum += x;
        sink = sum;
        std::cout << "contiguous vector<double>   : "
                  << std::setw(8) << t.ms() << " ms\n";
    }
    {
        Timer t;
        double sum = 0.0;
        for (const auto& p : scattered) sum += *p;
        sink = sum;
        std::cout << "scattered unique_ptr array  : "
                  << std::setw(8) << t.ms() << " ms\n";
    }
    std::cout << "\n2-D access (" << ROWS << " x " << COLS << ")\n";
    {
        Timer t;
        double sum = 0.0;
        for (std::size_t i = 0; i < ROWS; ++i)
            for (std::size_t j = 0; j < COLS; ++j) sum += nested[i][j];
        sink = sum;
        std::cout << "  vector<vector<double>>    : "
                  << std::setw(8) << t.ms() << " ms\n";
    }
    {
        Timer t;
        double sum = 0.0;
        for (std::size_t i = 0; i < ROWS; ++i)
            for (std::size_t j = 0; j < COLS; ++j) sum += flat2d[i * COLS + j];
        sink = sum;
        std::cout << "  flat vector (row-major)   : "
                  << std::setw(8) << t.ms() << " ms\n";
    }
    {
        Timer t;
        double sum = 0.0;
        // ★ 列優先アクセス（同じデータ、同じ計算量。順序だけが違う）
        for (std::size_t j = 0; j < COLS; ++j)
            for (std::size_t i = 0; i < ROWS; ++i) sum += flat2d[i * COLS + j];
        sink = sum;
        std::cout << "  flat vector (col-major)   : "
                  << std::setw(8) << t.ms() << " ms\n";
    }

    (void)sink;
    std::cout << "\nsizeof(std::vector<int>) = " << sizeof(std::vector<int>) << "\n";
    std::cout << "sizeof(std::string)      = " << sizeof(std::string) << "\n";
    std::cout << "sizeof(std::unique_ptr<double>) = "
              << sizeof(std::unique_ptr<double>) << "\n";
    return 0;
}
