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

### 公開: GitHub Pages

`main` に push すると GitHub Actions が自動でビルド・デプロイします
（[.github/workflows/deploy-pages.yml](.github/workflows/deploy-pages.yml)）。

**公開URL: https://lzh-function.github.io/cpp-manual/**

配信パスの検証をローカルで行う場合:

```bash
GH_PAGES=1 npm run build       # base を /cpp-manual/ にしてビルド
GH_PAGES=1 npm run preview     # http://localhost:4173/cpp-manual/
```

`preview` にも同じ環境変数が要ります（付け忘れるとサーバがルート配信になり、
`/cpp-manual/` 前提でビルドされたHTMLと噛み合わず全部404になります）。

> ⚠️ Git Bash で `DOCS_BASE=/cpp-manual/` を使うと MSYS のパス変換で
> `C:/Program Files/Git/...` に化けます。`GH_PAGES=1` を使ってください。

#### 検索エンジンへの掲載について

現在 `noindex, nofollow` を設定してあるため、**サイトは公開されていますが
Google などの検索結果には出ません**（URLを知っている人だけが辿り着けます）。

検索エンジンに載せたくなったら、`.vitepress/config.mts` の以下の行を削除してください:

```ts
['meta', { name: 'robots', content: 'noindex, nofollow, noarchive' }],
```

> なお、GitHub Pages に**アクセス制限はかけられません**（個人アカウントの場合）。
> 本当に閲覧者を限定したい場合は Cloudflare Pages + Cloudflare Access が必要です。

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

## 関連

C版もあります → **[PythonのためのC拡張入門](https://lzh-function.github.io/c-manual/)**
（[リポジトリ](https://github.com/Lzh-Function/c-manual)）

同じ読者層に向けた本ですが、軸が違います。

| | 本書（C++版） | [C版](https://lzh-function.github.io/c-manual/) |
|---|---|---|
| 軸 | **速さ**（メモリレイアウトとアルゴリズム） | **境界の設計**（PythonとCの繋ぎ方） |
| データ構造 | STL がある | 自作する |
| Pythonとの接続 | pybind11（第26章） | **ctypes / cffi が主題**（第III部） |
| ABI | コンパイラ・バージョンを揃える必要 | **安定。wheel を1つにできる** |
| 分量 | 約26,000行 | 約18,000行 |

用途で使い分けてください:

- **STLが欲しい / 既存C++資産（RDKitなど）がある / 大きなコードを書く** → 本書
- **境界を薄くしたい / 依存を減らしたい / 他言語からも呼びたい** → C版

---

*Happy compiling.*
