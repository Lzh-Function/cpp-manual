# 第9章 STLコンテナ完全ガイド

Pythonには list / dict / set / tuple の4つしかありませんが、
C++には十数種類のコンテナがあります。用途に応じて選べるので、
**適切なものを選べば桁違いに速くなります**。

## 9.1 全体像

```
                    STL コンテナ
                         │
        ┌────────────────┼────────────────┐
        │                │                │
   シーケンス          連想配列         コンテナアダプタ
        │                │                │
  vector             順序あり(木)      stack
  array              ├ map            queue
  deque              ├ set            priority_queue
  list               ├ multimap
  forward_list       └ multiset
                     順序なし(ハッシュ)
                     ├ unordered_map
                     ├ unordered_set
                     ├ unordered_multimap
                     └ unordered_multiset
```

### 選択フローチャート

```
  キーで検索したい?
   │
   ├─ No ──> 順序が必要? ─ Yes ─> vector（ソート済みなら二分探索も可）
   │                       No ──> vector
   │          （先頭への挿入が頻繁 → deque）
   │          （中間への挿入が頻繁かつ参照を保ちたい → list ※めったにない）
   │
   └─ Yes ─> キーの順序が必要?
              │
              ├─ Yes ──> std::map / std::set          （O(log n)、赤黒木）
              └─ No ───> std::unordered_map / _set    （O(1) 平均、ハッシュ）
                          ★ こちらの方が速い。順序が要らないなら常にこちら
```

---

## 9.2 std::unordered_map — Pythonの dict

```cpp
#include <unordered_map>
#include <string>

std::unordered_map<std::string, double> mw_table;

mw_table["CCO"] = 46.07;
mw_table["c1ccccc1"] = 78.11;
mw_table.emplace("CC(=O)O", 60.05);

std::cout << mw_table["CCO"] << "\n";       // 46.07
std::cout << mw_table.size() << "\n";       // 3
```

### Python dict との対応表

| やりたいこと | Python | C++ |
|---|---|---|
| 作る | `d = {}` | `std::unordered_map<K,V> d;` |
| 代入 | `d[k] = v` | `d[k] = v` |
| 取得 | `d[k]` | `d.at(k)`（無ければ例外） |
| 安全な取得 | `d.get(k, default)` | 後述 |
| 存在確認 | `k in d` | `d.contains(k)`（C++20）/ `d.count(k)` / `d.find(k) != d.end()` |
| 削除 | `del d[k]` | `d.erase(k)` |
| サイズ | `len(d)` | `d.size()` |
| 全消去 | `d.clear()` | `d.clear()` |
| キー一覧 | `d.keys()` | ループで `pair.first` |
| 値一覧 | `d.values()` | ループで `pair.second` |
| 反復 | `for k, v in d.items()` | `for (const auto& [k, v] : d)` |
| デフォルト値付き挿入 | `d.setdefault(k, v)` | `d.try_emplace(k, v)` |
| 更新 | `d.update(other)` | `d.merge(other)` / ループ |

### ⚠️ `operator[]` の落とし穴

```cpp
std::unordered_map<std::string, int> counts;

if (counts["CCO"] > 0) {      // ★ "CCO" が存在しないと、値0で自動挿入される！
    // ...
}
std::cout << counts.size();   // 1  ← 読んだだけなのに増えている
```

**`d[k]` は、キーが無ければ値をデフォルト構築して挿入します。**
Pythonの `d[k]` が `KeyError` を投げるのとは正反対の挙動です。

```cpp
// 読むだけなら
if (auto it = counts.find("CCO"); it != counts.end()) {
    use(it->second);
}
// または
if (counts.contains("CCO")) { ... }        // C++20
int v = counts.count("CCO") ? counts.at("CCO") : 0;
```

> 💡 **逆に、カウンタとしては便利です:**
> ```cpp
> std::unordered_map<std::string, int> element_count;
> for (const auto& sym : symbols) {
>     ++element_count[sym];      // 無ければ0で作られてから++される
> }
> ```
> Pythonの `collections.Counter` 相当が1行で書けます。

### 反復と構造化束縛

```cpp
for (const auto& [smiles, mw] : mw_table) {      // C++17 構造化束縛
    std::cout << smiles << " -> " << mw << "\n";
}

// C++17 以前
for (const auto& pair : mw_table) {
    std::cout << pair.first << " -> " << pair.second << "\n";
}
```

> ⚠️ **`unordered_map` の反復順序は不定**です。
> 挿入順でも、ソート順でもありません。実装依存で、要素数によっても変わります。
>
> Python 3.7+ の dict は挿入順を保つので、ここは違います。
> 順序が必要なら `std::map`（キー順）を使うか、
> 別に `std::vector<K>` で順序を保持してください。

### 挿入の作法

```cpp
std::unordered_map<std::string, Fingerprint> index;

index["CCO"] = fp;                    // 既存なら上書き。デフォルト構築のコストあり
index.insert({"CCO", fp});            // 既存なら何もしない
index.insert_or_assign("CCO", fp);    // 既存なら上書き（C++17）
index.emplace("CCO", fp);             // 既存なら何もしない、直接構築
index.try_emplace("CCO", fp);         // ★ 既存なら fp を構築すらしない（C++17）
```

> 💡 **`try_emplace` が最も効率的**です。
> `emplace` は「まず引数から値を構築 → 既存かチェック → 既存なら捨てる」なのに対し、
> `try_emplace` は「既存かチェック → 無ければ構築」です。
> 値が重いとき（Fingerprint 256バイトなど）に効きます。

### 戻り値のチェック

```cpp
auto [it, inserted] = index.emplace("CCO", fp);
if (!inserted) {
    std::cout << "already exists\n";
}
// it は挿入された（または既存の）要素へのイテレータ
```

### 独自の型をキーにする

```cpp
struct AtomPair {
    int a, b;
    bool operator==(const AtomPair& o) const { return a == o.a && b == o.b; }
};

// std に特殊化を追加してハッシュ関数を定義
template <>
struct std::hash<AtomPair> {
    std::size_t operator()(const AtomPair& p) const noexcept {
        // 単純だが実用的なハッシュ結合
        std::size_t h1 = std::hash<int>{}(p.a);
        std::size_t h2 = std::hash<int>{}(p.b);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

std::unordered_map<AtomPair, double> bond_lengths;
```

`0x9e3779b9...` は黄金比に由来する定数で、boost の `hash_combine` で使われています。
ハッシュ値を混ぜるときの定番です。

> 💡 **ハッシュの質は性能に直結します。**
> 悪いハッシュ関数だと衝突が増え、`unordered_map` が O(n) に劣化します。
> 自作するときは、上のように複数の値をしっかり混ぜてください。

---

## 9.3 std::map — ソート済み連想配列

```cpp
#include <map>

std::map<std::string, double> sorted_mw;
sorted_mw["c1ccccc1"] = 78.11;
sorted_mw["CCO"] = 46.07;
sorted_mw["CC(=O)O"] = 60.05;

for (const auto& [k, v] : sorted_mw) {
    std::cout << k << "\n";       // ★ キーの辞書順で出てくる
}
// CC(=O)O
// CCO
// c1ccccc1
```

内部は**赤黒木**（平衡二分探索木）。

| | `std::map` | `std::unordered_map` |
|---|---|---|
| 内部構造 | 赤黒木 | ハッシュテーブル |
| 検索 | O(log n) | **O(1) 平均**、O(n) 最悪 |
| 挿入 | O(log n) | O(1) 平均 |
| 反復順序 | **キー昇順** | 不定 |
| メモリ | 要素ごとにノード（ポインタ3個分のオーバーヘッド） | バケット配列 + ノード |
| 要件 | `operator<` | `std::hash` と `operator==` |
| イテレータ無効化 | 挿入で無効化されない | rehash で無効化される |

> 💡 **原則: `unordered_map` を使う。**
> 順序が必要なとき、または範囲検索（「MW が 300〜400 の分子」）が
> 必要なときだけ `map` を使ってください。

### map の範囲検索

```cpp
std::map<double, std::string> by_mw;
by_mw[180.16] = "aspirin";
by_mw[194.19] = "caffeine";
by_mw[206.28] = "ibuprofen";

// MW が 190 以上 210 未満の分子
auto lo = by_mw.lower_bound(190.0);   // 190 以上の最初
auto hi = by_mw.lower_bound(210.0);   // 210 以上の最初
for (auto it = lo; it != hi; ++it) {
    std::cout << it->second << "\n";  // caffeine, ibuprofen
}
```

これは `unordered_map` ではできません。

---

## 9.4 std::set / std::unordered_set — Pythonの set

```cpp
#include <unordered_set>

std::unordered_set<std::string> seen;

seen.insert("CCO");
if (seen.contains("CCO")) { }        // C++20
if (seen.count("CCO")) { }           // 従来の書き方
seen.erase("CCO");
```

### 重複除去の定番パターン

```python
unique = list(set(smiles_list))       # Python
```

```cpp
// 方法A: unordered_set 経由（順序は失われる）
std::unordered_set<std::string> s(smiles_list.begin(), smiles_list.end());
std::vector<std::string> unique(s.begin(), s.end());

// 方法B: ソート + unique（順序はソート順。★こちらの方が速いことが多い）
std::vector<std::string> unique = smiles_list;
std::sort(unique.begin(), unique.end());
unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
```

> 💡 **要素数が少ない（数千以下）なら、方法Bの方が速い**ことがよくあります。
> ハッシュテーブルはノードがメモリ上に散らばるのに対し、
> ソートは連続メモリ上で完結するからです。第28章のキャッシュの話です。

### set の集合演算

```cpp
#include <algorithm>
#include <set>

std::set<int> a = {1, 2, 3, 4};
std::set<int> b = {3, 4, 5, 6};
std::vector<int> result;

std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                      std::back_inserter(result));   // {3, 4}
std::set_union(a.begin(), a.end(), b.begin(), b.end(),
               std::back_inserter(result));          // {1,2,3,4,5,6}
std::set_difference(a.begin(), a.end(), b.begin(), b.end(),
                    std::back_inserter(result));     // {1, 2}
```

> ⚠️ これらのアルゴリズムは**ソート済みの範囲**を要求します。
> `std::set` はソート済みなのでOKですが、`unordered_set` には使えません。
> ソート済み `std::vector` にも使えます（そちらの方が速い）。

---

## 9.5 std::deque — 両端キュー

```cpp
#include <deque>

std::deque<int> dq;
dq.push_back(1);      // 末尾に追加
dq.push_front(0);     // ★ 先頭に追加（vector にはできない）
dq.pop_front();
dq[0];                // 添字アクセスもできる
```

Pythonの `collections.deque` に相当します。

```
  deque の内部（チャンクのリスト）
  +--------+   +--------+   +--------+
  | 512個  |   | 512個  |   | 512個  |
  +--------+   +--------+   +--------+
       ▲                         ▲
    先頭側に追加             末尾側に追加
```

| | `vector` | `deque` |
|---|---|---|
| 先頭への挿入 | O(n) | **O(1)** |
| 末尾への挿入 | 償却O(1) | O(1) |
| 添字アクセス | **超高速**（1回の計算） | 高速（2回の間接参照） |
| メモリ連続性 | **完全に連続** | チャンク単位で連続 |
| 要素への参照の安定性 | 再確保で無効化 | **push_back/push_front では無効化されない** |

> 💡 **deque の隠れた利点: ポインタの安定性**
>
> ```cpp
> std::deque<Atom> atoms;
> Atom* p = &atoms[0];
> atoms.push_back(new_atom);    // ★ p はまだ有効！（vector なら無効化される）
> ```
>
> グラフ構造で「ノードへのポインタを保持しつつ、ノードを追加していく」
> ような場合に便利です。第21章の分子グラフで検討します。
>
> ただし、添字アクセスは vector より少し遅く、
> メモリ効率も vector に劣ります。デフォルトは vector です。

---

## 9.6 std::list / std::forward_list — 連結リスト

```cpp
#include <list>

std::list<int> lst = {1, 2, 3};
auto it = lst.begin();
++it;
lst.insert(it, 99);       // O(1) で中間挿入
```

| | `list` |
|---|---|
| 中間への挿入/削除 | **O(1)**（イテレータを持っていれば） |
| 添字アクセス | **できない**（`lst[3]` は書けない） |
| メモリ | 要素ごとにポインタ2個のオーバーヘッド |
| キャッシュ効率 | **最悪**（ノードが散らばる） |

> ⚠️ **`std::list` はほぼ使いません。**
>
> 「中間挿入がO(1)だから速い」というのは**教科書上の話**で、
> 現代のCPUでは**vector の方が速いことがほとんど**です。
>
> 要素を1000個持つvectorの中間に挿入する場合:
> - vector: 999要素を memmove（連続メモリ。SIMDで一瞬）
> - list: ノードを辿って挿入位置を探す（毎回キャッシュミス）
>
> 実測すると、要素数が数千程度なら vector が圧勝します。
> Bjarne Stroustrup 自身が講演でこれをデモしています。
>
> `std::list` を使うべきなのは「巨大なオブジェクトを大量に持ち、
> 頻繁に中間から削除し、要素への参照を保持し続ける必要がある」という
> かなり限定的な場合だけです。

---

## 9.7 コンテナアダプタ

### std::stack

```cpp
#include <stack>

std::stack<int> st;
st.push(1);
st.push(2);
std::cout << st.top() << "\n";   // 2
st.pop();                        // 値は返さない
st.empty();
st.size();
```

🧪 SMILESパーサの括弧処理や、グラフのDFSで使います。

```cpp
// 実は vector で代用できる（そちらの方が柔軟）
std::vector<int> st;
st.push_back(1);
int top = st.back();
st.pop_back();
```

### std::queue

```cpp
#include <queue>

std::queue<int> q;
q.push(1);
std::cout << q.front() << "\n";
q.pop();
```

🧪 グラフのBFS（Morganアルゴリズムの半径展開など）で使います。

### std::priority_queue — ヒープ

**類似度検索の Top-K 抽出で必須**です。

```cpp
#include <queue>
#include <vector>

// デフォルトは最大ヒープ（top() が最大値）
std::priority_queue<double> max_heap;
max_heap.push(0.8);
max_heap.push(0.3);
max_heap.push(0.9);
std::cout << max_heap.top() << "\n";   // 0.9

// 最小ヒープにするには比較関数を変える
std::priority_queue<double, std::vector<double>, std::greater<double>> min_heap;
```

🧪 **Top-K 類似化合物の抽出パターン:**

```cpp
struct Hit {
    double      score;
    std::size_t index;
    // priority_queue の比較用。「scoreが小さいものが top に来る」最小ヒープにする
    bool operator>(const Hit& o) const { return score > o.score; }
};

std::priority_queue<Hit, std::vector<Hit>, std::greater<Hit>> topk;

for (std::size_t i = 0; i < n_molecules; ++i) {
    double s = tanimoto(query, db[i]);
    if (topk.size() < K) {
        topk.push({s, i});
    } else if (s > topk.top().score) {
        topk.pop();          // 最小のものを捨てる
        topk.push({s, i});
    }
}
```

**なぜこれが良いのか:** 全スコアを保存してソートすると O(n log n) かつ
メモリ O(n) ですが、この方法は **O(n log K)、メモリ O(K)** です。
1億分子から上位100件を取るなら、圧倒的な差になります。

第23章で使います。

---

## 9.8 std::tuple と std::pair

```cpp
#include <tuple>
#include <utility>

std::pair<std::string, double> p{"CCO", 46.07};
std::cout << p.first << " " << p.second << "\n";

std::tuple<std::string, double, int> t{"CCO", 46.07, 3};
std::cout << std::get<0>(t) << "\n";       // インデックスで取り出す
std::cout << std::get<double>(t) << "\n";  // 型で取り出す（一意なら）

// 構造化束縛（C++17）でPythonのアンパックのように書ける
auto [smiles, mw, atoms] = t;
```

```python
smiles, mw, atoms = ("CCO", 46.07, 3)     # Python
```

### 複数の値を返す

```cpp
// tuple で返す
std::tuple<double, double, int> analyze(const Molecule& m) {
    return {mw, logp, n_rings};
}
auto [mw, logp, rings] = analyze(mol);

// ★ でも、名前付き struct の方が読みやすい
struct AnalysisResult {
    double mw;
    double logp;
    int    n_rings;
};
AnalysisResult analyze(const Molecule& m);

auto r = analyze(mol);
std::cout << r.mw;              // 何の値か明確
```

> 💡 **3つ以上返すなら struct を使ってください。**
> `std::get<2>(t)` が何を意味するか、半年後の自分は覚えていません。
> Pythonの `NamedTuple` / `dataclass` を使うのと同じ理由です。

### tie で既存の変数に代入

```cpp
double mw, logp;
int rings;
std::tie(mw, logp, rings) = analyze(mol);    // 既存の変数に代入
std::tie(mw, std::ignore, rings) = analyze(mol);   // 2番目を無視
```

### tuple の比較（辞書順ソートに便利）

```cpp
// 複数キーでのソートが1行で書ける
std::sort(mols.begin(), mols.end(), [](const auto& a, const auto& b){
    return std::tie(a.n_rings, a.mw) < std::tie(b.n_rings, b.mw);
});
// 環数で昇順、同じなら MW で昇順
```

---

## 9.9 計算量チートシート

| 操作 | vector | deque | list | map | unordered_map |
|---|---|---|---|---|---|
| 添字アクセス | **O(1)** | O(1) | — | — | — |
| キー検索 | O(n) | O(n) | O(n) | O(log n) | **O(1)** 平均 |
| 末尾挿入 | 償却O(1) | O(1) | O(1) | — | — |
| 先頭挿入 | O(n) | **O(1)** | O(1) | — | — |
| 中間挿入 | O(n) | O(n) | **O(1)**※ | O(log n) | O(1) 平均 |
| 削除 | O(n) | O(n) | **O(1)**※ | O(log n) | O(1) 平均 |
| メモリ効率 | **最良** | 良 | 悪 | 悪 | 中 |
| キャッシュ効率 | **最良** | 良 | **最悪** | 悪 | 中 |

※イテレータを既に持っている場合

> ⚠️ **計算量だけで選ばないでください。**
> 定数倍とキャッシュ効率が支配的です。
> 実測すると、n が数千以下なら vector が O(1) のコンテナに勝つことが多いです。
>
> **迷ったら vector。困ったら測る。** これがC++の鉄則です。

---

## 9.10 🧪 実践: 分子データベースのインデックス構築

```cpp
// code/ch09/mol_index.cpp
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct MolRecord {
    std::string id;
    std::string smiles;
    double      mw;
    double      logp;
    int         n_rings;
};

class MolDatabase {
    std::vector<MolRecord> records_;                        // 実データ（連続）
    std::unordered_map<std::string, std::size_t> by_id_;    // ID -> 添字
    std::unordered_map<std::string, std::size_t> by_smiles_;// SMILES -> 添字
    std::map<double, std::vector<std::size_t>>   by_mw_;    // MW でソート済み

public:
    void reserve(std::size_t n) {
        records_.reserve(n);
        by_id_.reserve(n);
        by_smiles_.reserve(n);
    }

    // 追加。重複IDは false を返す
    bool add(MolRecord rec) {
        if (by_id_.contains(rec.id)) return false;

        std::size_t idx = records_.size();
        by_id_.emplace(rec.id, idx);
        by_smiles_.try_emplace(rec.smiles, idx);
        by_mw_[rec.mw].push_back(idx);
        records_.push_back(std::move(rec));
        return true;
    }

    // ID で検索。見つからなければ nullptr
    const MolRecord* find_by_id(const std::string& id) const {
        auto it = by_id_.find(id);
        return it == by_id_.end() ? nullptr : &records_[it->second];
    }

    // MW の範囲検索（map の lower_bound を活用）
    std::vector<const MolRecord*> range_by_mw(double lo, double hi) const {
        std::vector<const MolRecord*> out;
        auto begin = by_mw_.lower_bound(lo);
        auto end   = by_mw_.upper_bound(hi);
        for (auto it = begin; it != end; ++it) {
            for (std::size_t idx : it->second) out.push_back(&records_[idx]);
        }
        return out;
    }

    // 重複SMILES（同じ構造の別ID）を検出
    std::vector<std::string> duplicate_smiles() const {
        std::unordered_map<std::string, int> counts;
        counts.reserve(records_.size());
        for (const auto& r : records_) ++counts[r.smiles];

        std::vector<std::string> dups;
        for (const auto& [smi, n] : counts) {
            if (n > 1) dups.push_back(smi);
        }
        std::sort(dups.begin(), dups.end());
        return dups;
    }

    // logP の上位K件（priority_queue で O(n log K)）
    std::vector<const MolRecord*> top_k_logp(std::size_t k) const {
        struct Entry {
            double      logp;
            std::size_t idx;
            bool operator>(const Entry& o) const { return logp > o.logp; }
        };
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> heap;

        for (std::size_t i = 0; i < records_.size(); ++i) {
            Entry e{records_[i].logp, i};
            if (heap.size() < k)            heap.push(e);
            else if (e.logp > heap.top().logp) { heap.pop(); heap.push(e); }
        }

        std::vector<const MolRecord*> out;
        out.reserve(heap.size());
        while (!heap.empty()) { out.push_back(&records_[heap.top().idx]); heap.pop(); }
        std::reverse(out.begin(), out.end());   // 降順にする
        return out;
    }

    std::size_t size() const { return records_.size(); }
};

int main() {
    MolDatabase db;
    db.reserve(8);

    db.add({"CHEMBL25",  "CC(=O)Oc1ccccc1C(=O)O",      180.16, 1.19, 1});
    db.add({"CHEMBL113", "Cn1cnc2c1c(=O)n(C)c(=O)n2C", 194.19, -1.03, 2});
    db.add({"CHEMBL521", "CC(C)Cc1ccc(cc1)C(C)C(=O)O", 206.28, 3.97, 1});
    db.add({"CHEMBL545", "CC(=O)Nc1ccc(O)cc1",         151.16, 0.46, 1});
    db.add({"DUP001",    "CC(=O)Oc1ccccc1C(=O)O",      180.16, 1.19, 1});  // 重複構造
    db.add({"CHEMBL16",  "CCO",                         46.07, -0.31, 0});

    std::cout << "records: " << db.size() << "\n\n";

    // ID 検索
    if (const auto* r = db.find_by_id("CHEMBL113")) {
        std::cout << "CHEMBL113 = " << r->smiles << " (MW " << r->mw << ")\n\n";
    }

    // MW 範囲検索
    std::cout << "MW 150-200:\n";
    for (const auto* r : db.range_by_mw(150.0, 200.0)) {
        std::cout << "  " << r->id << "  " << r->mw << "\n";
    }
    std::cout << "\n";

    // 重複構造
    std::cout << "duplicate structures:\n";
    for (const auto& s : db.duplicate_smiles()) std::cout << "  " << s << "\n";
    std::cout << "\n";

    // logP 上位3件
    std::cout << "top 3 by logP:\n";
    for (const auto* r : db.top_k_logp(3)) {
        std::cout << "  " << r->id << "  logP=" << r->logp << "\n";
    }
    return 0;
}
```

**設計のポイント:**

- **実データは `std::vector` に連続配置**（キャッシュ効率）
- インデックスは**添字（`std::size_t`）を持つ**。ポインタではない
  → `records_` が再確保されてもインデックスは無効にならない
- ID/SMILES 検索は `unordered_map`（O(1)）
- MW の範囲検索は `map`（順序が必要）
- Top-K は `priority_queue`（O(n log K)、メモリ O(K)）

> ⚠️ **ポインタではなく添字を持つ**のは重要な設計判断です。
> ```cpp
> std::unordered_map<std::string, MolRecord*> by_id_;   // ✗ 危険
> std::unordered_map<std::string, std::size_t> by_id_;  // ✓ 安全
> ```
> `records_.push_back()` で再確保が起きると、前者のポインタは全部無効になります。
> 添字なら無効になりません。

---

## 9.11 この章のまとめ

- **`unordered_map` が Python の dict**。順序が要らないなら常にこれ
- **`d[k]` はキーが無ければ挿入する**（Pythonと逆）。読むだけなら `find` / `contains`
- `map` は木構造でキー順。**範囲検索ができる**のが唯一無二の強み
- `set` / `unordered_set` は Python の set
- `deque` は先頭挿入O(1) + **参照が安定**
- **`list` はほぼ使わない**（キャッシュ効率が最悪）
- `priority_queue` は **Top-K 抽出の必須道具**（O(n log K)）
- `tuple`/`pair` は構造化束縛でPython風にアンパックできる。3つ以上なら struct
- **計算量よりキャッシュ効率が支配的なことが多い。迷ったら vector、困ったら測る**
- インデックスは**ポインタではなく添字**で持つ

> 📝 **練習問題 9-1**
>
> SMILES文字列のリストから、各元素記号の出現回数をカウントする関数
> `std::unordered_map<std::string,int> count_elements(const std::vector<std::string>&)`
> を書いてください。

> 📝 **練習問題 9-2**
>
> `std::map<std::string,int>` と `std::unordered_map<std::string,int>` に
> 100万個のランダムな文字列キーを挿入し、検索する時間を比較してください。
> どれくらい差がありますか?

> 📝 **練習問題 9-3**
>
> 9.10 の `MolDatabase` に、
> 「環数が N 以上の分子を全部返す」メソッドを追加してください。
> どのコンテナを使うのが適切ですか? （ヒント: 環数は0〜10程度の小さい整数）

> 📝 **練習問題 9-4**
>
> Top-K を「全部ソートしてから上位K件を取る」実装と
> `priority_queue` 版で、100万要素・K=100 の場合で比較してください。

---

→ [第10章 イテレータとアルゴリズム](ch10_イテレータとアルゴリズム.md)
