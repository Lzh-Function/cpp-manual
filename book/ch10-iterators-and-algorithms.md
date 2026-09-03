# 第10章 イテレータと \<algorithm\>

Pythonのリスト内包表記、`sorted()`, `map()`, `filter()`, `any()`, `sum()` に
相当するものが、C++では `<algorithm>` ヘッダにまとまっています。

## 10.1 イテレータとは

**イテレータは「コンテナの中の位置を指すもの」**です。
ポインタの一般化だと思ってください。

```cpp
std::vector<int> v = {10, 20, 30};

auto it = v.begin();      // 先頭要素を指す
std::cout << *it << "\n"; // 10   （* でデリファレンス。ポインタと同じ）
++it;                     // 次へ
std::cout << *it << "\n"; // 20
```

```
   v = [10][20][30]
        ▲          ▲
     begin()     end()   ← end() は「最後の次」を指す（要素ではない）
```

> ⚠️ **`end()` は最後の要素ではなく、「最後の次」**です。
> だから `*v.end()` は未定義動作です。
> ループの終了条件 `it != v.end()` に使うためのマーカーです。

### 半開区間 [begin, end)

```
   [begin, end)   ← begin は含む、end は含まない
```

Pythonの `range(0, n)` や `s[2:5]` と同じ「半開区間」の考え方です。
だから `end() - begin() == size()` になります。

### イテレータを使ったループ

```cpp
// 昔ながらの書き方
for (auto it = v.begin(); it != v.end(); ++it) {
    std::cout << *it << "\n";
}

// range-based for（内部では上と同じことをしている）
for (const auto& x : v) {
    std::cout << x << "\n";
}
```

**range-based for は、上のイテレータループの糖衣構文**です。
だから `begin()` / `end()` を持つ型なら何でも range-based for が使えます。

### イテレータの種類

| 種類 | できること | 例 |
|---|---|---|
| 入力イテレータ | 1回だけ読みながら前進 | 入力ストリーム |
| 前方イテレータ | 何度でも読みながら前進 | `forward_list` |
| 双方向イテレータ | `++` と `--` | `list`, `map`, `set` |
| **ランダムアクセス** | `it + 5`, `it[3]`, `it2 - it1` | **`vector`, `array`, `deque`** |
| 連続イテレータ (C++20) | メモリが連続と保証 | `vector`, `array`, `string` |

`std::sort` はランダムアクセスイテレータを要求します。
だから `std::list` には `std::sort` が使えず、専用の `lst.sort()` があります。

### 便利なイテレータ

```cpp
v.begin()   / v.end()     // 通常
v.cbegin()  / v.cend()    // const 版（読み取り専用）
v.rbegin()  / v.rend()    // 逆順（末尾から先頭へ）

// 逆順ループ
for (auto it = v.rbegin(); it != v.rend(); ++it) {
    std::cout << *it;     // 30, 20, 10
}
```

C++20 の `ranges` があれば:

```cpp
#include <ranges>
for (int x : v | std::views::reverse) { }
```

---

## 10.2 ソート

### 基本

```cpp
#include <algorithm>

std::vector<double> mws = {180.16, 46.07, 194.19, 206.28};
std::sort(mws.begin(), mws.end());                 // 昇順
std::sort(mws.begin(), mws.end(), std::greater<>{}); // 降順
```

```python
mws.sort()                    # Python
mws.sort(reverse=True)
```

### 比較関数（Pythonの key= に相当）

```cpp
struct Mol { std::string id; double mw; double logp; };
std::vector<Mol> mols = { ... };

// MW で昇順
std::sort(mols.begin(), mols.end(),
          [](const Mol& a, const Mol& b) { return a.mw < b.mw; });

// logP で降順
std::sort(mols.begin(), mols.end(),
          [](const Mol& a, const Mol& b) { return a.logp > b.logp; });

// 複数キー: MW 昇順、同じなら logP 降順
std::sort(mols.begin(), mols.end(), [](const Mol& a, const Mol& b) {
    if (a.mw != b.mw) return a.mw < b.mw;
    return a.logp > b.logp;
});
```

```python
mols.sort(key=lambda m: m.mw)                         # Python
mols.sort(key=lambda m: (m.mw, -m.logp))
```

> ⚠️ **比較関数は「strict weak ordering」でなければなりません。**
> つまり `comp(a, a)` は必ず `false` を返す必要があります。
>
> ```cpp
> // ✗ 危険: <= を使うと a==b のとき true になる
> std::sort(v.begin(), v.end(), [](int a, int b){ return a <= b; });
> ```
> これは**クラッシュや無限ループを引き起こします**（実際に起きます）。
> **必ず `<` か `>` を使ってください。**

### ソートの種類

```cpp
std::sort(b, e);              // O(n log n)。安定でない（同値の順序は保証されない）
std::stable_sort(b, e);       // 安定ソート。少し遅く、追加メモリを使う
std::partial_sort(b, b+K, e); // 先頭K個だけ正しくソート（Top-K に便利）
std::nth_element(b, b+K, e);  // K番目の要素だけ正しい位置に（中央値などに）
```

🧪 **Top-K に `partial_sort` を使う:**

```cpp
// 100万件から上位100件だけ欲しい
std::partial_sort(scores.begin(), scores.begin() + 100, scores.end(),
                  std::greater<>{});
// scores[0..99] が上位100件（降順）。残りは未定義な順序
```

`std::sort` が O(n log n) なのに対し、`partial_sort` は **O(n log K)** です。
`priority_queue`（第9章）と同じ計算量ですが、
既に全データが配列にあるならこちらの方がシンプルで速いです。

### ソート済みかチェック

```cpp
bool ok = std::is_sorted(v.begin(), v.end());
```

---

## 10.3 検索

```cpp
#include <algorithm>

std::vector<int> v = {5, 3, 8, 1, 9};

// 線形検索 O(n)
auto it = std::find(v.begin(), v.end(), 8);
if (it != v.end()) {
    std::cout << "found at index " << (it - v.begin()) << "\n";
}

// 条件付き検索
auto it2 = std::find_if(v.begin(), v.end(), [](int x){ return x > 5; });

// 個数を数える
int n = std::count(v.begin(), v.end(), 8);
int m = std::count_if(v.begin(), v.end(), [](int x){ return x % 2 == 0; });
```

```python
8 in v                                    # 存在確認
v.index(8)                                # 位置
next(x for x in v if x > 5)               # 条件検索
v.count(8)                                # 個数
sum(1 for x in v if x % 2 == 0)           # 条件付き個数
```

### 二分探索（ソート済み配列に対して O(log n)）

```cpp
std::vector<double> sorted_mw = {46.07, 151.16, 180.16, 194.19, 206.28};

bool found = std::binary_search(sorted_mw.begin(), sorted_mw.end(), 180.16);

// 180.0 以上の最初の位置
auto lo = std::lower_bound(sorted_mw.begin(), sorted_mw.end(), 180.0);

// 200.0 より大きい最初の位置
auto hi = std::upper_bound(sorted_mw.begin(), sorted_mw.end(), 200.0);

// [180, 200] の範囲の要素数
std::cout << (hi - lo) << "\n";
```

```python
import bisect
bisect.bisect_left(sorted_mw, 180.0)      # lower_bound
bisect.bisect_right(sorted_mw, 200.0)     # upper_bound
```

> 💡 **ソート済み vector + 二分探索 は、`std::map` より速いことが多い**です。
> データが連続しているのでキャッシュ効率が良く、
> 更新が少ない読み取り中心のインデックスに最適です。
>
> 🧪 「MW でソートした化合物リストから、範囲でフィルタする」なら
> `std::map` より `std::vector` + `lower_bound` を推奨します。

---

## 10.4 変換・集約（Pythonの map/filter/sum 相当）

### transform（map 相当）

```cpp
std::vector<double> mws = {180.16, 194.19, 206.28};
std::vector<double> logs(mws.size());

std::transform(mws.begin(), mws.end(), logs.begin(),
               [](double x){ return std::log(x); });
```

```python
logs = [math.log(x) for x in mws]         # Python
```

2つの入力から1つの出力を作る版もあります:

```cpp
std::vector<double> ratio(a.size());
std::transform(a.begin(), a.end(), b.begin(), ratio.begin(),
               [](double x, double y){ return x / y; });
```

```python
ratio = [x / y for x, y in zip(a, b)]     # Python
```

### accumulate（sum / reduce 相当）

```cpp
#include <numeric>

std::vector<double> v = {1.0, 2.0, 3.0};

double sum  = std::accumulate(v.begin(), v.end(), 0.0);      // 6.0
double prod = std::accumulate(v.begin(), v.end(), 1.0,
                              std::multiplies<>{});          // 6.0

// カスタム集約
double sum_sq = std::accumulate(v.begin(), v.end(), 0.0,
                                [](double acc, double x){ return acc + x*x; });
```

> ⚠️ **初期値の型に注意！**
> ```cpp
> std::vector<double> v = {1.5, 2.5};
> auto s = std::accumulate(v.begin(), v.end(), 0);    // ★ 0 は int！ 結果は 3（切り捨て）
> auto t = std::accumulate(v.begin(), v.end(), 0.0);  // ✓ 4.0
> ```
> これは実際に頻発するバグです。**必ず `0.0` と書いてください。**

C++23 なら `std::ranges::fold_left` が使えます。

### inner_product（内積）

```cpp
double dot = std::inner_product(a.begin(), a.end(), b.begin(), 0.0);
```

```python
dot = sum(x*y for x, y in zip(a, b))      # または np.dot(a, b)
```

### iota（range 相当）

```cpp
std::vector<int> idx(10);
std::iota(idx.begin(), idx.end(), 0);     // 0, 1, 2, ..., 9
```

```python
idx = list(range(10))                     # Python
```

🧪 **インデックスソートの定番パターン:**

```cpp
// スコアの降順に、元の添字を並べ替える（argsort 相当）
std::vector<std::size_t> order(scores.size());
std::iota(order.begin(), order.end(), 0);
std::sort(order.begin(), order.end(),
          [&scores](std::size_t a, std::size_t b){ return scores[a] > scores[b]; });
// order[0] が最高スコアの分子の添字
```

```python
order = np.argsort(-scores)               # NumPy
```

### partial_sum（累積和）

```cpp
std::vector<int> v = {1, 2, 3, 4};
std::vector<int> cumsum(v.size());
std::partial_sum(v.begin(), v.end(), cumsum.begin());   // 1, 3, 6, 10
```

```python
cumsum = np.cumsum(v)                     # NumPy
list(itertools.accumulate(v))             # 標準ライブラリ
```

---

## 10.5 述語（any / all / none）

```cpp
bool any = std::any_of(v.begin(), v.end(), [](int x){ return x > 5; });
bool all = std::all_of(v.begin(), v.end(), [](int x){ return x > 0; });
bool non = std::none_of(v.begin(), v.end(), [](int x){ return x < 0; });
```

```python
any(x > 5 for x in v)                     # Python
all(x > 0 for x in v)
not any(x < 0 for x in v)
```

🧪 Lipinski の Rule of Five チェックに使えます:

```cpp
std::vector<Mol> mols = ...;
bool all_druglike = std::all_of(mols.begin(), mols.end(),
                                [](const Mol& m){ return m.mw < 500; });
```

---

## 10.6 最大・最小

```cpp
auto max_it = std::max_element(v.begin(), v.end());
auto min_it = std::min_element(v.begin(), v.end());
auto [mn, mx] = std::minmax_element(v.begin(), v.end());   // 両方一度に

std::cout << *max_it << " at index " << (max_it - v.begin()) << "\n";

// 比較関数付き
auto heaviest = std::max_element(mols.begin(), mols.end(),
                                 [](const Mol& a, const Mol& b){ return a.mw < b.mw; });
```

```python
max(v)                                    # Python
v.index(max(v))
max(mols, key=lambda m: m.mw)
```

> ⚠️ **`max_element` は空の範囲に対して `end()` を返します。**
> `*max_element(...)` する前に、`v.empty()` をチェックしてください。

2値の最大最小は関数版:

```cpp
int m = std::max(a, b);
int n = std::min(a, b);
int c = std::clamp(x, lo, hi);            // C++17: lo <= x <= hi に丸める
```

---

## 10.7 その他の便利アルゴリズム

```cpp
// 埋める
std::fill(v.begin(), v.end(), 0);
std::fill_n(v.begin(), 10, 0);

// コピー
std::copy(src.begin(), src.end(), dst.begin());
std::copy_if(src.begin(), src.end(), std::back_inserter(dst),
             [](int x){ return x > 0; });      // filter 相当

// 反転
std::reverse(v.begin(), v.end());

// 回転
std::rotate(v.begin(), v.begin() + 3, v.end());

// シャッフル
#include <random>
std::mt19937 rng(42);
std::shuffle(v.begin(), v.end(), rng);

// 重複除去（要ソート）
std::sort(v.begin(), v.end());
v.erase(std::unique(v.begin(), v.end()), v.end());

// 分割（条件を満たすものを前に集める）
auto mid = std::partition(v.begin(), v.end(), [](int x){ return x > 0; });
// [begin, mid) が正の数、[mid, end) がそれ以外

// 生成
std::generate(v.begin(), v.end(), [n = 0]() mutable { return n++; });

// 各要素に対して実行（副作用目的）
std::for_each(v.begin(), v.end(), [](int x){ std::cout << x; });
```

### back_inserter

```cpp
std::vector<int> dst;                     // 空
std::copy_if(src.begin(), src.end(), std::back_inserter(dst),
             [](int x){ return x > 0; });
```

`std::back_inserter(dst)` は「代入されると `dst.push_back()` する」
という特殊なイテレータです。出力先のサイズが事前に分からないときに使います。

> 💡 `dst.reserve(src.size())` を先に呼んでおくと速くなります。

---

## 10.8 ★C++20 ranges★ — 現代的な書き方

`begin()`, `end()` を毎回書くのが面倒だと思いましたか? その通りです。
C++20 の `ranges` がそれを解決します。

```cpp
#include <algorithm>
#include <ranges>

std::vector<int> v = {5, 3, 8, 1, 9};

std::ranges::sort(v);                                   // イテレータ不要！
auto it = std::ranges::find(v, 8);
int  n  = std::ranges::count_if(v, [](int x){ return x > 3; });
bool a  = std::ranges::any_of(v, [](int x){ return x > 8; });
auto mx = std::ranges::max_element(v);
```

### プロジェクション（key= 相当）

```cpp
struct Mol { std::string id; double mw; };
std::vector<Mol> mols = ...;

std::ranges::sort(mols, {}, &Mol::mw);          // ★ MW で昇順ソート
std::ranges::sort(mols, std::greater{}, &Mol::mw);  // MW で降順

auto it = std::ranges::find(mols, "CHEMBL25", &Mol::id);
auto mx = std::ranges::max_element(mols, {}, &Mol::mw);
```

```python
mols.sort(key=lambda m: m.mw)             # Python の key= とほぼ同じ！
```

**第3引数がプロジェクション（射影）** で、Pythonの `key=` に相当します。
これは本当に便利です。

### views — 遅延評価パイプライン

```cpp
#include <ranges>
namespace rv = std::views;

std::vector<Mol> mols = ...;

// MW が 500 未満の分子の ID を取り出す
for (const auto& id : mols
                    | rv::filter([](const Mol& m){ return m.mw < 500; })
                    | rv::transform([](const Mol& m){ return m.id; })) {
    std::cout << id << "\n";
}
```

```python
# Python のジェネレータ式に相当
ids = (m.id for m in mols if m.mw < 500)
```

**重要: これは遅延評価です。** 中間のvectorは作られません。
Pythonのジェネレータと同じで、必要になったときに1要素ずつ計算されます。

主な views:

```cpp
rv::filter(pred)          // 条件を満たすものだけ
rv::transform(func)       // 変換
rv::take(n)               // 先頭n個
rv::drop(n)               // 先頭n個を飛ばす
rv::take_while(pred)      // 条件を満たす間
rv::drop_while(pred)
rv::reverse               // 逆順
rv::iota(0, 10)           // 0..9 の範囲（Python の range）
rv::split(delim)          // 分割
rv::join                  // 平坦化
rv::keys / rv::values     // map の キー/値
rv::enumerate             // C++23
rv::zip                   // C++23
rv::chunk(n)              // C++23: n個ずつのブロックに分割
```

### vector に変換する

```cpp
// C++23
auto ids = mols
         | rv::filter([](const Mol& m){ return m.mw < 500; })
         | rv::transform([](const Mol& m){ return m.id; })
         | std::ranges::to<std::vector>();

// C++20 まで（手動）
std::vector<std::string> ids;
for (const auto& id : mols | rv::filter(...) | rv::transform(...)) {
    ids.push_back(id);
}
```

> ⚠️ **ranges の注意点**
>
> 1. **コンパイル時間が増えます**（テンプレートが重い）
> 2. **エラーメッセージが長大**になります（数百行のことも）
> 3. **デバッガで追いにくい**
> 4. 一部のviewには**性能上の落とし穴**があります
>    （`filter` の後の `reverse` など、複雑な組み合わせ）
>
> 私の推奨: **単純なパイプラインには積極的に使う。
> 複雑になったら普通のループに戻す。**
> 性能が重要なホットループでは、必ず実測してから採用してください。

---

## 10.9 🧪 実践: 化合物ライブラリのフィルタリングとランキング

```cpp
// code/ch10/screening.cpp
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

struct Compound {
    std::string id;
    std::string smiles;
    double      mw;
    double      logp;
    int         hbd;
    int         hba;
    double      tpsa;
    double      activity;   // pIC50
};

// Lipinski の Rule of Five（違反1つまで許容）
bool passes_ro5(const Compound& c) {
    int v = 0;
    if (c.mw   > 500.0) ++v;
    if (c.logp > 5.0)   ++v;
    if (c.hbd  > 5)     ++v;
    if (c.hba  > 10)    ++v;
    return v <= 1;
}

// Ligand Efficiency の簡易版（重原子数の代わりに MW を使用）
double ligand_efficiency(const Compound& c) {
    double heavy_atoms_est = c.mw / 13.0;   // 粗い推定
    return heavy_atoms_est > 0 ? c.activity * 1.37 / heavy_atoms_est : 0.0;
}

int main() {
    std::vector<Compound> lib = {
        {"CMP001", "CC(=O)Oc1ccccc1C(=O)O",      180.16, 1.19, 1, 3, 63.6, 5.2},
        {"CMP002", "Cn1cnc2c1c(=O)n(C)c(=O)n2C", 194.19,-1.03, 0, 6, 61.8, 4.1},
        {"CMP003", "CC(C)Cc1ccc(cc1)C(C)C(=O)O", 206.28, 3.97, 1, 2, 37.3, 6.8},
        {"CMP004", "CC(=O)Nc1ccc(O)cc1",         151.16, 0.46, 2, 2, 49.3, 3.9},
        {"CMP005", "CCCCCCCCCCCCCCCCCCCC(=O)O",  312.53, 8.23, 1, 2, 37.3, 7.1},
        {"CMP006", "CCO",                         46.07,-0.31, 1, 1, 20.2, 1.2},
        {"CMP007", "c1ccc2c(c1)ccc1ccccc12",     178.23, 4.45, 0, 0,  0.0, 5.9},
        {"CMP008", "NC(Cc1ccccc1)C(=O)O",        165.19,-1.52, 2, 3, 63.3, 4.5},
    };

    std::cout << "library size: " << lib.size() << "\n\n";

    // --- ① フィルタ: Ro5 を通過する化合物 ---
    std::vector<Compound> filtered;
    filtered.reserve(lib.size());
    std::copy_if(lib.begin(), lib.end(), std::back_inserter(filtered), passes_ro5);
    std::cout << "passed Ro5: " << filtered.size() << " / " << lib.size() << "\n";

    // --- ② 統計 ---
    double sum_mw = std::accumulate(filtered.begin(), filtered.end(), 0.0,
                                    [](double acc, const Compound& c){ return acc + c.mw; });
    double mean_mw = sum_mw / filtered.size();

    double var = std::accumulate(filtered.begin(), filtered.end(), 0.0,
                                 [mean_mw](double acc, const Compound& c){
                                     double d = c.mw - mean_mw;
                                     return acc + d * d;
                                 }) / filtered.size();

    std::cout << "mean MW: " << mean_mw << "  sd: " << std::sqrt(var) << "\n\n";

    // --- ③ 活性でソートして Top-3 ---
    std::vector<std::size_t> order(filtered.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return filtered[a].activity > filtered[b].activity;
    });

    std::cout << "Top 3 by activity:\n";
    for (std::size_t k = 0; k < std::min<std::size_t>(3, order.size()); ++k) {
        const auto& c = filtered[order[k]];
        std::cout << "  " << c.id << "  pIC50=" << c.activity
                  << "  LE=" << ligand_efficiency(c) << "\n";
    }
    std::cout << "\n";

    // --- ④ 述語チェック ---
    bool any_toxic_logp = std::any_of(lib.begin(), lib.end(),
                                      [](const Compound& c){ return c.logp > 5.0; });
    std::cout << "any logP > 5: " << (any_toxic_logp ? "yes" : "no") << "\n";

    int n_polar = std::count_if(lib.begin(), lib.end(),
                                [](const Compound& c){ return c.tpsa > 60.0; });
    std::cout << "TPSA > 60: " << n_polar << " compounds\n\n";

    // --- ⑤ MW でソートして二分探索 ---
    std::vector<Compound> by_mw = lib;
    std::sort(by_mw.begin(), by_mw.end(),
              [](const Compound& a, const Compound& b){ return a.mw < b.mw; });

    auto lo = std::lower_bound(by_mw.begin(), by_mw.end(), 150.0,
                               [](const Compound& c, double v){ return c.mw < v; });
    auto hi = std::upper_bound(by_mw.begin(), by_mw.end(), 210.0,
                               [](double v, const Compound& c){ return v < c.mw; });

    std::cout << "MW 150-210 (" << (hi - lo) << " compounds):\n";
    for (auto it = lo; it != hi; ++it) {
        std::cout << "  " << it->id << "  " << it->mw << "\n";
    }
    return 0;
}
```

> ⚠️ **`lower_bound` / `upper_bound` の比較関数の引数順に注意**
> - `lower_bound(first, last, value, comp)`: `comp(element, value)`
> - `upper_bound(first, last, value, comp)`: `comp(value, element)`
>
> 引数の順序が**逆**です。間違えるとコンパイルエラーになるので気付けますが、
> 混乱しやすいポイントです。

同じことを ranges で書くと:

```cpp
std::ranges::sort(by_mw, {}, &Compound::mw);   // これだけ
auto n = std::ranges::count_if(lib, [](double t){ return t > 60.0; }, &Compound::tpsa);
```

短い！ ranges が使える環境ならこちらを推奨します。

---

## 10.10 Python → C++ アルゴリズム対応表（保存版）

| Python | C++ (`<algorithm>` / `<numeric>`) |
|---|---|
| `sorted(v)` | `std::sort(v.begin(), v.end())` |
| `sorted(v, key=f)` | `std::sort(v.begin(), v.end(), cmp)` / `std::ranges::sort(v, {}, f)` |
| `sorted(v, reverse=True)` | `std::sort(v.begin(), v.end(), std::greater<>{})` |
| `x in v` | `std::find(v.begin(), v.end(), x) != v.end()` |
| `v.index(x)` | `std::find(...) - v.begin()` |
| `v.count(x)` | `std::count(v.begin(), v.end(), x)` |
| `[f(x) for x in v]` | `std::transform` |
| `[x for x in v if p(x)]` | `std::copy_if` + `back_inserter` / `views::filter` |
| `sum(v)` | `std::accumulate(v.begin(), v.end(), 0.0)` |
| `max(v)` / `min(v)` | `*std::max_element(...)` / `*std::min_element(...)` |
| `max(v, key=f)` | `*std::max_element(v.begin(), v.end(), cmp)` |
| `any(...)` / `all(...)` | `std::any_of` / `std::all_of` |
| `reversed(v)` | `std::reverse` / `views::reverse` |
| `list(range(n))` | `std::iota` |
| `itertools.accumulate` | `std::partial_sum` |
| `np.argsort` | `iota` + `sort` （10.4節参照） |
| `np.dot(a,b)` | `std::inner_product` |
| `random.shuffle(v)` | `std::shuffle(v.begin(), v.end(), rng)` |
| `bisect.bisect_left` | `std::lower_bound` |
| `set(v)` （重複除去） | `sort` + `unique` + `erase` |
| `heapq.nlargest(k, v)` | `std::partial_sort` / `std::priority_queue` |
| `statistics.median(v)` | `std::nth_element(b, b+n/2, e)` |

---

## 10.11 この章のまとめ

- イテレータは「位置を指すもの」。`end()` は**最後の次**
- 範囲は `[begin, end)` の半開区間
- `std::sort` の比較関数は**必ず `<` か `>`**（`<=` はクラッシュの原因）
- Top-K には `std::partial_sort`（O(n log K)）
- ソート済みなら `lower_bound` / `upper_bound` で O(log n) 検索
- `std::accumulate` の**初期値の型に注意**（`0` と `0.0`）
- argsort は `iota` + `sort` のイディオム
- **C++20 の `ranges` を使えば `begin()/end()` が省ける**上に
  プロジェクション（`key=` 相当）が使える
- `views` は遅延評価。Pythonのジェネレータと同じ

> 📝 **練習問題 10-1**
>
> `std::vector<Compound>` から「TPSA が 40〜100 かつ MW < 400」の化合物を抽出し、
> logP の絶対値が小さい順（つまり `|logP|` 昇順）にソートするコードを書いてください。
> 通常のアルゴリズム版と ranges 版の両方で。

> 📝 **練習問題 10-2**
>
> 次のコードのバグを見つけてください。
> ```cpp
> std::vector<double> scores = {0.9, 0.7, 0.85};
> double total = std::accumulate(scores.begin(), scores.end(), 0);
> std::cout << total / scores.size();
> ```

> 📝 **練習問題 10-3**
>
> 100万個の double から上位1000件を取り出す処理を、
> ① `std::sort` 全体ソート ② `std::partial_sort` ③ `std::nth_element` + `sort`
> の3通りで実装し、速度を比較してください。

> 📝 **練習問題 10-4**
>
> 分子の中央値MWを求める関数を `std::nth_element` を使って書いてください。
> `std::sort` を使う版より速いはずです。なぜですか?

---

→ [第11章 クラスとRAII](ch11-classes-and-raii.md)
