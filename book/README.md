# PythonケモインフォマティシャンのためのC++入門

> 「深層学習はPyTorchで回せる。でも、1000万化合物の類似度検索が3日かかる。」
> ——そういう人のための本です。

---

## この本について

この本は、**普段Pythonで深層学習・ケモインフォマティクスの研究をしている人**が、
C++を**実務で使えるレベル**まで持っていくための入門書です。

一般的なC++入門書と違うところ:

| 普通のC++入門書 | この本 |
|---|---|
| 「Hello, World!」から始まる | 「なぜPythonでは遅いのか」から始まる |
| 例題は電卓、成績管理、動物クラス | 例題はSMILESパーサ、ECFP、Tanimoto検索 |
| 文法を網羅的に説明 | Pythonの知識に**接続**しながら説明 |
| ポインタで挫折させる | ポインタを**メモリの絵**で腹落ちさせる |
| C++単体で完結 | 最終的に**Pythonから呼べるように**する |

**前提知識**: Pythonが書けること。それだけです。
NumPy / RDKit / PyTorch を触ったことがあると、例え話がより刺さります。

**前提としないもの**: C言語、コンパイル、メモリ管理、ポインタ、makeなどの知識。全部ゼロから説明します。

---

## 目次

### 第I部 — なぜC++か、そして最初の一歩

| 章 | タイトル | 内容 |
|---|---|---|
| [第0章](ch00-how-to-read.md) | この本の読み方 | 学習ロードマップ、挫折しないための地図 |
| [第1章](ch01-why-cpp.md) | なぜPython使いがC++を学ぶのか | Pythonが遅い本当の理由、100倍速の実測 |
| [第2章](ch02-setup.md) | 環境構築 | Windows/macOS/Linux、コンパイラ、VSCode |
| [第3章](ch03-hello-molecule.md) | Hello, Molecule | 最初のプログラムとコンパイルの仕組み |

### 第II部 — 言語の核心

| 章 | タイトル | 内容 |
|---|---|---|
| [第4章](ch04-types-and-variables.md) | 型と変数 | 静的型付け、整数の罠、浮動小数点、const |
| [第5章](ch05-control-flow-and-functions.md) | 制御構文と関数 | if/for/while、値渡しと参照渡し、オーバーロード |
| [第6章](ch06-memory-model.md) | メモリモデル：スタックとヒープ | **この本の心臓部**。ポインタと参照を絵で理解する |
| [第7章](ch07-vector-and-arrays.md) | 配列とstd::vector | Pythonのlistとの決定的な違い、reserve、二次元配列 |
| [第8章](ch08-strings.md) | 文字列 | std::string、string_view、SMILESを刻む |
| [第9章](ch09-stl-containers.md) | STLコンテナ完全ガイド | map/unordered_map/set/array/deque、dict対応表 |
| [第10章](ch10-iterators-and-algorithms.md) | イテレータと\<algorithm\> | sort/find/transform、Pythonの内包表記の代わり |
| [第11章](ch11-classes-and-raii.md) | クラスとRAII | コンストラクタ・デストラクタ、\_\_init\_\_との違い |
| [第12章](ch12-copy-and-move.md) | コピーとムーブ | rule of five/zero、std::move、なぜ速いのか |
| [第13章](ch13-smart-pointers.md) | スマートポインタと所有権設計 | unique_ptr/shared_ptr、newを書かない現代C++ |
| [第14章](ch14-inheritance-and-polymorphism.md) | 継承と多態性 | virtual、抽象基底クラス、そして「継承より合成」 |
| [第15章](ch15-templates.md) | テンプレート入門 | ジェネリクス、concept、コンパイル時計算 |
| [第16章](ch16-lambdas.md) | ラムダと関数オブジェクト | キャプチャ、std::function、コールバック設計 |
| [第17章](ch17-modern-cpp-toolbox.md) | モダンC++道具箱 | auto/構造化束縛/optional/variant/chrono/filesystem/format/span/ranges |
| [第18章](ch18-error-handling.md) | エラー処理 | 例外、expected、assert、契約プログラミング |

### 第III部 — 実践ケモインフォマティクス

| 章 | タイトル | 内容 |
|---|---|---|
| [第19章](ch19-project-and-cmake.md) | プロジェクトを作る | CMake、ヘッダと実装の分離、ライブラリ化 |
| [第20章](ch20-smiles-parser.md) | 実践1: SMILESパーサ | トークナイザから分子グラフ構築まで |
| [第21章](ch21-molecular-graph-and-ecfp.md) | 実践2: 分子グラフとECFP | Morganフィンガープリントを自作する |
| [第22章](ch22-bitvector-and-tanimoto.md) | 実践3: ビットベクタとTanimoto | popcount、ビット演算、`__builtin_popcountll` |
| [第23章](ch23-large-scale-search.md) | 実践4: 100万化合物の高速検索 | SoA、キャッシュ、枝刈り、SIMD、実測 |
| [第24章](ch24-descriptors-and-sdf.md) | 実践5: 記述子計算とSDF入出力 | 分子量、環検出(SSSR)、ファイルI/O |
| [第25章](ch25-parallelism.md) | 並列化 | thread/mutex/atomic、OpenMP、std::execution |
| [第26章](ch26-pybind11.md) | Pythonから使う | pybind11 / nanobind、NumPy連携、GIL解放 |
| [第27章](ch27-rdkit-cpp-api.md) | RDKitのC++ API | ROMol、Conformer、既存資産の活用 |
| [第28章](ch28-profiling-and-optimization.md) | プロファイリングと最適化 | 計測の作法、キャッシュ、分岐予測、最適化12箇条 |
| [第29章](ch29-testing-and-debugging.md) | テスト・デバッグ・サニタイザ | Catch2、gdb/lldb、ASan/UBSan、CI |

### 第IV部 — 付録

| 章 | タイトル |
|---|---|
| [付録A](appendix-a-cheatsheet.md) | Python → C++ 完全チートシート |
| [付録B](appendix-b-error-dictionary.md) | コンパイルエラー辞典（読めない呪文を読む） |
| [付録C](appendix-c-glossary.md) | 用語集 |
| [付録D](appendix-d-solutions.md) | 練習問題の解答と、次に読むべきもの |

---

## サンプルコードの動かし方

単一ファイルの例は、そのまま保存してコンパイルできます。

```bash
g++ -std=c++20 -O2 -Wall -Wextra example.cpp -o example
./example
```

第III部で作るライブラリ [`book/code/chemcpp/`](https://github.com/Lzh-Function/cpp-manual/tree/main/book/code/chemcpp) は CMake で一発ビルドできます。

```bash
cd code/chemcpp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/parse_demo                                            # SMILESパース
./build/fp_demo                                               # 類似度行列
./build/descriptors_cli data/sample.smi out.csv               # 記述子計算
./build/screen data/sample.smi "CC(=O)Oc1ccccc1C(=O)O" 0.4    # 類似度検索
```

環境構築は[第2章](ch02-setup.md)で丁寧に説明します。

> ⚠️ サンプルコードは執筆環境にコンパイラが無かったため**未検証**です。
> エラーが出たら[付録B](appendix-b-error-dictionary.md)を引いてください。

---

## 表記について

- 🐍 …… Pythonとの対比コラム
- ⚠️ …… ハマりどころ・罠
- 🧪 …… ケモインフォマティクス的な実例
- 💡 …… 実務のコツ
- 📝 …… 練習問題

コード中の `// ...` は省略を表します。

---

*Happy compiling.*
