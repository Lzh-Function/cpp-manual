// 第21章: 分子グラフのアルゴリズム
// 再帰ではなく明示スタック/キューを使う（巨大な分子でも安全）
#pragma once

#include <chemcpp/molecule.hpp>

#include <algorithm>
#include <limits>
#include <vector>

namespace chemcpp {

/// start から全原子への結合距離。到達不能なら -1。
[[nodiscard]] inline std::vector<int>
shortest_paths_from(const Molecule& mol, std::uint32_t start) {
    std::vector<int> dist(mol.num_atoms(), -1);
    if (start >= mol.num_atoms()) return dist;

    std::vector<std::uint32_t> frontier{start}, next;
    dist[start] = 0;
    int d = 0;

    while (!frontier.empty()) {
        ++d;
        next.clear();
        for (std::uint32_t u : frontier) {
            for (const auto& nb : mol.neighbors(u)) {
                if (dist[nb.atom] < 0) {
                    dist[nb.atom] = d;
                    next.push_back(nb.atom);
                }
            }
        }
        frontier.swap(next);
    }
    return dist;
}

/// 全原子ペアの距離行列（row-major、n*n）
[[nodiscard]] inline std::vector<int> distance_matrix(const Molecule& mol) {
    const std::size_t n = mol.num_atoms();
    std::vector<int> d(n * n, -1);
    for (std::uint32_t i = 0; i < n; ++i) {
        const auto row = shortest_paths_from(mol, i);
        for (std::size_t j = 0; j < n; ++j) d[i * n + j] = row[j];
    }
    return d;
}

/// グラフの直径（最も離れた2原子の距離）
[[nodiscard]] inline int graph_diameter(const Molecule& mol) {
    int best = 0;
    for (std::uint32_t i = 0; i < mol.num_atoms(); ++i) {
        for (int x : shortest_paths_from(mol, i)) best = std::max(best, x);
    }
    return best;
}

/// 各原子が属する連結成分の番号（0始まり）
[[nodiscard]] inline std::vector<int> connected_components(const Molecule& mol) {
    const std::size_t n = mol.num_atoms();
    std::vector<int> comp(n, -1);
    int c = 0;
    std::vector<std::uint32_t> stack;

    for (std::uint32_t s = 0; s < n; ++s) {
        if (comp[s] >= 0) continue;
        stack.clear();
        stack.push_back(s);
        comp[s] = c;
        while (!stack.empty()) {
            const std::uint32_t u = stack.back();
            stack.pop_back();
            for (const auto& nb : mol.neighbors(u)) {
                if (comp[nb.atom] < 0) { comp[nb.atom] = c; stack.push_back(nb.atom); }
            }
        }
        ++c;
    }
    return comp;
}

[[nodiscard]] inline int num_components(const Molecule& mol) {
    if (mol.num_atoms() == 0) return 0;
    const auto comp = connected_components(mol);
    return *std::max_element(comp.begin(), comp.end()) + 1;
}

/// 環の個数（Frerejacque 数 = 結合数 - 原子数 + 連結成分数）
[[nodiscard]] inline int ring_count(const Molecule& mol) {
    if (mol.num_atoms() == 0) return 0;
    return static_cast<int>(mol.num_bonds())
         - static_cast<int>(mol.num_atoms())
         + num_components(mol);
}

/// 各原子が属する最小の環のサイズ（環にないなら 0）
[[nodiscard]] inline std::vector<int> smallest_ring_size(const Molecule& mol) {
    const std::size_t n = mol.num_atoms();
    std::vector<int> result(n, 0);

    for (std::uint32_t start = 0; start < n; ++start) {
        if (!mol.atom(start).in_ring) continue;

        int best = std::numeric_limits<int>::max();

        for (const auto& first_nb : mol.neighbors(start)) {
            // start-first_nb の結合を使わずに start へ戻る最短経路を探す
            std::vector<int> dist(n, -1);
            std::vector<std::uint32_t> frontier{first_nb.atom}, next;
            dist[first_nb.atom] = 0;

            int d = 0;
            bool found = false;
            while (!frontier.empty() && !found) {
                ++d;
                next.clear();
                for (std::uint32_t u : frontier) {
                    for (const auto& nb : mol.neighbors(u)) {
                        if (nb.bond == first_nb.bond) continue;
                        if (nb.atom == start) { best = std::min(best, d + 1);
                                                found = true; break; }
                        if (dist[nb.atom] < 0) {
                            dist[nb.atom] = d;
                            next.push_back(nb.atom);
                        }
                    }
                    if (found) break;
                }
                frontier.swap(next);
            }
        }
        if (best != std::numeric_limits<int>::max()) result[start] = best;
    }
    return result;
}

}  // namespace chemcpp
