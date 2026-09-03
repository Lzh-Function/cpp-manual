# chemcpp — 本書のサンプルライブラリ

第19〜24章、第29章で作るケモインフォマティクスの小さなライブラリです。

## 構成

```
chemcpp/
├── CMakeLists.txt
├── CMakePresets.json
├── include/chemcpp/
│   ├── element.hpp        # 元素テーブル（constexpr）      第20章
│   ├── molecule.hpp       # 分子グラフのデータ構造          第20章
│   ├── smiles.hpp         # SMILESパーサ                    第20章
│   ├── graph.hpp          # BFS/連結成分/環検出             第21章
│   ├── fingerprint.hpp    # Morgan (ECFP)                   第21章
│   ├── similarity.hpp     # Tanimoto/Dice/Cosine/Tversky    第22章
│   ├── database.hpp       # SoA フィンガープリントDB        第23章
│   ├── search.hpp         # 閾値検索 / Top-K / 並列版       第23章
│   ├── descriptors.hpp    # 記述子                          第24章
│   └── io.hpp             # ファイル入出力                  第24章
├── src/                   # 各実装
├── apps/
│   ├── parse_demo.cpp      # SMILESパースのデモ
│   ├── fp_demo.cpp         # フィンガープリントと類似度
│   ├── screen.cpp          # 大規模類似度検索
│   └── descriptors_cli.cpp # 記述子計算CLI
└── tests/
    └── test_chemcpp.cpp    # Catch2 テストスイート
```

## ビルド

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

プリセットを使わない場合:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

> **Ninja が無い場合**は `-G Ninja` を外すか、
> `CMakePresets.json` の `"generator"` を削除してください。

## 実行

```bash
# SMILESをパースして情報を表示
./build/release/parse_demo
./build/release/parse_demo "CC(=O)Oc1ccccc1C(=O)O"

# フィンガープリントと類似度行列
./build/release/fp_demo

# 記述子をCSVに出力
./build/release/descriptors_cli data/sample.smi out.csv

# 類似度検索（閾値0.6）
./build/release/screen data/sample.smi "CC(=O)Oc1ccccc1C(=O)O" 0.6

# Top-10 検索
./build/release/screen data/sample.smi "CC(=O)Oc1ccccc1C(=O)O" 0 10
```

## デバッグ・検証

```bash
# AddressSanitizer + UBSan でテストを実行
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

## 入力ファイル形式

`.smi` は「SMILES + TAB + ID」の1行1分子です。

```
CCO	ethanol
c1ccccc1	benzene
CC(=O)Oc1ccccc1C(=O)O	aspirin
```

## 注意

- **教育目的の実装です。** 芳香族の検証、ケクレ化、立体化学の解決、
  正準SMILES生成などは実装していません。
  本番の解析には [RDKit](https://www.rdkit.org/) を使ってください（第27章）。
- 記述子（logP、TPSA）は簡易版です。RDKit とは値が異なります。
- 本リポジトリのコードは、執筆環境にC++コンパイラが無いため
  **コンパイル検証を行っていません。** 環境に応じた微修正が必要な場合があります
  （そのときは付録Bのエラー辞典を活用してください——それも学習の一部です）。

## ライセンス

本書のサンプルコードは自由に利用・改変してください。
