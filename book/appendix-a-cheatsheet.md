# 付録A Python → C++ 完全チートシート

手元に置いて引くための対応表です。

---

## A.1 基本構文

| Python | C++ |
|---|---|
| `# comment` | `// comment` |
| `""" doc """` | `/* comment */` |
| （インデント） | `{ }` |
| （改行） | `;` |
| `x = 1` | `int x = 1;` |
| `x: int = 1` | `int x = 1;` |
| `if x: ...` | `if (x) { ... }` |
| `elif` | `else if` |
| `and` / `or` / `not` | `&&` / `\|\|` / `!` |
| `x if c else y` | `c ? x : y` |
| `0 < x < 10` | `0 < x && x < 10` |
| `pass` | `;` または `{}` |
| `None` | `nullptr` / `std::nullopt` |
| `True` / `False` | `true` / `false` |
| `is` | `==`（ポインタ比較なら `p == q`） |
| `x ** 2` | `x * x` または `std::pow(x, 2)` |
| `7 // 2` | `7 / 2`（int同士なら自動的に整数除算） |
| `7 / 2` | `7.0 / 2`（片方をdoubleに） |
| `x += 1` | `x += 1` / `++x` |
| `del x` | （スコープ管理に任せる） |
| `global x` | （グローバル変数を直接使う） |
| `assert x` | `assert(x);` |
| `raise E("m")` | `throw E("m");` |
| `try/except/finally` | `try/catch`（finally は RAII） |
| `with open(f) as x:` | `std::ifstream x(f);`（RAII） |
| `lambda x: x*2` | `[](auto x) { return x*2; }` |
| `f-string` | `std::format("{}", x)` |
| `print(x)` | `std::cout << x << "\n";` |
| `input()` | `std::getline(std::cin, s);` |

---

## A.2 型

| Python | C++ | 備考 |
|---|---|---|
| `int` | `int` / `long long` / `std::int64_t` | Pythonは無限精度 |
| `float` | **`double`** | C++の `float` は32bit |
| `bool` | `bool` | |
| `str` | `std::string` | |
| `bytes` | `std::vector<std::uint8_t>` | |
| `list[T]` | `std::vector<T>` | |
| `tuple[A,B]` | `std::pair<A,B>` / `std::tuple<...>` | |
| `dict[K,V]` | `std::unordered_map<K,V>` | 順序なし |
| `dict`（順序付き） | `std::map<K,V>` | キー順ソート |
| `set[T]` | `std::unordered_set<T>` | |
| `frozenset` | `const std::unordered_set<T>` | |
| `T \| None` | `std::optional<T>` | |
| `Union[A,B]` | `std::variant<A,B>` | |
| `Callable` | `std::function<R(Args...)>` / テンプレート | |
| `Any` | （避ける。`std::any` はあるが非推奨） | |
| `np.ndarray` | `std::vector<T>` + shape / `std::span` | |
| `np.int32` | `std::int32_t` | |
| `np.float32` | `float` | |
| `np.uint8` | `std::uint8_t` | |
| `memoryview` | `std::span<T>` | |
| `type(x)` | `typeid(x)` / `decltype(x)` | |
| `isinstance(x, T)` | `dynamic_cast<T*>(&x) != nullptr` | 継承の場合 |

---

## A.3 リスト操作

| Python | C++ |
|---|---|
| `v = []` | `std::vector<int> v;` |
| `v = [1,2,3]` | `std::vector<int> v = {1,2,3};` |
| `v = [0]*n` | `std::vector<int> v(n);` |
| `v = [x]*n` | `std::vector<int> v(n, x);` |
| `len(v)` | `v.size()` |
| `not v` | `v.empty()` |
| `v.append(x)` | `v.push_back(x)` / `v.emplace_back(x)` |
| `v.pop()` | `x = v.back(); v.pop_back();` |
| `v.insert(i, x)` | `v.insert(v.begin()+i, x)` |
| `del v[i]` | `v.erase(v.begin()+i)` |
| `v.clear()` | `v.clear()` |
| `v[0]` | `v.front()` / `v[0]` |
| `v[-1]` | `v.back()` |
| `v[i]` | `v[i]`（チェックなし） / `v.at(i)`（チェックあり） |
| `v[2:5]` | `std::vector(v.begin()+2, v.begin()+5)` |
| `v[::-1]` | `std::reverse(v.begin(), v.end())` |
| `v1 + v2` | `v1.insert(v1.end(), v2.begin(), v2.end())` |
| `x in v` | `std::ranges::find(v,x) != v.end()` |
| `v.index(x)` | `std::ranges::find(v,x) - v.begin()` |
| `v.count(x)` | `std::ranges::count(v, x)` |
| `v.sort()` | `std::ranges::sort(v)` |
| `v.sort(key=f)` | `std::ranges::sort(v, {}, f)` |
| `v.sort(reverse=True)` | `std::ranges::sort(v, std::greater{})` |
| `sorted(v)` | `auto w = v; std::ranges::sort(w);` |
| `v.reverse()` | `std::ranges::reverse(v)` |
| `sum(v)` | `std::accumulate(v.begin(), v.end(), 0.0)` |
| `max(v)` | `*std::ranges::max_element(v)` |
| `min(v)` | `*std::ranges::min_element(v)` |
| `any(v)` | `std::ranges::any_of(v, pred)` |
| `all(v)` | `std::ranges::all_of(v, pred)` |
| `list(set(v))` | `sort` + `unique` + `erase` |
| `[f(x) for x in v]` | `std::ranges::transform` / `views::transform` |
| `[x for x in v if p(x)]` | `std::copy_if` / `views::filter` |
| `enumerate(v)` | 添字ループ / `views::enumerate`(C++23) |
| `zip(a,b)` | 添字ループ / `views::zip`(C++23) |
| `range(n)` | `for (int i=0;i<n;++i)` / `views::iota(0,n)` |
| `reversed(v)` | `views::reverse` |
| `list(range(n))` | `std::iota(v.begin(), v.end(), 0)` |
| `v.extend(w)` | `v.insert(v.end(), w.begin(), w.end())` |
| `random.shuffle(v)` | `std::shuffle(v.begin(), v.end(), rng)` |
| `random.choice(v)` | `v[dist(rng)]` |
| `heapq.nlargest(k,v)` | `std::partial_sort` / `std::priority_queue` |
| `bisect.bisect_left` | `std::lower_bound` |
| `itertools.accumulate` | `std::partial_sum` |
| `statistics.median(v)` | `std::nth_element(b, b+n/2, e)` |

---

## A.4 辞書・集合

| Python | C++ |
|---|---|
| `d = {}` | `std::unordered_map<K,V> d;` |
| `d[k] = v` | `d[k] = v;` |
| `d[k]`（読む） | `d.at(k)`（無ければ例外） |
| `d.get(k, default)` | `d.contains(k) ? d.at(k) : default` |
| `k in d` | `d.contains(k)` (C++20) / `d.count(k)` |
| `del d[k]` | `d.erase(k)` |
| `len(d)` | `d.size()` |
| `d.keys()` | ループで `[k, _] : d` |
| `d.values()` | ループで `[_, v] : d` |
| `d.items()` | `for (const auto& [k,v] : d)` |
| `d.setdefault(k,v)` | `d.try_emplace(k, v)` |
| `d.update(e)` | `d.merge(e)` / ループで insert |
| `d.pop(k)` | `auto v = d.at(k); d.erase(k);` |
| `collections.Counter` | `std::unordered_map<T,int>` + `++m[x]` |
| `defaultdict(list)` | `std::unordered_map<K, std::vector<V>>`（`[]` が自動作成） |
| `s = set()` | `std::unordered_set<T> s;` |
| `s.add(x)` | `s.insert(x)` |
| `x in s` | `s.contains(x)` |
| `s.remove(x)` | `s.erase(x)` |
| `a \| b` | `std::set_union` |
| `a & b` | `std::set_intersection` |
| `a - b` | `std::set_difference` |

⚠️ **`d[k]` は、キーが無ければ挿入します**（Pythonと逆）。読むだけなら `find` / `at`。

---

## A.5 文字列

| Python | C++ |
|---|---|
| `s = "abc"` | `std::string s = "abc";` |
| `len(s)` | `s.size()` |
| `s + t` | `s + t` |
| `s += t` | `s += t` |
| `s * 3` | ループ / `std::string(3, 'c')`（1文字なら） |
| `s[i]` | `s[i]`（`char` が返る） |
| `s[2:5]` | `s.substr(2, 3)`（開始, **長さ**） |
| `s.find("x")` | `s.find("x")`（無ければ `npos`） |
| `"x" in s` | `s.find("x") != std::string::npos` |
| `s.startswith("x")` | `s.starts_with("x")` (C++20) |
| `s.endswith("x")` | `s.ends_with("x")` (C++20) |
| `s.split(",")` | 自作（第8章） |
| `",".join(v)` | 自作（第8章） |
| `s.strip()` | 自作（第8章） |
| `s.replace(a,b)` | 自作（第8章） |
| `s.upper()` | `std::transform` + `std::toupper` |
| `s.lower()` | `std::transform` + `std::tolower` |
| `int(s)` | `std::stoi(s)` / `std::from_chars` |
| `float(s)` | `std::stod(s)` / `std::from_chars` |
| `str(n)` | `std::to_string(n)` |
| `f"{x:.2f}"` | `std::format("{:.2f}", x)` |
| `"".join(reversed(s))` | `std::reverse(s.begin(), s.end())` |
| `s.count("x")` | `std::ranges::count(s, 'x')` |
| `s.encode()` | （`std::string` は既にバイト列） |
| `s == t` | `s == t` ✓ |
| `s < t` | `s < t` ✓ |

---

## A.6 関数・クラス

| Python | C++ |
|---|---|
| `def f(x): return x` | `int f(int x) { return x; }` |
| `def f(x) -> int:` | `int f(...)` |
| `def f(x=1):` | `void f(int x = 1);` |
| `def f(*args):` | `template <typename... Args> void f(Args&&...)` |
| `def f(**kwargs):` | （なし。構造体で代用） |
| `f(x=1)`（キーワード） | （なし。構造体 + 指示付き初期化） |
| `lambda x: x*2` | `[](int x){ return x*2; }` |
| `functools.partial` | ラムダでキャプチャ |
| `yield` | （なし。コルーチンかイテレータ） |
| `class C:` | `class C { };` |
| `def __init__` | コンストラクタ `C(...)` |
| `def __del__` | デストラクタ `~C()` |
| `def m(self)` | `void m()` |
| `def m(self)`（読むだけ） | `void m() const` |
| `@staticmethod` | `static void m()` |
| `@property` | getter メソッド |
| `_private` | `private:` |
| `__str__` | `operator<<` |
| `__repr__` | `operator<<` / `to_string()` |
| `__eq__` | `operator==` |
| `__lt__` | `operator<` / `operator<=>` |
| `__len__` | `size()` |
| `__getitem__` | `operator[]` |
| `__call__` | `operator()` |
| `__enter__/__exit__` | コンストラクタ/デストラクタ（RAII） |
| `__hash__` | `std::hash<T>` の特殊化 |
| `super().__init__()` | `Base::Base()` / 初期化リスト |
| `@abstractmethod` | `virtual void m() = 0;` |
| `@dataclass` | `struct` + デフォルトメンバ初期化子 |
| `class C(A, B)` | `class C : public A, public B` |
| `isinstance(x, T)` | `dynamic_cast<T*>(&x)` |

---

## A.7 ファイル I/O

| Python | C++ |
|---|---|
| `open(p)` | `std::ifstream in(p);` |
| `open(p, "w")` | `std::ofstream out(p);` |
| `open(p, "rb")` | `std::ifstream in(p, std::ios::binary);` |
| `with open(p) as f:` | `{ std::ifstream f(p); ... }`（RAII） |
| `f.read()` | 第24章の `read_file()` |
| `f.readline()` | `std::getline(f, line)` |
| `for line in f:` | `while (std::getline(f, line))` |
| `f.readlines()` | ループで vector に集める |
| `f.write(s)` | `out << s;` |
| `print(x, file=f)` | `out << x << "\n";` |
| `f.close()` | （RAIIで自動） |
| `os.path.exists(p)` | `std::filesystem::exists(p)` |
| `os.listdir(d)` | `std::filesystem::directory_iterator(d)` |
| `os.makedirs(d)` | `std::filesystem::create_directories(d)` |
| `os.remove(p)` | `std::filesystem::remove(p)` |
| `Path(p).stem` | `std::filesystem::path(p).stem()` |
| `Path(a) / b` | `std::filesystem::path(a) / b` |
| `glob.glob("*.sdf")` | `directory_iterator` + 拡張子チェック |
| `shutil.copy(a,b)` | `std::filesystem::copy(a, b)` |
| `os.path.getsize(p)` | `std::filesystem::file_size(p)` |

---

## A.8 数学・乱数

| Python | C++ |
|---|---|
| `import math` | `#include <cmath>` |
| `math.sqrt(x)` | `std::sqrt(x)` |
| `math.exp/log/log10` | `std::exp/log/log10` |
| `math.sin/cos/atan2` | `std::sin/cos/atan2` |
| `abs(x)` | `std::abs(x)` |
| `round(x)` | `std::round(x)` |
| `math.floor/ceil` | `std::floor/ceil` |
| `math.pi` | `std::numbers::pi` (C++20) |
| `math.e` | `std::numbers::e` |
| `math.inf` | `std::numeric_limits<double>::infinity()` |
| `math.nan` | `std::numeric_limits<double>::quiet_NaN()` |
| `math.isnan(x)` | `std::isnan(x)` |
| `math.isinf(x)` | `std::isinf(x)` |
| `math.hypot(a,b)` | `std::hypot(a,b)` |
| `sys.maxsize` | `std::numeric_limits<std::size_t>::max()` |
| `import random` | `#include <random>` |
| `random.seed(42)` | `std::mt19937_64 rng(42);` |
| `random.random()` | `std::uniform_real_distribution<>(0,1)(rng)` |
| `random.randint(a,b)` | `std::uniform_int_distribution<>(a,b)(rng)` |
| `random.gauss(m,s)` | `std::normal_distribution<>(m,s)(rng)` |
| `random.shuffle(v)` | `std::shuffle(v.begin(), v.end(), rng)` |
| `random.sample(v,k)` | `std::sample(v.begin(), v.end(), out, k, rng)` |

---

## A.9 時間・計測

| Python | C++ |
|---|---|
| `import time` | `#include <chrono>` |
| `time.time()` | `std::chrono::steady_clock::now()` |
| `time.perf_counter()` | `std::chrono::steady_clock::now()` |
| `time.sleep(1)` | `std::this_thread::sleep_for(1s)` |
| `t1 - t0` | `duration<double>(t1-t0).count()` |
| `datetime.now()` | `std::chrono::system_clock::now()` |
| `%timeit` | Google Benchmark / Catch2 BENCHMARK |
| `cProfile` | perf / Instruments / VTune |

---

## A.10 並列処理

| Python | C++ |
|---|---|
| `threading.Thread` | `std::jthread` |
| `t.start()` / `t.join()` | （コンストラクタで開始）/ `t.join()` |
| `threading.Lock` | `std::mutex` |
| `with lock:` | `std::lock_guard lock(mtx);` |
| `threading.Event` | `std::condition_variable` |
| `queue.Queue` | `std::queue` + `mutex` + `cv` |
| `ThreadPoolExecutor` | 自作 ThreadPool（第25章） |
| `ProcessPoolExecutor` | （不要。GILがないので thread で十分） |
| `multiprocessing.Pool.map` | `#pragma omp parallel for` |
| `concurrent.futures.Future` | `std::future` |
| `asyncio` | （コルーチン C++20。まだ発展途上） |

---

## A.11 NumPy 対応

| NumPy | C++ |
|---|---|
| `np.zeros(n)` | `std::vector<double> v(n);` |
| `np.ones(n)` | `std::vector<double> v(n, 1.0);` |
| `np.arange(n)` | `std::iota` |
| `np.zeros((r,c))` | `std::vector<double> v(r*c);` + `v[i*c+j]` |
| `a.shape` | 自分で保持 |
| `a[i,j]` | `v[i*cols + j]` |
| `a + b` | ループ / `std::transform` |
| `a * b`（要素積） | `std::transform` with `multiplies` |
| `np.dot(a,b)` | `std::inner_product` |
| `a @ b`（行列積） | 自作 / Eigen / BLAS |
| `a.sum()` | `std::accumulate` / `std::reduce` |
| `a.mean()` | `sum / size` |
| `a.max()` | `*std::max_element` |
| `np.argmax(a)` | `std::max_element(...) - begin` |
| `np.argsort(a)` | `iota` + `sort` with comparator |
| `a[a > 5]` | `std::copy_if` |
| `np.where(c, x, y)` | `std::transform` with 三項演算子 |
| `np.cumsum(a)` | `std::partial_sum` |
| `np.unique(a)` | `sort` + `unique` + `erase` |
| `np.packbits` | ビット演算で自作（第22章） |
| `np.unpackbits` | 同上 |
| `a.astype(np.float32)` | `std::transform` with `static_cast<float>` |
| `np.linalg.*` | Eigen / LAPACK |
| `a.reshape(...)` | `std::mdspan` (C++23) / 添字計算 |
| `np.concatenate` | `v.insert(v.end(), ...)` |

**線形代数が必要なら [Eigen](https://eigen.tuxfamily.org/) を使ってください。**
NumPy に近い書き心地で、非常に高速です。

```cpp
#include <Eigen/Dense>
Eigen::MatrixXd A(3, 3);
Eigen::VectorXd b(3);
Eigen::VectorXd x = A.colPivHouseholderQr().solve(b);   // 連立方程式
```

---

## A.12 RDKit Python → RDKit C++

| Python | C++ |
|---|---|
| `Chem.MolFromSmiles(s)` | `RDKit::SmilesToMol(s)` |
| `Chem.MolToSmiles(m)` | `RDKit::MolToSmiles(*m)` |
| `Chem.MolFromSmarts(s)` | `RDKit::SmartsToMol(s)` |
| `m.GetNumAtoms()` | `m->getNumAtoms()` |
| `m.GetAtoms()` | `m->atoms()` |
| `m.GetAtomWithIdx(i)` | `m->getAtomWithIdx(i)` |
| `a.GetSymbol()` | `a->getSymbol()` |
| `a.GetAtomicNum()` | `a->getAtomicNum()` |
| `a.GetDegree()` | `a->getDegree()` |
| `a.GetIsAromatic()` | `a->getIsAromatic()` |
| `a.GetNeighbors()` | `m->atomNeighbors(a)` |
| `m.GetBonds()` | `m->bonds()` |
| `b.GetBondType()` | `b->getBondType()` |
| `Descriptors.MolWt(m)` | `RDKit::Descriptors::calcExactMW(*m)` |
| `Descriptors.MolLogP(m)` | `RDKit::Descriptors::calcClogP(*m)` |
| `Descriptors.TPSA(m)` | `RDKit::Descriptors::calcTPSA(*m)` |
| `rdMolDescriptors.CalcMolFormula` | `RDKit::Descriptors::calcMolFormula` |
| `AllChem.GetMorganFingerprintAsBitVect(m,2,2048)` | `MorganFingerprints::getFingerprintAsBitVect(*m,2,2048)` |
| `DataStructs.TanimotoSimilarity(a,b)` | `TanimotoSimilarity(*a, *b)` |
| `m.HasSubstructMatch(p)` | `RDKit::SubstructMatch(*m, *p, mv)` |
| `m.GetSubstructMatches(p)` | `RDKit::SubstructMatch(*m, *p, matches)` |
| `Chem.SDMolSupplier(f)` | `RDKit::SDMolSupplier sup(f);` |
| `Chem.SDWriter(f)` | `RDKit::SDWriter w(f);` |
| `AllChem.EmbedMolecule(m)` | `RDKit::DGeomHelpers::EmbedMolecule(*m)` |
| `AllChem.MMFFOptimizeMolecule(m)` | `RDKit::MMFF::MMFFOptimizeMolecule(*m)` |
| `Chem.AddHs(m)` | `RDKit::MolOps::addHs(rwmol)` |
| `Chem.SanitizeMol(m)` | `RDKit::MolOps::sanitizeMol(rwmol)` |
| `m.GetRingInfo().NumRings()` | `m->getRingInfo()->numRings()` |
| `m.GetProp("x")` | `m->getProp<std::string>("x")` |
| `m.SetProp("x", v)` | `m->setProp("x", v)` |

---

## A.13 よく使うイディオム

### 引数の受け取り方

```cpp
void f(int x);                        // 軽い型 → 値渡し
void f(const std::string& s);         // 重い型、読むだけ → const参照
void f(std::string_view s);           // 文字列、読むだけ → string_view
void f(std::span<const double> v);    // 配列、読むだけ → span
void f(std::vector<int>& v);          // 書き換える → 参照
void f(std::string s);                // メンバに保存する → 値 + std::move
void f(std::unique_ptr<T> p);         // 所有権をもらう → unique_ptr の値
```

### RAII

```cpp
{
    std::ifstream f(path);        // ファイル
    std::lock_guard lock(mtx);    // ミューテックス
    auto p = std::make_unique<T>();  // メモリ
    ScopedTimer t("phase1");      // 計測
}   // ★ すべて自動的に解放される
```

### エラー処理

```cpp
assert(i < n);                              // バグの検出（Debugのみ）
static_assert(sizeof(T) == 8);              // コンパイル時
throw std::runtime_error("...");            // 想定外のエラー
std::optional<T> f();                       // 失敗するかも
std::expected<T, E> f();                    // 失敗の理由も返す
void f() noexcept;                          // 絶対に失敗しない
```

### 並列化

```cpp
// 静的分割
std::vector<std::jthread> ts;
for (unsigned t = 0; t < nt; ++t)
    ts.emplace_back([&, t]{ /* 担当範囲を処理 */ });

// 動的分散
std::atomic<std::size_t> next{0};
// ... next.fetch_add(GRAIN) でブロックを取る

// OpenMP
#pragma omp parallel for schedule(dynamic, 64)
for (int i = 0; i < n; ++i) { ... }
```

### 高速化

```cpp
v.reserve(n);                          // 再確保を防ぐ
for (const auto& x : v)                // コピーを防ぐ
std::vector<T> flat(rows * cols);      // 連続メモリ
std::popcount(x)                       // ハードウェア命令
if (min/max < threshold) continue;     // 枝刈り
```

---

## A.14 コンパイルコマンド早見表

```bash
# 開発中
g++ -std=c++20 -g -O0 -Wall -Wextra -fsanitize=address,undefined \
    -D_GLIBCXX_ASSERTIONS prog.cpp -o prog

# 本番
g++ -std=c++20 -O2 -DNDEBUG -Wall -Wextra prog.cpp -o prog

# 最速（このマシン専用）
g++ -std=c++20 -O3 -march=native -DNDEBUG -flto prog.cpp -o prog

# プロファイリング
g++ -std=c++20 -O2 -g -fno-omit-frame-pointer prog.cpp -o prog

# 並列（OpenMP）
g++ -std=c++20 -O2 -fopenmp prog.cpp -o prog

# 複数ファイル
g++ -std=c++20 -O2 -Iinclude src/*.cpp main.cpp -o prog

# 共有ライブラリ
g++ -std=c++20 -O2 -fPIC -shared src/*.cpp -o libchemcpp.so

# Python モジュール
g++ -O3 -shared -std=c++20 -fPIC $(python3 -m pybind11 --includes) \
    bindings.cpp -o mod$(python3-config --extension-suffix)

# CMake
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

---

## A.15 「Pythonのアレ」はC++でどう書くか

### enumerate + zip

```python
for i, (a, b) in enumerate(zip(xs, ys)):
    ...
```

```cpp
for (std::size_t i = 0; i < std::min(xs.size(), ys.size()); ++i) {
    const auto& a = xs[i];
    const auto& b = ys[i];
}
```

### 辞書のデフォルト値

```python
from collections import defaultdict
d = defaultdict(list)
d["k"].append(1)
```

```cpp
std::unordered_map<std::string, std::vector<int>> d;
d["k"].push_back(1);      // ★ operator[] が自動で作ってくれる
```

### Counter

```python
from collections import Counter
c = Counter(items)
```

```cpp
std::unordered_map<T, int> c;
for (const auto& x : items) ++c[x];
```

### リスト内包表記のネスト

```python
result = [f(x, y) for x in xs for y in ys if p(x, y)]
```

```cpp
std::vector<R> result;
result.reserve(xs.size() * ys.size());
for (const auto& x : xs)
    for (const auto& y : ys)
        if (p(x, y)) result.push_back(f(x, y));
```

### スワップ

```python
a, b = b, a
```

```cpp
std::swap(a, b);
```

### 複数戻り値

```python
def f(): return 1, 2.0, "x"
a, b, c = f()
```

```cpp
struct Result { int a; double b; std::string c; };
Result f() { return {1, 2.0, "x"}; }
auto [a, b, c] = f();
```

### ジェネレータ

```python
def gen():
    for x in data:
        yield transform(x)
```

```cpp
// 選択肢1: コールバック
template <typename F>
void gen(F&& callback) {
    for (const auto& x : data) callback(transform(x));
}

// 選択肢2: ranges（遅延評価）
auto gen() { return data | std::views::transform(transform); }

// 選択肢3: C++20 コルーチン（複雑。当面は上の2つで）
```

### with文（複数）

```python
with open(a) as f, open(b, "w") as g:
    ...
```

```cpp
{
    std::ifstream f(a);
    std::ofstream g(b);
    // ...
}   // 両方自動的に閉じる
```

### 文字列の繰り返し

```python
"-" * 40
```

```cpp
std::string(40, '-')
```

### chained comparison

```python
if 0 <= x < len(v):
```

```cpp
if (0 <= x && x < static_cast<int>(v.size()))
// または符号なしなら
if (x < v.size())    // 負数は巨大な符号なし値になるので、これだけで両方チェックできる
```

---

→ [付録B コンパイルエラー辞典](appendix-b-error-dictionary.md)
