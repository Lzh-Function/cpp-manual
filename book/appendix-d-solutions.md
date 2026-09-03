# 付録D 練習問題の解答と、次の一歩

## D.1 練習問題の解答・解説

すべての問題ではなく、**特に重要なもの・間違えやすいもの**を解説します。

---

### 4-1: 整数除算

```cpp
int a = 5, b = 2;
std::cout << a / b << "\n";                      // 2
std::cout << a % b << "\n";                      // 1
std::cout << 1.0 * a / b << "\n";                // 2.5
std::cout << static_cast<double>(a / b) << "\n"; // 2   ★
```

**最後が罠です。** `a / b` が先に評価されて `2`（int）になり、
その後 double に変換されるので `2` のままです。
キャストは**割り算の前に**適用しなければなりません。

### 4-2: 整数オーバーフロー

```cpp
int x = std::numeric_limits<int>::max();   // 2147483647
std::cout << x + 1;                        // -2147483648（多くの環境で）
```

これは**符号付き整数のオーバーフロー = 未定義動作**です。
「ラップアラウンドする」と思い込むのは危険で、
コンパイラは「オーバーフローは起きない」と仮定して最適化します。

```cpp
if (x + 1 < x) { /* オーバーフロー検出 */ }   // ★ 最適化で消される可能性がある
```

正しい検出:

```cpp
if (x > std::numeric_limits<int>::max() - 1) { /* オーバーフローする */ }
// または C++20
#include <numeric>
// GCC/Clang の組み込み関数
if (__builtin_add_overflow(x, 1, &result)) { /* オーバーフロー */ }
```

---

### 5-2: 関数シグネチャの改善

```cpp
// Before
std::vector<double> normalize(std::vector<double> data, double mean, double sd);
bool contains(std::vector<std::string> names, std::string target);
void scale_coords(std::vector<double> coords, double factor);

// After
std::vector<double> normalize(std::vector<double> data, double mean, double sd);
// ★ これは実は正しい。中で加工して返すので、値渡し + ムーブが最適

bool contains(const std::vector<std::string>& names, std::string_view target);
// ★ 読むだけなので const& と string_view

void scale_coords(std::vector<double>& coords, double factor);
// ★ バグ: 値渡しなので呼び出し元に反映されない → 参照にする
```

3番目が典型的なバグです。コンパイルは通り、警告も出ず、
**ただ何も起きない**という最悪のパターンです。

---

### 6-2: ダングリング参照

```cpp
const std::vector<int>& get_squares(int n) {
    std::vector<int> v;                // ★ ローカル変数
    for (int i = 0; i < n; ++i) v.push_back(i * i);
    return v;                          // ★ 関数を抜けたら v は消える
}
```

**修正:**

```cpp
std::vector<int> get_squares(int n) {   // ★ 値で返す
    std::vector<int> v;
    v.reserve(n);
    for (int i = 0; i < n; ++i) v.push_back(i * i);
    return v;                           // NRVO でコピーは起きない
}
```

**「値で返すと遅い」は誤解です**（第12章）。

---

### 7-3: イテレータの無効化

```cpp
std::vector<int> v = {1,2,3,4,5};
for (auto it = v.begin(); it != v.end(); ++it) {
    if (*it % 2 == 0) v.erase(it);      // ★ erase 後の it は無効
}
```

`erase` は削除位置以降のイテレータを無効化します。
さらに `++it` でスキップも起きます。

**修正:**

```cpp
// 方法1: erase の戻り値を使う
for (auto it = v.begin(); it != v.end(); ) {
    if (*it % 2 == 0) it = v.erase(it);    // erase は次のイテレータを返す
    else              ++it;
}

// 方法2: C++20（推奨）
std::erase_if(v, [](int x){ return x % 2 == 0; });

// 方法3: erase-remove イディオム（C++17以前）
v.erase(std::remove_if(v.begin(), v.end(),
                       [](int x){ return x % 2 == 0; }),
        v.end());
```

**方法2/3の方が速い**です（要素の移動が1回で済むため）。

---

### 8-4: string_view のダングリング

```cpp
std::vector<std::string_view> get_tokens() {
    std::string s = read_smiles_from_file();   // ★ ローカル変数
    return tokenize(s);                        // ★ s を指す view を返す
}                                              // ★ s が破棄される
```

**返された `string_view` は全部ダングリング**です。

**修正:**

```cpp
// 方法1: 文字列も一緒に返す
struct TokenizedSmiles {
    std::string                    source;   // ★ 所有する
    std::vector<std::string_view>  tokens;   // source を指す
};

// ⚠️ ただし、これも危険: source がムーブされると tokens が無効になる
//    （SSOの範囲内なら特に。ヒープ上なら偶然動くこともある）

// 方法2: 所有する文字列にする（安全）
std::vector<std::string> get_tokens() {
    std::string s = read_smiles_from_file();
    std::vector<std::string> out;
    for (auto sv : tokenize(s)) out.emplace_back(sv);
    return out;
}

// 方法3: 呼び出し側が文字列を保持する（最速）
void process(std::string_view smiles) {
    auto tokens = tokenize(smiles);    // smiles は呼び出し元が保持
    // ...
}
```

**方法3が最も良い設計**です。所有権を呼び出し側に置きます。

---

### 10-2: accumulate の初期値

```cpp
std::vector<double> scores = {0.9, 0.7, 0.85};
double total = std::accumulate(scores.begin(), scores.end(), 0);   // ★ 0 は int
```

`accumulate` の型は**初期値の型**で決まります。
`0`（int）を渡すと、内部で `int + double → int` の変換が繰り返され、
結果は `2`（0.9→0、+0.7→0、+0.85→0... 実際には切り捨てで 2）になります。

**修正: `0.0` と書く。**

```cpp
double total = std::accumulate(scores.begin(), scores.end(), 0.0);   // 2.45
```

---

### 12-2: 空のデストラクタの罠

```cpp
class Descriptor {
    std::vector<double> values_;
public:
    Descriptor(const std::vector<double>& v) : values_(v) {}
    ~Descriptor() {}                    // ★ 問題
};
```

**問題:**
1. `~Descriptor() {}` を書いたせいで、**ムーブコンストラクタが自動生成されない**
   → `std::vector<Descriptor>` の再確保が全部コピーになる
2. `explicit` がないので暗黙変換が起きる
3. 引数が `const&` なので、常にコピーが発生する

**修正:**

```cpp
class Descriptor {
    std::vector<double> values_;
public:
    explicit Descriptor(std::vector<double> v) : values_(std::move(v)) {}
    // ★ デストラクタは書かない（Rule of Zero）
};
```

---

### 14-1: 仮想デストラクタ忘れ

```cpp
class Shape { public: ~Shape() {} virtual double area() const = 0; };
class Circle : public Shape {
    std::vector<double> points_;
public:
    double area() const override { return 3.14; }
};
std::vector<std::unique_ptr<Shape>> shapes;
shapes.push_back(std::make_unique<Circle>());
```

**`unique_ptr<Shape>` が破棄されるとき `~Circle()` が呼ばれず、
`points_` がリーク**します。規格上は未定義動作です。

**修正:**

```cpp
class Shape {
public:
    virtual ~Shape() = default;         // ★ virtual を付ける
    virtual double area() const = 0;
};
```

`-Wnon-virtual-dtor` を付けていれば警告されます。

---

### 16-1: ラムダのキャプチャの罠

```cpp
std::vector<std::function<int()>> fs;
for (int i = 0; i < 3; ++i) {
    fs.push_back([&i]() { return i; });    // ★ i を参照キャプチャ
}
for (auto& f : fs) std::cout << f() << " ";
```

**出力: 未定義動作。** ループ変数 `i` はループ終了時に破棄されるので、
すべてのラムダがダングリング参照を持ちます。

**修正:**

```cpp
fs.push_back([i]() { return i; });      // ★ コピーキャプチャ
// 出力: 0 1 2
```

> 🐍 **Pythonでも似た罠があります:**
> ```python
> fs = [lambda: i for i in range(3)]
> [f() for f in fs]        # [2, 2, 2]  ← 全部同じ
> fs = [lambda i=i: i for i in range(3)]
> [f() for f in fs]        # [0, 1, 2]  ← デフォルト引数で束縛
> ```
> C++の `[i]` は、Pythonの `i=i` に相当します。

---

### 25-2: OpenMP のデータ競合

```cpp
std::vector<double> results;
#pragma omp parallel for
for (int i = 0; i < n; ++i) {
    results.push_back(compute(i));      // ★ データ競合
}
```

`push_back` は `size_` を更新するので、複数スレッドから呼ぶと壊れます。

**修正:**

```cpp
// 方法1: 事前にサイズを確保して、添字で書く（★推奨）
std::vector<double> results(n);
#pragma omp parallel for
for (int i = 0; i < n; ++i) {
    results[i] = compute(i);            // ✓ 各スレッドが別の要素に書く
}

// 方法2: スレッドごとに集めて統合
std::vector<std::vector<double>> partial(omp_get_max_threads());
#pragma omp parallel
{
    auto& local = partial[omp_get_thread_num()];
    #pragma omp for
    for (int i = 0; i < n; ++i) local.push_back(compute(i));
}
for (const auto& p : partial)
    results.insert(results.end(), p.begin(), p.end());
```

**方法1が圧倒的に速く、シンプル**です。

---

## D.2 次に読むべきもの

### 書籍

| タイトル | 対象 | 一言 |
|---|---|---|
| **A Tour of C++** (Bjarne Stroustrup) | 初〜中級 | 設計者による200ページの概説。**次に読むならこれ** |
| **Effective Modern C++** (Scott Meyers) | 中級 | C++11/14の要点42項目。ムーブとテンプレートの理解が深まる |
| **C++ Core Guidelines** (Web) | 全般 | 「こう書け」の公式指針。無料 |
| **C++ Concurrency in Action** (Anthony Williams) | 中〜上級 | 並列処理の決定版 |
| **Optimized C++** (Kurt Guntheroth) | 中級 | 実践的な最適化 |
| **Computer Systems: A Programmer's Perspective** | 全般 | **CPUとメモリの理解に最良**（C++の本ではないが必読級） |
| **Modern CMake** (Web, Henry Schreiner) | 全般 | CMakeの正しい書き方。無料 |
| **C++ Templates: The Complete Guide** | 上級 | テンプレートを本気でやるなら |

### Webリソース

| サイト | 用途 |
|---|---|
| **[cppreference.com](https://en.cppreference.com/)** | **標準ライブラリのリファレンス。最重要** |
| [Compiler Explorer](https://godbolt.org/) | 生成コードの確認 |
| [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/) | 公式のベストプラクティス |
| [cppinsights.io](https://cppinsights.io/) | テンプレート展開やautoの実体が見える |
| [quick-bench.com](https://quick-bench.com/) | ブラウザ上でベンチマーク |
| [Awesome C++](https://github.com/fffaraz/awesome-cpp) | ライブラリカタログ |
| [C++ Weekly (YouTube)](https://www.youtube.com/@cppweekly) | Jason Turner の週刊ビデオ |
| [CppCon (YouTube)](https://www.youtube.com/@CppCon) | カンファレンス講演 |

> 💡 **cppreference.com の使い方**
> 関数名で検索すると、シグネチャ、計算量、例外保証、使用例、
> どのC++バージョンから使えるかが全部載っています。
> **迷ったらここを見る**習慣をつけてください。

### 押さえておくべきCppCon講演

- **"Back to Basics"** シリーズ — 各トピックの入門（毎年更新）
- Chandler Carruth, "Efficiency with Algorithms, Performance with Data Structures"
  — **キャッシュとデータ構造の話。必見**
- Bjarne Stroustrup, "Writing Good C++14"
- Herb Sutter, "Leak-Freedom in C++"
- Andrei Alexandrescu, "Speed Is Found In The Minds of People"

---

## D.3 ケモインフォマティクスのC++資産

| ライブラリ | 用途 | 備考 |
|---|---|---|
| **[RDKit](https://github.com/rdkit/rdkit)** | 総合 | デファクトスタンダード（第27章） |
| [Open Babel](https://github.com/openbabel/openbabel) | フォーマット変換 | 100以上の形式に対応 |
| [OpenMM](https://openmm.org/) | 分子動力学 | GPU対応、Pythonバインディング充実 |
| [xtb](https://github.com/grimme-lab/xtb) | 半経験的量子化学 | 高速な構造最適化 |
| [LAMMPS](https://www.lammps.org/) | 分子動力学 | 大規模系 |
| [Chemfiles](https://github.com/chemfiles/chemfiles) | トラジェクトリI/O | 軽量 |
| [Indigo](https://github.com/epam/Indigo) | 化学構造処理 | RDKitの代替 |
| [libmolgrid](https://github.com/gnina/libmolgrid) | 3D格子生成 | 深層学習用 |
| [nauty](https://pallini.di.uniroma1.it/) | グラフ同型 | 正準ラベリング |
| [Eigen](https://eigen.tuxfamily.org/) | 線形代数 | NumPy相当 |
| [xtensor](https://github.com/xtensor-stack/xtensor) | 多次元配列 | NumPy風API |
| [ONNX Runtime](https://onnxruntime.ai/) | ML推論 | 学習済みモデルをC++から |
| [LibTorch](https://pytorch.org/cppdocs/) | PyTorch C++版 | 学習も推論も |

### 深層学習モデルをC++から使う

学習は Python、推論は C++ という構成が実用的です。

```python
# Python: モデルを ONNX にエクスポート
import torch
torch.onnx.export(model, dummy_input, "model.onnx")
```

```cpp
// C++: ONNX Runtime で推論
#include <onnxruntime_cxx_api.h>

Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "chemcpp");
Ort::Session session(env, "model.onnx", Ort::SessionOptions{});

std::vector<float> input(2048);       // フィンガープリント
std::vector<int64_t> shape{1, 2048};

auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
auto input_tensor = Ort::Value::CreateTensor<float>(
    memory_info, input.data(), input.size(), shape.data(), shape.size());

const char* input_names[]  = {"input"};
const char* output_names[] = {"output"};
auto output = session.Run(Ort::RunOptions{nullptr},
                          input_names, &input_tensor, 1, output_names, 1);
float* pred = output[0].GetTensorMutableData<float>();
```

**フィンガープリント計算 → 推論 → 後処理 を全部C++で完結**させれば、
Python層のオーバーヘッドがゼロになります。
大規模バーチャルスクリーニングでは大きな差になります。

---

## D.4 学習ロードマップ（この本の後）

### レベル1: 定着させる（1〜2ヶ月）

```
□ 自分の研究コードのホットスポットを1つC++に移植する
□ pybind11 で Python から呼べるようにする
□ テストを書く（Catch2）
□ ASan/UBSan で動かす
□ CI を設定する
□ プロファイルを取って最適化する
```

**「1つ完成させる」ことが何より重要**です。
チュートリアルを100個読むより、動くものを1つ作る方が身につきます。

### レベル2: 広げる（3〜6ヶ月）

```
□ A Tour of C++ を読む
□ RDKit の C++ API を使う
□ 並列化を本格的に実装する
□ Eigen で線形代数を扱う
□ Compiler Explorer で最適化を確認する習慣をつける
□ C++ Core Guidelines を読む
```

### レベル3: 深める（6ヶ月〜）

```
□ Effective Modern C++ を読む
□ SIMD を書いてみる（xsimd / Highway）
□ RDKit のソースコードを読む
□ OSS にコントリビュートする
□ CppCon の講演を見る
□ C++20 の coroutine / modules を試す
```

---

## D.5 実務で最初に作るべきもの

具体的なプロジェクト案を、難易度順に:

### ★ 初級

1. **記述子計算CLI**（第24章）
   SMILESファイルを読んでCSVを出力。ファイルI/Oとパースの練習。

2. **重複除去ツール**
   InChIKey or 正準SMILESで巨大なライブラリの重複を除く。
   `unordered_set` の練習。

3. **フィルタリングツール**
   Ro5、PAINS、構造アラートで化合物を絞り込む。

### ★★ 中級

4. **類似度検索エンジン**（第23章）
   フィンガープリントDBを構築し、高速検索。
   バイナリ形式での保存、mmap、枝刈り。

5. **Python 拡張モジュール**（第26章）
   上記をpybind11でPythonから使えるようにする。

6. **並列SDFプロセッサ**
   巨大なSDFをマルチスレッドで処理し、記述子を計算。

### ★★★ 上級

7. **クラスタリングツール**
   Taylor-Butina や k-medoids で化合物をクラスタリング。
   距離行列の計算が O(n²) なので、最適化の腕の見せどころ。

8. **部分構造検索エンジン**
   フィンガープリントでの事前フィルタ + VF2 アルゴリズム。

9. **カスタムフィンガープリント**
   自分の研究に最適化した記述子/FPを設計・実装・評価。

10. **推論パイプライン**
    SMILES → FP → ONNX推論 → ランキング を全部C++で。

---

## D.6 最後に

### この本で伝えたかったこと

1. **C++は難しくない。難しい部分は使わなくていい。**
   モダンC++（vector, string, unique_ptr, RAII）だけで、
   実務の9割はカバーできます。

2. **速さの正体はメモリレイアウトとアルゴリズム。**
   小手先の最適化より、データがメモリのどこにあるかが重要です。

3. **全部C++で書く必要はない。**
   Pythonで書き、遅い部分だけをC++に落として `import` する。
   これが最も生産的です。

4. **測ってから最適化する。**
   直感は外れます。プロファイラを信じてください。

5. **ツールに頼る。**
   サニタイザ、静的解析、テスト、CI。
   C++の危険性は、ツールでほぼ潰せます。

### 覚えておいてほしい3つのルール（再掲）

> **ルール1: `new` と `delete` は自分で書かない**
> **ルール2: 生ポインタは「所有しない参照」としてだけ使う**
> **ルール3: 型に嘘をつかない**

この3つを守れば、C++は安全で楽しい言語です。

### あなたの研究のために

ケモインフォマティクスは、**計算速度が研究の可能性を直接広げる**分野です。

- 1万化合物しか試せなかったスクリーニングが、100万化合物になる
- 1日かかっていた解析が、1分になる
- 「重いから諦めた」アイデアが、試せるようになる

C++は、その扉を開ける鍵のひとつです。

そして何より——**自分のコードが100倍速くなる瞬間は、単純に楽しい**です。

Happy compiling. そして、良い研究を。

---

## D.7 チートシート（1ページまとめ）

```cpp
// ============ 基本 ============
#include <vector>
#include <string>
int main() { return 0; }

// ============ 型 ============
int, long long, std::int64_t     // 整数
double                            // Python の float
std::string, std::string_view     // 文字列
std::vector<T>                    // list
std::unordered_map<K,V>           // dict
std::unordered_set<T>             // set
std::optional<T>                  // T | None
std::array<T,N>                   // 固定長配列

// ============ 引数 ============
void f(int x);                    // 軽い型 → 値
void f(const std::string& s);     // 重い型、読む → const&
void f(std::string_view s);       // 文字列、読む → view
void f(std::vector<int>& v);      // 書き換える → &
void f(std::string s);            // 保存する → 値 + move

// ============ ループ ============
for (const auto& x : v) { }       // 読む（★最頻出）
for (auto& x : v) { }             // 書き換える
for (std::size_t i = 0; i < v.size(); ++i) { }

// ============ RAII ============
{
    std::ifstream f(path);
    std::lock_guard lock(mtx);
    auto p = std::make_unique<T>();
}   // すべて自動解放

// ============ 高速化 ============
v.reserve(n);                     // 再確保を防ぐ
const auto&                       // コピーを防ぐ
std::vector<T> flat(rows*cols);   // 連続メモリ
std::popcount(x)                  // ハードウェア命令
if (bound < threshold) continue;  // 枝刈り

// ============ ビルド ============
// 開発: -g -O0 -Wall -Wextra -fsanitize=address,undefined
// 本番: -O2 -DNDEBUG -Wall -Wextra
// 最速: -O3 -march=native -DNDEBUG -flto

// ============ エラー処理 ============
assert(i < n);                    // バグ（Debugのみ）
throw std::runtime_error("...");  // 想定外
std::optional<T> f();             // 想定内の失敗
void f() noexcept;                // 絶対に失敗しない
```

---

**完**

［[目次に戻る](README.md)］
