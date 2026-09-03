// 第23章: 大規模類似度検索ツール
//
// 使い方:
//   ./screen <library.smi> <query_smiles> [threshold=0.7] [topk=0]
//
// library.smi は "SMILES<TAB>ID" 形式（1行1分子）。
#include <chemcpp/smiles.hpp>
#include <chemcpp/fingerprint.hpp>
#include <chemcpp/database.hpp>
#include <chemcpp/search.hpp>
#include <chemcpp/io.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

using namespace chemcpp;

namespace {

struct Timer {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    double ms() const {
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - t0).count();
    }
    void reset() { t0 = std::chrono::steady_clock::now(); }
};

}  // namespace

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);

    if (argc < 3) {
        std::cerr << "usage: screen <library.smi> <query_smiles> "
                     "[threshold=0.7] [topk=0]\n";
        return 1;
    }
    const std::string lib_path  = argv[1];
    const std::string query_smi = argv[2];
    const double      threshold = (argc > 3) ? std::stod(argv[3]) : 0.7;
    const std::size_t topk      = (argc > 4) ? std::stoul(argv[4]) : 0;

    // ---------------- ライブラリの読み込み ----------------
    Timer t;
    FingerprintDB db;
    db.reserve(100000);

    std::size_t n_ok = 0, n_fail = 0;
    MorganWorkspace ws;
    const MorganOptions opt{.radius = 2, .n_bits = 2048};

    try {
        read_smi(lib_path, [&](std::string_view smi, std::string_view id) {
            const auto mol = parse_smiles(smi);
            if (!mol) { ++n_fail; return; }
            db.add(morgan_fingerprint(*mol, opt, ws),
                   id.empty() ? ("MOL" + std::to_string(n_ok)) : std::string(id));
            ++n_ok;
        });
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "loaded " << n_ok << " molecules (" << n_fail << " failed) in "
              << t.ms() << " ms\n";
    std::cout << "memory: ~"
              << (db.size() * (FP_WORDS * 8 + 4)) / (1024 * 1024) << " MB\n";

    t.reset();
    db.sort_by_count();        // ★ 枝刈りのためにビット数でソート
    std::cout << "sorted by popcount in " << t.ms() << " ms\n";

    // ---------------- クエリ ----------------
    const auto qmol = parse_smiles(query_smi);
    if (!qmol) { std::cerr << "invalid query SMILES\n"; return 1; }
    const auto query = morgan_fingerprint(*qmol, opt, ws);

    std::cout << "\nquery: " << query_smi
              << " (" << popcount(query) << " bits set)\n";

    // ---------------- 検索 ----------------
    t.reset();
    const auto hits = (topk > 0) ? search_topk(db, query, topk)
                                 : search_threshold(db, query, threshold);
    const double search_ms = t.ms();

    std::cout << hits.size() << " hits in "
              << std::setprecision(3) << search_ms << " ms\n\n";

    const std::size_t show = std::min<std::size_t>(hits.size(), 20);
    for (std::size_t i = 0; i < show; ++i) {
        std::cout << std::setw(4) << (i + 1) << ". "
                  << std::left << std::setw(22) << db.id(hits[i].index)
                  << std::right << std::setprecision(4) << hits[i].score
                  << "  (" << db.count(hits[i].index) << " bits)\n";
    }
    if (hits.size() > show) {
        std::cout << "  ... and " << (hits.size() - show) << " more\n";
    }
    return 0;
}
