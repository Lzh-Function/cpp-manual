# 付録C 用語集

C++とケモインフォマティクスの用語を、Pythonユーザー向けに解説します。

---

## C.1 C++ の言語用語

### ABI (Application Binary Interface)
コンパイル済みバイナリ同士が連携するための約束事（関数の呼び出し規約、
構造体のレイアウトなど）。**コンパイラやC++標準のバージョンが違うと
ABI非互換になり、リンクエラーや実行時クラッシュが起きる**ことがある。
RDKit を使うときに C++17 で揃える必要があるのはこのため（第27章）。

### auto
型推論。コンパイル時に右辺から型が決まる。実行時コストゼロ。
`auto x = 42;` は `int x = 42;` と全く同じ機械語になる（第4章、第17章）。

### CRTP (Curiously Recurring Template Pattern)
`class D : public Base<D>` という形で、仮想関数なしに静的な多態性を実現する技法。
Eigen などで使われる（第14章）。

### constexpr
コンパイル時に評価される定数・関数。実行時コストゼロ。
配列サイズやテンプレート引数に使える（第4章）。

### const 汚染 (const poisoning)
`const` を付け忘れた関数があると、それを呼ぶ側も `const` にできなくなり、
連鎖的に `const` が付けられなくなる現象。**最初から正しく付けるのが対策**（第11章）。

### concept (C++20)
テンプレート引数に対する制約。
`template <std::floating_point T>` のように書き、エラーメッセージを劇的に改善する（第15章）。

### decay（配列のポインタへの退化）
配列を関数に渡すとポインタになり、サイズ情報が失われる現象。
`void f(int arr[10])` は実は `void f(int* arr)`（第7章）。

### explicit
暗黙の型変換を禁止する指定。引数1つのコンストラクタには原則付ける（第11章）。

### false sharing
異なるスレッドが、同じキャッシュライン（64バイト）内の別々の変数を
更新することで発生する性能低下。パディングで分離する（第25章）。

### inline
本来の意味は「複数の翻訳単位で定義されても二重定義エラーにしない」。
最適化ヒントとしての意味はほぼない（現代のコンパイラは自分で判断する）（第5章）。

### lvalue / rvalue（左辺値 / 右辺値）
- **左辺値**: 名前があり、アドレスが取れるもの（`x`, `v[0]`）
- **右辺値**: 一時的な値（`5`, `x+1`, `f()`）

右辺値は誰も使わないので、中身を盗んでよい = ムーブできる（第12章）。

### mangling（名前修飾）
オーバーロードを実現するため、コンパイラが関数名に引数の型情報を埋め込むこと。
`_ZN8Molecule5parseE...` のような呪文になる。`c++filt` で人間語に戻せる（第3章）。

### move（ムーブ）
リソースの所有権を移す操作。データをコピーせず、ポインタを付け替える。
`std::move` は**キャストであって、何も動かさない**（第12章）。

### NRVO / RVO (Named / Return Value Optimization)
関数の戻り値を、呼び出し元の場所に直接構築する最適化。
**値で返してもコピーは起きない**（第12章）。

### noexcept
「この関数は例外を投げない」という宣言。
**ムーブコンストラクタに付け忘れると vector の再確保が数十倍遅くなる**（第12章、第18章）。

### POD (Plain Old Data)
メタデータを持たない素のデータ型。`int`, `double`, 単純な `struct`。
`memcpy` でコピーでき、バイナリI/Oに直接使える。
現代では `std::is_trivially_copyable` などで判定する（第1章）。

### RAII (Resource Acquisition Is Initialization)
**C++で最も重要なイディオム。** リソースの取得をコンストラクタで、
解放をデストラクタで行う。**スコープを抜ければ必ず解放される**。
Pythonの `with` 文が言語に組み込まれたようなもの（第11章）。

### Rule of Zero / Three / Five
- **Rule of Zero**: リソースを直接管理しないクラスは、
  5つの特殊メンバ関数を一切書かない ← **これが正解**
- **Rule of Five**: 1つでも書くなら、5つ全部書く
  （デストラクタ、コピーctor/代入、ムーブctor/代入）（第12章）

### SFINAE (Substitution Failure Is Not An Error)
テンプレートの置換に失敗しても、エラーではなく候補から外れるだけ、というルール。
`std::enable_if` を使った古い技法。**C++20 の concept で不要になった**（第15章）。

### SIMD (Single Instruction, Multiple Data)
1命令で複数のデータを同時に処理する技術。AVX2、AVX-512、NEONなど。
**まずコンパイラの自動ベクトル化を試すこと**（第22章、第28章）。

### SSO (Small String Optimization)
短い文字列をヒープではなく `std::string` オブジェクト内に直接格納する最適化。
libstdc++ では15文字まで（第8章）。

### slicing（スライシング）
派生クラスのオブジェクトを基底クラスの**値**に代入すると、
派生部分が切り落とされる現象。**多態的なものは参照/ポインタで扱う**（第14章）。

### UB (Undefined Behavior、未定義動作)
規格が動作を定めていない操作。**何が起きても文句を言えない**。
配列の範囲外、符号付き整数のオーバーフロー、null参照、データ競合など。
**サニタイザで検出する**（第29章）。

### vtable（仮想関数テーブル）
仮想関数の実装アドレスを格納したテーブル。
仮想関数を持つクラスのオブジェクトには、vtableへのポインタ（vptr、8バイト）が
隠れて追加される。**仮想呼び出しはインライン展開できない**（第14章）。

### ゼロオーバーヘッド原則
「使わない機能にはコストを払わない。使う機能は手書きと同等以上に速い」
というC++の設計哲学。**抽象化してもタダ**（第1章）。

### 翻訳単位 (translation unit)
1つの `.cpp` ファイルと、それが `#include` した全内容。
コンパイラはこの単位で処理する（第3章）。

---

## C.2 メモリ関連

### スタック (stack)
関数のローカル変数が置かれる領域。確保・解放が超高速（ポインタを動かすだけ）。
サイズは小さい（1〜8MB）。スコープを抜けると自動解放（第6章）。

### ヒープ (heap)
`new` / `malloc` で動的に確保される領域。遅いが大きい。
明示的な解放が必要（vectorやスマートポインタが代行）（第6章）。

### キャッシュライン
CPUがメモリから読む最小単位。通常64バイト。
**データが連続していれば1回の読み込みで多くのデータが手に入る**（第6章）。

### キャッシュミス
必要なデータがキャッシュになく、メインメモリから読む必要がある状態。
**200〜400サイクルの待ち時間**が発生する（第6章、第28章）。

### ダングリング参照/ポインタ
既に破棄されたオブジェクトを指す参照/ポインタ。
使うと未定義動作。**ローカル変数への参照を返してはいけない**（第6章）。

### メモリリーク
確保したメモリを解放し忘れること。
**スマートポインタと vector を使えば起きない**（第13章）。

### AoS / SoA (Array of Structures / Structure of Arrays)
- **AoS**: `std::vector<Molecule>` — 1分子の全情報が近くにある
- **SoA**: `struct { vector<double> mw; vector<double> logp; }` —
  同じフィールドが連続

**一括処理には SoA が速い**（第7章、第23章）。

### アライメント
データが特定のバイト境界に配置される要件。
`double` は8バイト境界など。構造体にパディング（隙間）が入る原因（第4章）。

### 所有権 (ownership)
「このオブジェクトを解放する責任は誰にあるか」。
`unique_ptr` = 単独所有、`shared_ptr` = 共有所有、
生ポインタ/参照 = 所有しない（第13章）。

---

## C.3 ビルド関連

### コンパイル
`.cpp` → `.o`（オブジェクトファイル）への変換。
実際には プリプロセス → コンパイル → アセンブル の3段階（第3章）。

### リンク
複数の `.o` とライブラリを結合して実行ファイルを作る工程。
`undefined reference` はリンクエラー（第3章）。

### 静的ライブラリ (.a / .lib)
実行ファイルに埋め込まれるライブラリ。配布が楽だがサイズが大きい。

### 共有ライブラリ (.so / .dll / .dylib)
実行時にロードされるライブラリ。複数プログラムで共有できるが、
実行時にパスが通っている必要がある。

### LTO (Link Time Optimization)
リンク時に、ファイルをまたいだ最適化（インライン展開など）を行う。
`-flto`（第19章、第28章）。

### PGO (Profile Guided Optimization)
実行時のプロファイルを使ってコンパイラを最適化する手法。
`-fprofile-generate` → 実行 → `-fprofile-use`（第28章）。

### ヘッダオンリーライブラリ
`.hpp` ファイルだけで完結するライブラリ。
コピーするだけで使えるので導入が楽（第19章）。

### CMake
ビルド手順の設計図を書くツール。Makefile や Ninja を**生成**する。
CMake自体はビルドしない（第19章）。

### Ninja
高速なビルドツール。Makeの代替。`cmake -G Ninja` で使う。

### ccache
コンパイル結果をキャッシュして再ビルドを高速化するツール。

---

## C.4 ツール

### ASan (AddressSanitizer)
メモリエラー（範囲外、解放後使用、リーク）を検出。
`-fsanitize=address`。**C++で最も価値のあるツール**（第29章）。

### UBSan (UndefinedBehaviorSanitizer)
未定義動作を検出。`-fsanitize=undefined`（第29章）。

### TSan (ThreadSanitizer)
データ競合を検出。`-fsanitize=thread`（第29章）。

### Valgrind
実行時解析ツール。再コンパイル不要だが遅い（20〜50倍）。

### perf
Linuxのプロファイラ。`perf record` / `perf stat`（第28章）。

### clang-tidy
静的解析ツール。バグの可能性やモダンでない書き方を指摘（第29章）。

### clang-format
コード整形ツール。`.clang-format` で設定（第19章）。

### clangd
Language Server。VSCodeの補完とエラー表示を担当（第2章）。

### Compiler Explorer (godbolt.org)
ブラウザ上でC++を書くと、生成された機械語がリアルタイムで見えるツール。
**最適化の確認に必須**（第2章、第28章）。

### c++filt
マングリングされた名前を人間語に戻すツール（付録B）。

---

## C.5 ケモインフォマティクス用語

### SMILES (Simplified Molecular Input Line Entry System)
分子を1行の文字列で表す記法。`CCO` = エタノール、
`c1ccccc1` = ベンゼン。小文字は芳香族（第20章）。

### SMARTS
SMILESを拡張した、部分構造検索用のパターン言語。
`[CX3](=O)[OX2H1]` = カルボン酸（第27章）。

### InChI / InChIKey
IUPAC標準の分子識別子。InChIKeyは27文字の固定長ハッシュで、
データベースの検索キーに使われる。

### SDF (Structure Data File)
MDL Molfile + プロパティフィールドの形式。
実務で最もよく使われる分子ファイル形式（第24章）。

### ECFP (Extended-Connectivity Fingerprint)
= **Morganフィンガープリント**。各原子の周辺環境を反復的にハッシュ化して
ビットベクタにする。ECFP4 は半径2（直径4）（第21章）。

### FCFP (Functional-Class Fingerprint)
ECFPの変種。原子の種類ではなく、薬理特徴
（ドナー/アクセプタ/芳香/疎水など）でハッシュする（第21章）。

### Tanimoto係数 (= Jaccard係数)
2つのビットベクタの類似度。`c / (a + b - c)`。
ケモインフォマティクスで最も使われる指標（第22章）。

### Dice係数 (= Sørensen係数)
`2c / (a + b)`。Tanimotoより高い値が出る。常に Dice ≥ Tanimoto。

### Tversky係数
非対称な類似度。`c / (α(a-c) + β(b-c) + c)`。
α=β=1 で Tanimoto、α=β=0.5 で Dice。
部分構造検索的な用途で α を大きくする（第22章）。

### folding（ビット畳み込み）
ハッシュ値を `mod nBits` でビットベクタに押し込む操作。
**ビット衝突が起きる**のがフィンガープリントの精度限界の一因（第21章）。

### popcount (population count)
ビット列で1が立っている個数を数える操作。
**CPU命令1個で実行できる**（`std::popcount`）（第22章）。

### Lipinski's Rule of Five
経口薬らしさの経験則。MW ≤ 500、logP ≤ 5、HBD ≤ 5、HBA ≤ 10。
違反2つ以上で「薬らしくない」（第10章、第24章）。

### TPSA (Topological Polar Surface Area)
極性表面積。膜透過性の指標。N と O の寄与を原子タイプごとに足し合わせる
（Ertl法）（第24章）。

### logP
オクタノール/水分配係数の対数。脂溶性の指標。
Crippen法（原子寄与法）でよく計算される（第24章）。

### QED (Quantitative Estimate of Drug-likeness)
複数の記述子を統合した0〜1の薬らしさスコア（Bickerton 2012）（第24章）。

### HBD / HBA
水素結合ドナー / アクセプタの数。
ドナーは N-H, O-H を持つ原子、アクセプタは N, O（第24章）。

### 回転可能結合 (rotatable bond)
単結合で、環内でなく、末端でなく、アミド結合でないもの。
分子の柔軟性の指標（第24章）。

### SSSR (Smallest Set of Smallest Rings)
最小環集合。分子の環構造を特定する標準的な方法。
環の数は「結合数 - 原子数 + 連結成分数」（Frèrejacque数）（第21章、第24章）。

### 芳香族性 (aromaticity)
Hückel則（4n+2 π電子）による判定。
実装によって定義が異なる（RDKit、Daylight、MDLで違う）（第20章）。

### ケクレ化 (kekulization)
芳香結合を、交互の単結合/二重結合に変換すること。
逆方向は芳香族化（第20章）。

### 正準SMILES (canonical SMILES)
同じ分子から常に同じ文字列が得られるSMILES。
Morganアルゴリズムで原子に正準順序を割り当てて生成する。

### 部分構造検索 (substructure search)
分子の中に特定の構造が含まれるかの判定。
グラフの部分同型問題で、NP困難。VF2アルゴリズムなどを使う。
**フィンガープリントで事前フィルタする**のが実用的（第22章）。

### MCS (Maximum Common Substructure)
2分子の最大共通部分構造。NP困難。

### コンフォマー (conformer)
分子の3次元配座。単結合の回転で生じる異なる立体構造（第27章）。

### 力場 (force field)
分子のエネルギーを計算する経験的なモデル。MMFF94、UFF、AMBERなど（第27章）。

### 記述子 (descriptor)
分子を数値で特徴づける量。分子量、logP、TPSA など。
機械学習の特徴量として使う（第24章）。

### 仮想スクリーニング (virtual screening)
計算機上で大量の化合物から有望なものを絞り込むこと。
類似度検索、ドッキング、機械学習モデルなど（第23章）。

### 構造アラート (structural alert)
毒性や反応性が懸念される部分構造。
PAINS、Brenk、NIH などのルールセットがある（第27章）。

---

## C.6 数値・計算用語

### IPC (Instructions Per Cycle)
1サイクルあたりの実行命令数。**2.0前後なら健全、1.0以下は何かが詰まっている**（第28章）。

### memory bound / compute bound
- **memory bound**: メモリ帯域が律速。計算を速くしても効果がない
- **compute bound**: CPU演算が律速

大規模類似度検索は memory bound になりやすい（第23章）。

### Amdahlの法則
「全体の一部を高速化しても、全体の改善は限定的」という法則。
20%の処理を10倍速くしても、全体は1.2倍にしかならない（第28章）。

### 埋め込み並列 (embarrassingly parallel)
タスク間に依存がなく、そのまま並列化できる問題。
ケモインフォマティクスの多くはこれ（第25章）。

### GIL (Global Interpreter Lock)
CPythonの制約で、同時に1スレッドしかPythonコードを実行できない。
**C++コードの実行中は解放できる**（第26章）。

### データ競合 (data race)
複数スレッドが同じメモリを、少なくとも1つが書き込みでアクセスすること。
**未定義動作**。TSanで検出（第25章、第29章）。

### アトミック操作
分割不可能な操作。`std::atomic`。ロックより軽いが、
キャッシュラインの奪い合いは起きる（第25章）。

### 分岐予測 (branch prediction)
CPUが `if` の結果を先読みする機構。外れると15〜20サイクルのペナルティ。
**ソート済みデータの方が速い理由**（第28章）。

### 自動ベクトル化
コンパイラがループをSIMD命令に変換する最適化。
`-O3 -march=native` で有効。`-fopt-info-vec` で確認（第28章）。

---

## C.7 略語一覧

| 略語 | 正式名称 |
|---|---|
| ABI | Application Binary Interface |
| ASan | AddressSanitizer |
| AoS | Array of Structures |
| AVX | Advanced Vector Extensions |
| CRTP | Curiously Recurring Template Pattern |
| CTAD | Class Template Argument Deduction |
| ECFP | Extended-Connectivity Fingerprint |
| FCFP | Functional-Class Fingerprint |
| GIL | Global Interpreter Lock |
| HBA/HBD | Hydrogen Bond Acceptor / Donor |
| IPC | Instructions Per Cycle |
| LTO | Link Time Optimization |
| MCS | Maximum Common Substructure |
| NRVO | Named Return Value Optimization |
| PGO | Profile Guided Optimization |
| POD | Plain Old Data |
| QED | Quantitative Estimate of Drug-likeness |
| RAII | Resource Acquisition Is Initialization |
| RTTI | Run-Time Type Information |
| RVO | Return Value Optimization |
| SFINAE | Substitution Failure Is Not An Error |
| SIMD | Single Instruction Multiple Data |
| SMARTS | SMILES Arbitrary Target Specification |
| SMILES | Simplified Molecular Input Line Entry System |
| SoA | Structure of Arrays |
| SSO | Small String Optimization |
| SSSR | Smallest Set of Smallest Rings |
| STL | Standard Template Library |
| TBB | Threading Building Blocks |
| TPSA | Topological Polar Surface Area |
| TSan | ThreadSanitizer |
| UB | Undefined Behavior |
| UBSan | UndefinedBehaviorSanitizer |

---

→ [付録D 練習問題の解答と次の一歩](appendix-d-solutions.md)
