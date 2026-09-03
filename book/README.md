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
| [第0章](ch00_この本の読み方.md) | この本の読み方 | 学習ロードマップ、挫折しないための地図 |
| [第1章](ch01_なぜPython使いがCppを学ぶのか.md) | なぜPython使いがC++を学ぶのか | Pythonが遅い本当の理由、100倍速の実測 |
| [第2章](ch02_環境構築.md) | 環境構築 | Windows/macOS/Linux、コンパイラ、VSCode |
| [第3章](ch03_Hello_Molecule.md) | Hello, Molecule | 最初のプログラムとコンパイルの仕組み |

### 第II部 — 言語の核心

| 章 | タイトル | 内容 |
|---|---|---|
| [第4章](ch04_型と変数.md) | 型と変数 | 静的型付け、整数の罠、浮動小数点、const |
| [第5章](ch05_制御構文と関数.md) | 制御構文と関数 | if/for/while、値渡しと参照渡し、オーバーロード |
| [第6章](ch06_メモリモデル.md) | メモリモデル：スタックとヒープ | **この本の心臓部**。ポインタと参照を絵で理解する |
| [第7章](ch07_vectorと配列.md) | 配列とstd::vector | Pythonのlistとの決定的な違い、reserve、二次元配列 |
| [第8章](ch08_文字列.md) | 文字列 | std::string、string_view、SMILESを刻む |
| [第9章](ch09_STLコンテナ.md) | STLコンテナ完全ガイド | map/unordered_map/set/array/deque、dict対応表 |
| [第10章](ch10_イテレータとアルゴリズム.md) | イテレータと\<algorithm\> | sort/find/transform、Pythonの内包表記の代わり |
| [第11章](ch11_クラスとRAII.md) | クラスとRAII | コンストラクタ・デストラクタ、\_\_init\_\_との違い |
| [第12章](ch12_コピーとムーブ.md) | コピーとムーブ | rule of five/zero、std::move、なぜ速いのか |
| [第13章](ch13_スマートポインタ.md) | スマートポインタと所有権設計 | unique_ptr/shared_ptr、newを書かない現代C++ |
| [第14章](ch14_継承と多態性.md) | 継承と多態性 | virtual、抽象基底クラス、そして「継承より合成」 |
| [第15章](ch15_テンプレート.md) | テンプレート入門 | ジェネリクス、concept、コンパイル時計算 |
| [第16章](ch16_ラムダと関数オブジェクト.md) | ラムダと関数オブジェクト | キャプチャ、std::function、コールバック設計 |
| [第17章](ch17_モダンCpp道具箱.md) | モダンC++道具箱 | auto/構造化束縛/optional/variant/chrono/filesystem/format/span/ranges |
| [第18章](ch18_エラー処理.md) | エラー処理 | 例外、expected、assert、契約プログラミング |

### 第III部 — 実践ケモインフォマティクス

| 章 | タイトル | 内容 |
|---|---|---|
| [第19章](ch19_プロジェクトとCMake.md) | プロジェクトを作る | CMake、ヘッダと実装の分離、ライブラリ化 |
| [第20章](ch20_実践_SMILESパーサ.md) | 実践1: SMILESパーサ | トークナイザから分子グラフ構築まで |
| [第21章](ch21_実践_分子グラフとECFP.md) | 実践2: 分子グラフとECFP | Morganフィンガープリントを自作する |
| [第22章](ch22_実践_ビットベクタとTanimoto.md) | 実践3: ビットベクタとTanimoto | popcount、ビット演算、`__builtin_popcountll` |
| [第23章](ch23_実践_大規模類似度検索.md) | 実践4: 100万化合物の高速検索 | SoA、キャッシュ、枝刈り、SIMD、実測 |
| [第24章](ch24_実践_記述子とSDF.md) | 実践5: 記述子計算とSDF入出力 | 分子量、環検出(SSSR)、ファイルI/O |
| [第25章](ch25_並列化.md) | 並列化 | thread/mutex/atomic、OpenMP、std::execution |
| [第26章](ch26_pybind11.md) | Pythonから使う | pybind11 / nanobind、NumPy連携、GIL解放 |
| [第27章](ch27_RDKit_CppAPI.md) | RDKitのC++ API | ROMol、Conformer、既存資産の活用 |
| [第28章](ch28_プロファイリングと最適化.md) | プロファイリングと最適化 | 計測の作法、キャッシュ、分岐予測、最適化12箇条 |
| [第29章](ch29_テストとデバッグ.md) | テスト・デバッグ・サニタイザ | Catch2、gdb/lldb、ASan/UBSan、CI |

### 第IV部 — 付録

| 章 | タイトル |
|---|---|
| [付録A](appendixA_Python_Cpp_チートシート.md) | Python → C++ 完全チートシート |
| [付録B](appendixB_コンパイルエラー辞典.md) | コンパイルエラー辞典（読めない呪文を読む） |
| [付録C](appendixC_用語集.md) | 用語集 |
| [付録D](appendixD_練習問題と次の一歩.md) | 練習問題の解答と、次に読むべきもの |

---

## サンプルコードの動かし方

単一ファイルの例は、そのまま保存してコンパイルできます。

```bash
g++ -std=c++20 -O2 -Wall -Wextra example.cpp -o example
./example
```

第III部で作るライブラリ [`code/chemcpp/`](code/chemcpp/) は CMake で一発ビルドできます。

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

環境構築は[第2章](ch02_環境構築.md)で丁寧に説明します。

> ⚠️ サンプルコードは執筆環境にコンパイラが無かったため**未検証**です。
> エラーが出たら[付録B](appendixB_コンパイルエラー辞典.md)を引いてください。

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
