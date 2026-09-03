import { defineConfig } from 'vitepress'

// 配信パスの決定。
//
//   ローカル / Cloudflare Pages など        → '/'          (既定)
//   GitHub Pages のプロジェクトサイト        → '/cpp-manual/'  (GH_PAGES=1)
//
// ※ DOCS_BASE で明示指定もできるが、Git Bash では '/cpp-manual/' のような
//    値が MSYS のパス変換で 'C:/Program Files/Git/...' に化けるため、
//    通常は GH_PAGES=1 を使うこと。
const REPO_BASE = '/cpp-manual/'
const base = process.env.DOCS_BASE ?? (process.env.GH_PAGES ? REPO_BASE : '/')

/**
 * 日本語（CJK）対応のトークナイザ。
 *
 * VitePress の既定トークナイザは空白と句読点で切るだけなので、
 * 「ムーブセマンティクス」が1トークンになり「ムーブ」で検索しても
 * ヒットしない。そこで CJK の連続部分は 1〜2 文字の N-gram に分解して
 * 部分一致できるようにする。
 */
function tokenize(text: string): string[] {
  const tokens: string[] = []
  // 空白・句読点で大まかに分割（ラテン文字はこれで十分）
  for (const chunk of text.split(/[\s\n\r\t,.;:!?'"`()[\]{}<>/\\|=+*&^%$#@~—–…、。「」『』（）【】〔〕・：；！？]+/u)) {
    if (!chunk) continue
    tokens.push(chunk.toLowerCase())

    // CJK を含むなら N-gram も足す
    if (/[぀-ヿ㐀-䶿一-鿿豈-﫿]/.test(chunk)) {
      const chars = Array.from(chunk)
      for (let i = 0; i < chars.length; i++) {
        tokens.push(chars[i])
        if (i + 1 < chars.length) tokens.push(chars[i] + chars[i + 1])
        if (i + 2 < chars.length) tokens.push(chars[i] + chars[i + 1] + chars[i + 2])
      }
    }
  }
  return tokens
}

export default defineConfig({
  base,
  srcDir: 'book',
  outDir: '.vitepress/dist',
  cacheDir: '.vitepress/cache',

  lang: 'ja-JP',
  title: 'PythonケモインフォマティシャンのためのC++入門',
  titleTemplate: ':title | C++入門',
  description:
    'Pythonで深層学習・ケモインフォマティクスの研究をしている人が、C++を実務で使えるレベルまで持っていくための入門書。',

  // book/README.md をトップページとして配信する
  rewrites: {
    'README.md': 'index.md',
  },

  lastUpdated: true,
  cleanUrls: true,
  ignoreDeadLinks: [
    // 本文中の GitHub 上のコードへのリンクは外部URL扱い
    /^https?:\/\//,
  ],

  head: [
    ['meta', { name: 'theme-color', content: '#3c8772' }],
    ['meta', { property: 'og:type', content: 'book' }],
    ['meta', { property: 'og:locale', content: 'ja_JP' }],
    // 検索エンジンに載せない（限定公開向け。公開したくなったら削除）
    ['meta', { name: 'robots', content: 'noindex, nofollow, noarchive' }],
  ],

  markdown: {
    lineNumbers: false,
    theme: { light: 'github-light', dark: 'github-dark' },
    // 見出しの自動リンク
    anchor: { level: [2, 3, 4] },
  },

  themeConfig: {
    logo: undefined,
    siteTitle: 'C++入門 for ケモインフォ',

    nav: [
      { text: 'はじめに', link: '/ch00-how-to-read' },
      {
        text: '各部',
        items: [
          { text: '第I部 なぜC++か', link: '/ch01-why-cpp' },
          { text: '第II部 言語の核心', link: '/ch04-types-and-variables' },
          { text: '第III部 実践ケモインフォ', link: '/ch19-project-and-cmake' },
          { text: '第IV部 付録', link: '/appendix-a-cheatsheet' },
        ],
      },
      {
        text: 'よく引くページ',
        items: [
          { text: 'Python→C++ チートシート', link: '/appendix-a-cheatsheet' },
          { text: 'コンパイルエラー辞典', link: '/appendix-b-error-dictionary' },
          { text: '用語集', link: '/appendix-c-glossary' },
        ],
      },
    ],

    sidebar: [
      {
        text: '第I部 なぜC++か、そして最初の一歩',
        collapsed: false,
        items: [
          { text: '第0章 この本の読み方', link: '/ch00-how-to-read' },
          { text: '第1章 なぜPython使いがC++を学ぶのか', link: '/ch01-why-cpp' },
          { text: '第2章 環境構築', link: '/ch02-setup' },
          { text: '第3章 Hello, Molecule', link: '/ch03-hello-molecule' },
        ],
      },
      {
        text: '第II部 言語の核心',
        collapsed: false,
        items: [
          { text: '第4章 型と変数', link: '/ch04-types-and-variables' },
          { text: '第5章 制御構文と関数', link: '/ch05-control-flow-and-functions' },
          { text: '第6章 メモリモデル ★重要', link: '/ch06-memory-model' },
          { text: '第7章 配列とstd::vector', link: '/ch07-vector-and-arrays' },
          { text: '第8章 文字列', link: '/ch08-strings' },
          { text: '第9章 STLコンテナ完全ガイド', link: '/ch09-stl-containers' },
          { text: '第10章 イテレータとアルゴリズム', link: '/ch10-iterators-and-algorithms' },
          { text: '第11章 クラスとRAII', link: '/ch11-classes-and-raii' },
          { text: '第12章 コピーとムーブ', link: '/ch12-copy-and-move' },
          { text: '第13章 スマートポインタ', link: '/ch13-smart-pointers' },
          { text: '第14章 継承と多態性', link: '/ch14-inheritance-and-polymorphism' },
          { text: '第15章 テンプレート入門', link: '/ch15-templates' },
          { text: '第16章 ラムダと関数オブジェクト', link: '/ch16-lambdas' },
          { text: '第17章 モダンC++道具箱', link: '/ch17-modern-cpp-toolbox' },
          { text: '第18章 エラー処理', link: '/ch18-error-handling' },
        ],
      },
      {
        text: '第III部 実践ケモインフォマティクス',
        collapsed: false,
        items: [
          { text: '第19章 プロジェクトとCMake', link: '/ch19-project-and-cmake' },
          { text: '第20章 実践1: SMILESパーサ', link: '/ch20-smiles-parser' },
          { text: '第21章 実践2: 分子グラフとECFP', link: '/ch21-molecular-graph-and-ecfp' },
          { text: '第22章 実践3: ビットベクタとTanimoto', link: '/ch22-bitvector-and-tanimoto' },
          { text: '第23章 実践4: 100万化合物の高速検索', link: '/ch23-large-scale-search' },
          { text: '第24章 実践5: 記述子とSDF入出力', link: '/ch24-descriptors-and-sdf' },
          { text: '第25章 並列化', link: '/ch25-parallelism' },
          { text: '第26章 Pythonから使う (pybind11)', link: '/ch26-pybind11' },
          { text: '第27章 RDKitのC++ API', link: '/ch27-rdkit-cpp-api' },
          { text: '第28章 プロファイリングと最適化', link: '/ch28-profiling-and-optimization' },
          { text: '第29章 テスト・デバッグ・サニタイザ', link: '/ch29-testing-and-debugging' },
        ],
      },
      {
        text: '第IV部 付録',
        collapsed: false,
        items: [
          { text: '付録A Python→C++ チートシート', link: '/appendix-a-cheatsheet' },
          { text: '付録B コンパイルエラー辞典', link: '/appendix-b-error-dictionary' },
          { text: '付録C 用語集', link: '/appendix-c-glossary' },
          { text: '付録D 練習問題と次の一歩', link: '/appendix-d-solutions' },
        ],
      },
    ],

    outline: {
      level: [2, 3],
      label: 'このページの内容',
    },

    docFooter: {
      prev: '前の章',
      next: '次の章',
    },

    returnToTopLabel: 'トップへ戻る',
    sidebarMenuLabel: '目次',
    darkModeSwitchLabel: 'テーマ',
    lightModeSwitchTitle: 'ライトモードに切り替え',
    darkModeSwitchTitle: 'ダークモードに切り替え',
    externalLinkIcon: true,

    lastUpdatedText: '最終更新',

    socialLinks: [
      { icon: 'github', link: 'https://github.com/Lzh-Function/cpp-manual' },
    ],

    footer: {
      message: 'サンプルコードは自由に利用・改変してください。',
      copyright: 'PythonケモインフォマティシャンのためのC++入門',
    },

    search: {
      provider: 'local',
      options: {
        miniSearch: {
          options: {
            tokenize,
          },
          searchOptions: {
            fuzzy: 0.2,
            prefix: true,
            boost: { title: 4, text: 2, titles: 1 },
          },
        },
        translations: {
          button: {
            buttonText: '検索',
            buttonAriaLabel: '検索',
          },
          modal: {
            displayDetails: '詳細を表示',
            resetButtonTitle: 'リセット',
            backButtonTitle: '戻る',
            noResultsText: '見つかりませんでした:',
            footer: {
              selectText: '選択',
              selectKeyAriaLabel: 'Enter',
              navigateText: '移動',
              navigateUpKeyAriaLabel: '上',
              navigateDownKeyAriaLabel: '下',
              closeText: '閉じる',
              closeKeyAriaLabel: 'Esc',
            },
          },
        },
      },
    },
  },
})
