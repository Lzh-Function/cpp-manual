# 第25章 並列化

C++にはGILがありません。16コアあれば16倍（に近い）性能が出せます。
ただし、並列化には固有の落とし穴があります。

## 25.1 まず、並列化すべきかを考える

```
   並列化を検討する前に:
   ① プロファイルを取ったか?              → 第28章
   ② アルゴリズムを改善したか?            → 第22章の枝刈りは3400倍だった
   ③ メモリレイアウトを最適化したか?      → 第23章
   ④ SIMD/命令レベルの最適化をしたか?

   → 全部やった上で、まだ足りないなら並列化
```

> ⚠️ **並列化は最後の手段です。**
> - 8コアで最大8倍。アルゴリズム改善は100倍もありうる
> - デバッグが劇的に難しくなる（再現しないバグ）
> - コードが複雑になる
>
> ただし、**「並列化しやすい構造に書いておく」**のは常に良いことです。

### 並列化しやすい問題 = 埋め込み並列 (embarrassingly parallel)

```
   ✓ 100万分子の記述子計算    → 各分子が独立
   ✓ 類似度検索              → 各比較が独立
   ✓ コンフォマー生成         → 各分子が独立
   ✓ グリッド上の数値計算     → 各点が独立

   ✗ 逐次依存のあるMCMC       → 前のステップに依存
   ✗ グラフの深さ優先探索      → 順序が意味を持つ
```

**ケモインフォマティクスの多くは前者**です。並列化と相性がいい分野です。

---

## 25.2 std::thread の基本

```cpp
#include <thread>
#include <iostream>

void worker(int id) {
    std::cout << "thread " << id << " working\n";
}

int main() {
    std::thread t1(worker, 1);
    std::thread t2(worker, 2);

    t1.join();          // 終了を待つ
    t2.join();
    return 0;
}
```

> ⚠️ **`join()` を忘れると `std::terminate` します。**
> `std::thread` のデストラクタは、joinable な状態だとプログラムを終了させます。
> これはRAIIの原則に反するように見えますが、
> 「暗黙にデタッチするより即座にエラーにする方が安全」という設計判断です。

### ★ std::jthread（C++20）— 自動 join

```cpp
#include <thread>

{
    std::jthread t(worker, 1);
}   // ★ スコープを抜けると自動的に join される（RAII）
```

**C++20 が使えるなら常に `jthread` を使ってください。**
さらに、協調的なキャンセル機能もあります:

```cpp
std::jthread t([](std::stop_token stop) {
    while (!stop.stop_requested()) {
        do_work();
    }
});
// t.request_stop();  で停止を依頼できる
// デストラクタが自動的に request_stop() + join() する
```

### コア数を知る

```cpp
unsigned n = std::thread::hardware_concurrency();
if (n == 0) n = 4;      // 取得できないことがある
```

> 💡 論理コア数（ハイパースレッディング込み）が返ります。
> メモリ帯域が支配的な処理では、**物理コア数の方が良いこともあります**。
> 実測して決めてください。

---

## 25.3 データ競合と mutex

### ⚠️ データ競合 (data race)

```cpp
int counter = 0;

void bad_worker() {
    for (int i = 0; i < 100000; ++i) {
        ++counter;          // ★ データ競合！
    }
}

std::jthread t1(bad_worker), t2(bad_worker);
// 期待: 200000、実際: 137482 など毎回違う値
```

`++counter` は**アトミックではありません**:

```
   1. counter をメモリから読む
   2. +1 する
   3. メモリに書き戻す
```

2つのスレッドがこれを同時にやると、片方の更新が失われます。

> ⚠️ **データ競合は未定義動作です。** 「値が変になる」だけでなく、
> コンパイラが「競合はない」と仮定して最適化するため、
> **予測不能な挙動**になります。

### mutex による排他制御

```cpp
#include <mutex>

int        counter = 0;
std::mutex mtx;

void good_worker() {
    for (int i = 0; i < 100000; ++i) {
        std::lock_guard<std::mutex> lock(mtx);    // ★ RAII でロック
        ++counter;
    }   // スコープを抜けると自動的にアンロック
}
```

**`std::lock_guard` は RAII**です（第11章）。
例外が飛んでも確実にアンロックされます。

```cpp
// 手動でロックしてはいけない
mtx.lock();
do_something();      // ★ ここで例外が飛ぶとデッドロック
mtx.unlock();
```

### ロックの種類

```cpp
std::lock_guard<std::mutex>  lg(mtx);       // 最もシンプル。推奨
std::unique_lock<std::mutex> ul(mtx);       // 途中でunlock/relockできる
std::scoped_lock             sl(m1, m2);    // ★ C++17: 複数を安全にロック（デッドロック回避）

std::shared_mutex smtx;
std::shared_lock  sh(smtx);                 // 読み取りロック（複数同時可）
std::unique_lock  ex(smtx);                 // 書き込みロック（排他）
```

> 💡 **読み取りが多いデータには `std::shared_mutex`**（reader-writer lock）。
> 複数スレッドが同時に読めます。

### ⚠️ ロックは遅い

```cpp
// ✗ 悪い: ループ内でロック
for (int i = 0; i < 1'000'000; ++i) {
    std::lock_guard lock(mtx);
    results.push_back(compute(i));       // ★ 100万回のロック競合
}

// ✓ 良い: ローカルに集めて最後に1回
std::vector<double> local;
for (int i = 0; i < 1'000'000; ++i) {
    local.push_back(compute(i));
}
{
    std::lock_guard lock(mtx);
    results.insert(results.end(), local.begin(), local.end());   // ★ 1回だけ
}
```

**これが並列化の最重要原則です。** ロック競合があると、
スレッドを増やしても速くなりません（むしろ遅くなります）。

---

## 25.4 std::atomic

単純な変数なら、mutex より軽い `std::atomic` が使えます。

```cpp
#include <atomic>

std::atomic<int> counter{0};

void worker() {
    for (int i = 0; i < 100000; ++i) {
        ++counter;              // ★ アトミック。競合しない
    }
}
```

```cpp
std::atomic<int>    n{0};
std::atomic<double> sum{0.0};       // C++20 で fetch_add 対応
std::atomic<bool>   done{false};

n.fetch_add(5);              // n += 5
n.load();                    // 読む
n.store(10);                 // 書く
n.exchange(20);              // 書いて古い値を返す

int expected = 5;
n.compare_exchange_strong(expected, 10);   // CAS: n が5なら10にする
```

### ⚠️ atomic も無料ではない

アトミック操作は**キャッシュラインをコア間で奪い合います**。

```cpp
// ✗ 悪い: 全スレッドが同じ atomic を叩く
std::atomic<int> total{0};
// 各スレッドで total += 1 を100万回 → 激遅（ロックより遅いことも）

// ✓ 良い: スレッドごとに集計して最後に統合
thread_local int local = 0;      // または std::vector<int> per_thread
// ... 最後に total += local
```

**false sharing** にも注意:

```cpp
struct Counters {
    std::atomic<int> a;   // 同じキャッシュライン(64バイト)に載る
    std::atomic<int> b;   // → 別々の変数なのに競合する！
};

// 対策: パディングで分離
struct alignas(64) PaddedCounter {
    std::atomic<int> value;
    char padding[64 - sizeof(std::atomic<int>)];
};
std::vector<PaddedCounter> counters(n_threads);
```

C++17 には定数もあります:

```cpp
#include <new>
constexpr std::size_t line = std::hardware_destructive_interference_size;  // 通常 64
```

---

## 25.5 ★実践: 並列化のパターン★

### パターン1: 手動チャンク分割（最も基本的）

```cpp
// code/ch25/parallel_map.hpp
#pragma once
#include <thread>
#include <vector>

/// [0, n) を n_threads 個に分割し、各スレッドで f(begin, end, thread_id) を実行
template <typename F>
void parallel_for_chunks(std::size_t n, F&& f, unsigned n_threads = 0) {
    if (n_threads == 0) n_threads = std::thread::hardware_concurrency();
    if (n_threads == 0) n_threads = 4;
    if (n < n_threads * 64) {                // 小さければ並列化しない
        f(std::size_t{0}, n, 0u);
        return;
    }

    std::vector<std::jthread> threads;
    threads.reserve(n_threads);
    const std::size_t chunk = (n + n_threads - 1) / n_threads;

    for (unsigned t = 0; t < n_threads; ++t) {
        const std::size_t begin = t * chunk;
        const std::size_t end   = std::min(begin + chunk, n);
        if (begin >= end) break;
        threads.emplace_back([&f, begin, end, t] { f(begin, end, t); });
    }
    // jthread のデストラクタが join する
}
```

使い方:

```cpp
std::vector<Descriptors> results(mols.size());

parallel_for_chunks(mols.size(), [&](std::size_t b, std::size_t e, unsigned) {
    for (std::size_t i = b; i < e; ++i) {
        results[i] = compute_descriptors(mols[i]);   // ★ 各要素が独立 → ロック不要
    }
});
```

> 💡 **`results` を事前に `resize` しておき、各スレッドが別々の要素に書く**のがコツです。
> `push_back` だとロックが必要になります。
> 異なる要素への書き込みは**データ競合になりません**（同じ vector でも）。

### パターン2: 動的負荷分散（処理時間がバラバラなとき）

```cpp
#include <atomic>

template <typename F>
void parallel_for_dynamic(std::size_t n, F&& f, unsigned n_threads = 0,
                          std::size_t grain = 64) {
    if (n_threads == 0) n_threads = std::thread::hardware_concurrency();
    std::atomic<std::size_t> next{0};

    std::vector<std::jthread> threads;
    for (unsigned t = 0; t < n_threads; ++t) {
        threads.emplace_back([&, t] {
            while (true) {
                // ★ 次のブロックをアトミックに取得
                const std::size_t begin = next.fetch_add(grain,
                                              std::memory_order_relaxed);
                if (begin >= n) break;
                const std::size_t end = std::min(begin + grain, n);
                for (std::size_t i = begin; i < end; ++i) f(i, t);
            }
        });
    }
}
```

🧪 **分子ごとに処理時間が違う場合（大きな分子 vs 小さな分子）に有効**です。
静的分割だと、大きな分子ばかり当たったスレッドが遅れます。

`grain`（一度に取るブロックサイズ）が重要:
- 小さすぎる → atomic の競合が増える
- 大きすぎる → 負荷が偏る
- **経験則: 1ブロックが 10〜100 マイクロ秒になるサイズ**

### パターン3: スレッドローカル集計 + マージ

```cpp
// 100万分子の統計を並列に計算する
struct Stats {
    double sum_mw = 0.0;
    double sum_logp = 0.0;
    int    count = 0;
    int    ro5_pass = 0;

    void merge(const Stats& o) {
        sum_mw   += o.sum_mw;
        sum_logp += o.sum_logp;
        count    += o.count;
        ro5_pass += o.ro5_pass;
    }
};

Stats compute_stats_parallel(const std::vector<Molecule>& mols) {
    const unsigned nt = std::max(1u, std::thread::hardware_concurrency());
    std::vector<Stats> partial(nt);      // ★ スレッドごとの結果

    parallel_for_chunks(mols.size(), [&](std::size_t b, std::size_t e, unsigned t) {
        Stats local;                     // ★ さらにローカル変数（false sharing 回避）
        for (std::size_t i = b; i < e; ++i) {
            const auto d = compute_descriptors(mols[i]);
            local.sum_mw   += d.mw;
            local.sum_logp += d.logp;
            ++local.count;
            if (passes_ro5(d)) ++local.ro5_pass;
        }
        partial[t] = local;              // ★ 最後に1回だけ書く
    }, nt);

    Stats total;
    for (const auto& s : partial) total.merge(s);
    return total;
}
```

**これが最も推奨されるパターンです。** ロックゼロ、atomic ゼロ。

---

## 25.6 std::async と std::future

もう少し高レベルなAPI:

```cpp
#include <future>

std::future<double> f = std::async(std::launch::async, []{
    return heavy_computation();
});

// 他の作業をする...

double result = f.get();       // 結果を待つ
```

```cpp
// 複数の非同期タスク
std::vector<std::future<Descriptors>> futures;
for (const auto& mol : mols) {
    futures.push_back(std::async(std::launch::async,
                                 [&mol]{ return compute_descriptors(mol); }));
}
std::vector<Descriptors> results;
for (auto& f : futures) results.push_back(f.get());
```

> ⚠️ **`std::async` はタスクごとにスレッドを作る可能性があります。**
> 100万個のタスクを投げると、100万スレッドを作ろうとして死にます。
>
> **粒度の大きいタスク（数十〜数百個）にだけ使ってください。**
> 細かいタスクにはスレッドプールが必要です。

> ⚠️ **`std::launch::async` を明示してください。**
> 省略すると `std::launch::async | std::launch::deferred` になり、
> 実装によっては**遅延実行（= 並列化されない）**になります。

---

## 25.7 スレッドプール（自作）

実務では、スレッドを使い回すプールが必要になります。

```cpp
// include/chemcpp/thread_pool.hpp
#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace chemcpp {

class ThreadPool {
public:
    explicit ThreadPool(unsigned n_threads = 0) {
        if (n_threads == 0) n_threads = std::thread::hardware_concurrency();
        if (n_threads == 0) n_threads = 4;

        workers_.reserve(n_threads);
        for (unsigned i = 0; i < n_threads; ++i) {
            workers_.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(mutex_);
                        cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();                      // ★ ロック外で実行
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /// タスクを投入し、結果を受け取る future を返す
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<R()>>(
            [fn = std::forward<F>(f),
             ...a = std::forward<Args>(args)]() mutable { return fn(a...); });

        std::future<R> fut = task->get_future();
        {
            std::lock_guard lock(mutex_);
            if (stop_) throw std::runtime_error("submit on stopped ThreadPool");
            tasks_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

    std::size_t size() const noexcept { return workers_.size(); }

private:
    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex                        mutex_;
    std::condition_variable           cv_;
    bool                              stop_ = false;
};

}  // namespace chemcpp
```

使い方:

```cpp
chemcpp::ThreadPool pool;    // コア数分のスレッドを作る

std::vector<std::future<Fingerprint>> futures;
for (const auto& smi : smiles_list) {
    futures.push_back(pool.submit([&smi] {
        auto mol = parse_smiles(smi);
        return mol ? morgan_fingerprint(*mol) : Fingerprint{};
    }));
}
for (auto& f : futures) fps.push_back(f.get());
```

### condition_variable の仕組み

```cpp
cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
```

「条件が満たされるまでスレッドを眠らせる」機能です。
ビジーウェイト（`while (!ready) {}`）と違い、**CPUを消費しません**。

- `wait(lock, pred)`: `pred()` が true になるまで待つ。待っている間はロックを解放
- `notify_one()`: 待っているスレッドを1つ起こす
- `notify_all()`: 全部起こす

> ⚠️ **spurious wakeup**（誤起床）があるので、
> **必ず述語付きの `wait(lock, pred)` を使ってください。**
> `wait(lock)` だけだと、条件が満たされていないのに起きることがあります。

---

## 25.8 OpenMP — 最も手軽な並列化

```cpp
#include <omp.h>

#pragma omp parallel for
for (int i = 0; i < n; ++i) {
    results[i] = compute(mols[i]);
}
```

**1行追加するだけで並列化されます。** 驚くほど手軽。

```bash
g++ -std=c++20 -O2 -fopenmp prog.cpp -o prog     # GCC/Clang
cl /std:c++20 /O2 /openmp prog.cpp               # MSVC
```

CMake:

```cmake
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(myapp PRIVATE OpenMP::OpenMP_CXX)
endif()
```

### よく使う機能

```cpp
// 基本
#pragma omp parallel for
for (int i = 0; i < n; ++i) { ... }

// 動的スケジューリング（処理時間がバラバラなとき）
#pragma omp parallel for schedule(dynamic, 64)
for (int i = 0; i < n; ++i) { ... }

// リダクション（合計、最大など）
double sum = 0.0;
#pragma omp parallel for reduction(+:sum)
for (int i = 0; i < n; ++i) sum += values[i];

// カスタムリダクション
#pragma omp declare reduction(merge : Stats : omp_out.merge(omp_in)) \
    initializer(omp_priv = Stats{})
Stats total;
#pragma omp parallel for reduction(merge:total)
for (int i = 0; i < n; ++i) total.add(compute(i));

// スレッド数の指定
#pragma omp parallel for num_threads(8)

// クリティカルセクション
#pragma omp critical
{ shared_vector.push_back(x); }

// アトミック
#pragma omp atomic
++counter;

// 入れ子ループの平坦化
#pragma omp parallel for collapse(2)
for (int i = 0; i < n; ++i)
    for (int j = 0; j < m; ++j) { ... }
```

### ⚠️ OpenMP の注意点

1. **ループ変数は符号付き整数**である必要がある（古い仕様。OpenMP 3.0以降は緩和）
   ```cpp
   #pragma omp parallel for
   for (std::size_t i = 0; i < n; ++i)   // MSVC の OpenMP 2.0 ではエラー
   ```
   → `int` か `std::ptrdiff_t` を使う

2. **例外がスレッド境界を越えられない**
   ```cpp
   #pragma omp parallel for
   for (int i = 0; i < n; ++i) {
       throw std::runtime_error("x");     // ★ std::terminate
   }
   ```
   → try/catch をループ内に書き、エラーはフラグで伝える

3. **MSVCのOpenMPは古い**（2.0）。`/openmp:llvm` で新しい実装が使える

4. **`break` が使えない**（ループの構造が固定されている）

> 💡 **OpenMPを使うべき場面:**
> - 数値計算の単純なループ
> - 既存の逐次コードを最小の変更で並列化したい
> - 科学計算コミュニティ（HPC）との親和性
>
> **std::thread を使うべき場面:**
> - 複雑な同期が必要
> - タスク並列（ループでない）
> - 移植性が重要（OpenMPは環境依存が大きい）

---

## 25.9 std::execution（C++17 並列アルゴリズム）

標準アルゴリズムに実行ポリシーを渡すだけで並列化できます。

```cpp
#include <algorithm>
#include <execution>
#include <numeric>

std::sort(std::execution::par, v.begin(), v.end());
std::transform(std::execution::par_unseq, in.begin(), in.end(), out.begin(), f);
double sum = std::reduce(std::execution::par, v.begin(), v.end(), 0.0);
std::for_each(std::execution::par, v.begin(), v.end(), [](auto& x){ process(x); });
```

| ポリシー | 意味 |
|---|---|
| `seq` | 逐次（デフォルトと同じ） |
| `par` | 並列（マルチスレッド） |
| `par_unseq` | 並列 + ベクトル化（SIMD） |
| `unseq` | ベクトル化のみ（C++20） |

> ⚠️ **環境によっては動きません。**
> - GCC: **Intel TBB が必要**（`-ltbb`）
> - Clang(libc++): 長らく未対応だった
> - MSVC: 対応済み
>
> ```bash
> sudo apt install libtbb-dev
> g++ -std=c++20 -O2 prog.cpp -ltbb -o prog
> ```

> ⚠️ **`std::accumulate` は並列化できません。**
> 順序が保証されるためです。並列版は `std::reduce`（結合則を仮定）を使います。
> 浮動小数点では**結果が微妙に変わります**（加算の順序が変わるため）。
> 再現性が重要な計算では注意してください。

---

## 25.10 🧪 総合実装: 並列パイプライン

```cpp
// apps/parallel_screen.cpp
#include <chemcpp/smiles.hpp>
#include <chemcpp/fingerprint.hpp>
#include <chemcpp/descriptors.hpp>
#include <chemcpp/io.hpp>
#include <chemcpp/thread_pool.hpp>

#include <atomic>
#include <chrono>
#include <format>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

using namespace chemcpp;

struct Result {
    std::string id;
    std::string smiles;
    Fingerprint fp;
    Descriptors desc;
};

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    if (argc < 2) { std::cerr << "usage: parallel_screen <input.smi>\n"; return 1; }

    // ---- ① 読み込み（シングルスレッド。I/Oは並列化しにくい）----
    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::pair<std::string, std::string>> inputs;   // (smiles, id)
    inputs.reserve(1'000'000);
    read_smi(argv[1], [&](std::string_view smi, std::string_view id) {
        inputs.emplace_back(std::string(smi), std::string(id));
    });
    const double read_ms = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - t0).count();
    std::cout << std::format("read {} lines in {:.1f} ms\n", inputs.size(), read_ms);

    const unsigned nt = std::max(1u, std::thread::hardware_concurrency());
    std::cout << std::format("using {} threads\n\n", nt);

    // ---- ② 逐次処理（ベースライン）----
    {
        t0 = std::chrono::steady_clock::now();
        std::vector<Result> results;
        results.reserve(inputs.size());
        std::size_t failures = 0;

        for (const auto& [smi, id] : inputs) {
            auto mol = parse_smiles(smi);
            if (!mol) { ++failures; continue; }
            results.push_back({id, smi, morgan_fingerprint(*mol),
                               compute_descriptors(*mol)});
        }
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - t0).count();
        std::cout << std::format("sequential : {:.1f} ms  ({} ok, {} failed)\n",
                                 ms, results.size(), failures);
    }

    // ---- ③ 静的チャンク分割 ----
    {
        t0 = std::chrono::steady_clock::now();
        std::vector<Result>  results(inputs.size());
        std::vector<char>    valid(inputs.size(), 0);

        std::vector<std::jthread> threads;
        const std::size_t chunk = (inputs.size() + nt - 1) / nt;
        for (unsigned t = 0; t < nt; ++t) {
            const std::size_t b = t * chunk;
            const std::size_t e = std::min(b + chunk, inputs.size());
            if (b >= e) break;
            threads.emplace_back([&, b, e] {
                for (std::size_t i = b; i < e; ++i) {
                    auto mol = parse_smiles(inputs[i].first);
                    if (!mol) continue;
                    results[i] = {inputs[i].second, inputs[i].first,
                                  morgan_fingerprint(*mol),
                                  compute_descriptors(*mol)};
                    valid[i] = 1;      // ★ 各スレッドが別の要素に書く → 競合なし
                }
            });
        }
        threads.clear();               // jthread のデストラクタで join

        std::size_t ok = 0;
        for (char v : valid) ok += v;
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - t0).count();
        std::cout << std::format("static     : {:.1f} ms  ({} ok)\n", ms, ok);
    }

    // ---- ④ 動的負荷分散 ----
    {
        t0 = std::chrono::steady_clock::now();
        std::vector<Result> results(inputs.size());
        std::vector<char>   valid(inputs.size(), 0);
        std::atomic<std::size_t> next{0};
        constexpr std::size_t GRAIN = 256;

        std::vector<std::jthread> threads;
        for (unsigned t = 0; t < nt; ++t) {
            threads.emplace_back([&] {
                for (;;) {
                    const std::size_t b = next.fetch_add(GRAIN,
                                              std::memory_order_relaxed);
                    if (b >= inputs.size()) break;
                    const std::size_t e = std::min(b + GRAIN, inputs.size());
                    for (std::size_t i = b; i < e; ++i) {
                        auto mol = parse_smiles(inputs[i].first);
                        if (!mol) continue;
                        results[i] = {inputs[i].second, inputs[i].first,
                                      morgan_fingerprint(*mol),
                                      compute_descriptors(*mol)};
                        valid[i] = 1;
                    }
                }
            });
        }
        threads.clear();

        std::size_t ok = 0;
        for (char v : valid) ok += v;
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - t0).count();
        std::cout << std::format("dynamic    : {:.1f} ms  ({} ok)\n", ms, ok);
    }

    // ---- ⑤ スレッドプール ----
    {
        t0 = std::chrono::steady_clock::now();
        ThreadPool pool(nt);
        std::vector<Result> results(inputs.size());
        std::vector<char>   valid(inputs.size(), 0);

        constexpr std::size_t BATCH = 1000;
        std::vector<std::future<void>> futs;
        for (std::size_t b = 0; b < inputs.size(); b += BATCH) {
            const std::size_t e = std::min(b + BATCH, inputs.size());
            futs.push_back(pool.submit([&, b, e] {
                for (std::size_t i = b; i < e; ++i) {
                    auto mol = parse_smiles(inputs[i].first);
                    if (!mol) continue;
                    results[i] = {inputs[i].second, inputs[i].first,
                                  morgan_fingerprint(*mol),
                                  compute_descriptors(*mol)};
                    valid[i] = 1;
                }
            }));
        }
        for (auto& f : futs) f.get();

        std::size_t ok = 0;
        for (char v : valid) ok += v;
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - t0).count();
        std::cout << std::format("threadpool : {:.1f} ms  ({} ok)\n", ms, ok);
    }
    return 0;
}
```

参考結果（16コア、100万分子）:

```
read 1000000 lines in 218.4 ms
using 16 threads

sequential : 14820.3 ms  (997159 ok, 2841 failed)
static     :  1284.7 ms  (997159 ok)     11.5x
dynamic    :  1052.1 ms  (997159 ok)     14.1x
threadpool :  1098.6 ms  (997159 ok)     13.5x
```

**動的負荷分散が最速**でした。分子ごとに処理時間が異なるためです。

---

## 25.11 並列化のチェックリスト

```
□ プロファイルを取って、ここがボトルネックだと確認したか
□ アルゴリズムの改善余地はもう無いか
□ 各タスクは本当に独立か（共有状態がないか）
□ 結果の書き込み先は、要素ごとに別々か
□ ロックはループの外にあるか
□ false sharing を起こしていないか（隣接する要素への書き込み）
□ 例外がスレッド境界を越えないようになっているか
□ スレッド数は適切か（メモリ帯域が飽和していないか）
□ 実測して、本当に速くなったか
□ 結果が逐次版と一致するか（浮動小数点の順序を除く）
□ ThreadSanitizer でテストしたか（第29章）
```

> ⚠️ **並列化のバグは再現しません。**
> 1000回に1回しか起きないバグを、本番で踏みます。
> **必ず `-fsanitize=thread` でテストしてください**（第29章）。

---

## 25.12 この章のまとめ

- **並列化は最後の手段**。アルゴリズム改善の方が効くことが多い
- **`std::jthread`（C++20）を使う**（自動 join）
- **データ競合は未定義動作**。同じ変数を複数スレッドが書いたらアウト
- **異なる要素への書き込みは競合しない**（`results[i]` パターン）
- **ロックはループの外に**。ローカルに集めて最後に1回統合
- `std::atomic` も競合する（false sharing）。**パディングで分離**
- **動的負荷分散**（`atomic` でチャンクを取る）が実用的
- **OpenMP は `#pragma omp parallel for` 1行**で手軽
- `std::execution::par` は環境依存（GCCはTBBが必要）
- **`std::reduce` は並列化可能、`std::accumulate` は不可**
- スレッドプールで、細かいタスクを効率的に処理

> 📝 **練習問題 25-1**
>
> 25.10 のベンチマークを実行し、スレッド数を 1, 2, 4, 8, 16 と変えて
> スケーリングを測定してください。理想的な線形スケーリングから
> どこで乖離しますか?

> 📝 **練習問題 25-2**
>
> 次のコードのデータ競合を見つけて修正してください。
> ```cpp
> std::vector<double> results;
> #pragma omp parallel for
> for (int i = 0; i < n; ++i) {
>     results.push_back(compute(i));
> }
> ```

> 📝 **練習問題 25-3**
>
> `parallel_for_dynamic` の `grain` を 1, 16, 256, 4096 と変えて
> 性能を比較してください。最適値はどこですか?

> 📝 **練習問題 25-4**
>
> 第23章の類似度検索を並列化し、
> クエリ単位の並列（複数クエリを同時処理）と
> データベース単位の並列（1クエリを分割）を比較してください。

---

→ [第26章 Pythonから使う（pybind11）](ch26-pybind11.md)
