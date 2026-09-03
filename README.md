# PythonケモインフォマティシャンのためのC++入門

Pythonで深層学習・ケモインフォマティクスの研究をしている人が、
C++を**実務で使えるレベル**まで持っていくための入門書です。

## 📖 [本編はこちら → book/README.md](book/README.md)

---

## 書籍サイトとして読む（推奨）

VitePress で静的サイトとして読めます。サイドバー目次・**日本語全文検索**・
ダークモード・前後の章へのナビゲーションが付きます。

```bash
npm install      # 初回のみ
npm run dev      # http://localhost:5173 が開く
```

本番ビルドとローカルプレビュー:

```bash
npm run build    # .vitepress/dist/ に静的サイトを生成
npm run preview  # http://localhost:4173 で確認
```

> 検索は「ムーブ」「popcount」のような部分語でヒットします
> （CJK用のN-gramトークナイザを `.vitepress/config.mts` に実装済み）。

### 公開する場合

現状は **ローカル閲覧のみ**の設定です（`noindex` メタタグ入り）。公開先の候補:

| 方法 | 限定公開 | 手順 |
|---|---|---|
| **Cloudflare Pages + Access** | ✅ できる | ビルド `npm run build` / 出力 `.vitepress/dist` を設定し、Zero Trust で許可メールを登録 |
| **GitHub Pages** | ❌ できない | `DOCS_BASE=/cpp-manual/ npm run build` して `.vitepress/dist` を Pages に配置。個人アカウントではサイトは常に全世界公開 |

GitHub Pages のアクセス制御は GitHub Enterprise Cloud 専用機能のため、
個人アカウントでは「Pages かつ限定公開」は選べません。

---

## 構成

```
cpp-manual/
├── .vitepress/config.mts        サイト設定（目次・検索・テーマ）
├── package.json
└── book/
    ├── README.md                  ← 目次。ここから読み始めてください
    ├── ch00〜ch29                  本編30章
    ├── appendixA〜D                付録（チートシート、エラー辞典、用語集）
    └── code/                       サンプルコード
        ├── ch03〜ch12/             各章の単体サンプル
        └── chemcpp/                第III部で作るライブラリ（CMakeでビルド可）
```

## 何が書いてあるか

| 部 | 内容 |
|---|---|
| **第I部**（1〜3章） | なぜPythonは遅いのか、環境構築、コンパイルの仕組み |
| **第II部**（4〜18章） | 型・メモリモデル・vector・STL・クラス・RAII・ムーブ・スマートポインタ・テンプレート・ラムダ・エラー処理 |
| **第III部**（19〜29章） | CMake、SMILESパーサ、ECFP、Tanimoto検索、記述子、SDF I/O、並列化、**pybind11**、RDKit C++ API、プロファイリング、テスト |
| **第IV部**（付録） | Python→C++チートシート、コンパイルエラー辞典、用語集、練習問題の解答 |

## 到達点

```python
import chemcpp
fps = chemcpp.fingerprints(million_smiles)   # 0.6秒（RDKit Python: 380秒）
```

**Pythonの書き心地のまま、100〜600倍。** これが本書のゴールです。

## サンプルコードのビルド

```bash
cd book/code/chemcpp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/parse_demo
./build/fp_demo
./build/screen data/sample.smi "CC(=O)Oc1ccccc1C(=O)O" 0.4
```

> ⚠️ サンプルコードは執筆環境にC++コンパイラが無かったため**コンパイル検証をしていません**。
> エラーが出たら [付録B コンパイルエラー辞典](book/appendix-b-error-dictionary.md) を引いてください。

---

*Happy compiling.*
