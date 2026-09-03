# 第3章 Hello, Molecule — コンパイルとは何か

## 3.1 最初のプログラムを解剖する

```cpp
// code/ch03/hello.cpp
#include <iostream>

int main() {
    std::cout << "Hello, Molecule!\n";
    return 0;
}
```

Pythonなら1行です。

```python
print("Hello, Molecule!")
```

この差分にC++の全てが詰まっているので、1行ずつ潰していきます。

### `#include <iostream>`

**Pythonの `import` に似ていますが、仕組みは全く違います。**

```python
import sys          # Python: 実行時にモジュールを探して読み込む
```

```cpp
#include <iostream>  // C++: コンパイル前に、そのファイルの中身をここに貼り付ける
```

`#` で始まる行は**プリプロセッサ指令**といって、コンパイル本体が始まる前に処理されます。
`#include <iostream>` は文字通り、`iostream` というファイルの中身を
**この位置にコピペする**という意味です。

実際に見てみましょう:

```bash
g++ -E -std=c++20 hello.cpp | wc -l
```

`-E` は「プリプロセスだけして止まれ」というオプション。結果は**約4万行**。
たった5行のプログラムが、`iostream` の中身を取り込んで4万行に膨れ上がります。

> ⚠️ **これがC++のコンパイルが遅い理由です。**
> ヘッダを include するたびに、その中身が丸ごと展開されます。
> 100個の `.cpp` ファイルが同じヘッダを include していたら、**100回展開されます**。
>
> C++20の **モジュール** (`import std;`) がこれを解決しますが、
> 2026年現在まだ対応が不完全なので、本書では従来の `#include` を使います。

`< >` と `" "` の違い:

```cpp
#include <vector>        // 標準ライブラリやシステムのディレクトリから探す
#include "molecule.hpp"  // まず自分のファイルと同じディレクトリから探す
```

自作ヘッダは `" "`、標準・外部ライブラリは `< >` と覚えてください。

### `int main()`

**プログラムの入口**です。C++では、`main` という名前の関数が必ず1つ必要で、
実行するとここから始まります。

```python
# Python: ファイルの先頭から順に実行される
if __name__ == "__main__":
    main()
```

```cpp
// C++: main が呼ばれる。他の場所に書いたコードは、main から呼ばれない限り動かない
int main() { ... }
```

`int` は「この関数は整数を返す」という意味。返す値は**終了コード**で、
`0` が成功、それ以外がエラーを表します（シェルの `echo $?` で見えるアレ）。

```cpp
int main() {
    return 0;   // 成功
}
```

> 💡 `main` に限り、`return 0;` は**省略できます**。書かなくても0を返したことになります。
> ただし本書では明示的に書きます（他の関数では省略できません）。

コマンドライン引数を受け取る形もあります:

```cpp
int main(int argc, char* argv[]) {
    // argc: 引数の個数（プログラム名を含む）
    // argv: 引数の配列
    // Python の sys.argv とほぼ同じ
}
```

```python
import sys
# sys.argv[0] がプログラム名、以降が引数 = argv と同じ
```

### `std::cout << "Hello, Molecule!\n";`

`std::cout` は **標準出力ストリーム**（Pythonの `sys.stdout`）。
`<<` は「ストリームに流し込む」演算子です。

```python
print("Hello")
print("x =", x, "y =", y)
```

```cpp
std::cout << "Hello\n";
std::cout << "x = " << x << " y = " << y << "\n";
```

`<<` は数珠つなぎにできます。左から右に流れていくイメージです。

```
std::cout << "x = " << x << " y = " << y << "\n";
    |         |        |       |       |     |
   出口     文字列    変数   文字列   変数  改行
```

> ⚠️ **`std::endl` は使わないでください**
>
> 古い教科書は `std::cout << "Hello" << std::endl;` と書きますが、
> `std::endl` は「改行 + **バッファのフラッシュ**」です。
> フラッシュはOSへのシステムコールを伴うので**とても遅い**。
>
> ループの中で100万回 `std::endl` を使うと、`"\n"` の**数十倍**遅くなります。
> **改行したいだけなら `"\n"` を使ってください。**

### `std::` は何なのか

`std` は **名前空間 (namespace)** です。Pythonのモジュール名に相当します。

```python
import numpy as np
np.array([1,2,3])      # np. が名前空間
```

```cpp
std::vector<int> v;    // std:: が名前空間
```

標準ライブラリの中身は全部 `std` 名前空間に入っています。
`::` は「〜の中の」という意味の演算子（スコープ解決演算子）。

> ⚠️ **`using namespace std;` は書かないでください**
>
> 多くの入門書が最初に教えますが、これは
> ```python
> from numpy import *
> ```
> と同じで、名前の衝突を引き起こします。
>
> 実例: 標準には `std::count`, `std::size`, `std::data`, `std::begin` などが
> あります。あなたが `count` という変数を作った瞬間に曖昧になり、
> **300行のテンプレートエラー**が出ます。
>
> `std::` の5文字を惜しまないでください。読む人にも「これは標準ライブラリだ」と伝わります。
>
> 関数の中でだけ限定的に使うのはOKです:
> ```cpp
> void f() {
>     using std::sqrt;   // この関数の中だけ
>     double x = sqrt(2.0);
> }
> ```

### セミコロン `;`

C++では**文の終わりにセミコロンが必要**です。Pythonの改行に相当します。

```python
a = 1
b = 2
```

```cpp
int a = 1;
int b = 2;
```

逆に言うと、**改行に意味がありません**。これは1行に書いても同じです:

```cpp
int a = 1; int b = 2;
```

インデントにも意味がありません。ブロックは `{ }` で表します。

```python
if x > 0:
    print("positive")
    print("indeed")
```

```cpp
if (x > 0) {
    std::cout << "positive\n";
    std::cout << "indeed\n";
}
```

> 🐍 **Pythonとの対比**
>
> Pythonのインデント = C++の `{ }`
> Pythonの改行 = C++の `;`
>
> C++でもインデントは**必ず**つけてください。意味はありませんが、
> 人間が読めなくなります。エディタの自動整形（clang-format）に任せるのが楽です。

---

## 3.2 コンパイルの4段階

`g++ hello.cpp -o hello` で何が起きているのか。実は4つの工程があります。

```
  hello.cpp
     │
     │ ① プリプロセス (cpp)      #include を展開、#define を置換
     ▼
  hello.i            <- 約4万行のC++コード
     │
     │ ② コンパイル (cc1plus)    C++ → アセンブリ言語
     ▼
  hello.s            <- 人間が（頑張れば）読めるCPU命令
     │
     │ ③ アセンブル (as)         アセンブリ → 機械語（バイナリ）
     ▼
  hello.o            <- オブジェクトファイル。まだ実行できない
     │
     │ ④ リンク (ld)             複数の .o と標準ライブラリを結合
     ▼
  hello              <- 実行ファイル！
```

実際に各段階を見てみましょう:

```bash
g++ -E hello.cpp -o hello.i     # ①だけ
g++ -S hello.cpp -o hello.s     # ①②
g++ -c hello.cpp -o hello.o     # ①②③
g++ hello.cpp -o hello          # ①②③④（全部）
```

`hello.s` を覗くと、こんなアセンブリが見えます:

```asm
main:
        push    rbp
        mov     rbp, rsp
        mov     esi, OFFSET FLAT:.LC0     ; "Hello, Molecule!\n" のアドレス
        mov     edi, OFFSET FLAT:_ZSt4cout
        call    _ZStlsISt11char_traitsIcEERSt13basic_ostream...
        mov     eax, 0
        pop     rbp
        ret
```

`_ZStlsISt11char_traits...` という呪文は **マングリングされた名前**です。
C++はオーバーロード（同名で引数違いの関数）を許すので、
コンパイラが引数の型を名前に埋め込んでユニークにします。

> 💡 リンクエラーで `undefined reference to '_ZN8Molecule5parseE...'` みたいな
> 呪文が出たら、`c++filt` で人間語に戻せます:
> ```bash
> echo '_ZN8Molecule5parseERKSs' | c++filt
> # Molecule::parse(std::string const&)
> ```
> 付録Bで詳しくやります。

### なぜ「コンパイル」と「リンク」を分けるのか

100個のファイルからなるプロジェクトで、1ファイルだけ直したとします。

- **分けない場合**: 100ファイル全部コンパイルし直し（10分）
- **分ける場合**: 直した1ファイルだけコンパイル（5秒）+ リンク（3秒）

これが分割コンパイルの威力です。`make` や `ninja` は、
「どのファイルが変更されたか」を見て必要な分だけ再コンパイルします。

---

## 3.3 複数ファイルに分ける

実際のプロジェクトはファイルが分かれます。ここが最初の関門です。

### 3ファイル構成の例

```cpp
// code/ch03/molecule.hpp   <- ヘッダ（宣言。「目次」）
#pragma once
#include <string>

// 「こういう関数がどこかにあります」という宣言だけ
int count_heavy_atoms(const std::string& smiles);
```

```cpp
// code/ch03/molecule.cpp   <- 実装（定義。「本文」）
#include "molecule.hpp"
#include <cctype>

int count_heavy_atoms(const std::string& smiles) {
    int n = 0;
    for (char c : smiles) {
        // 大文字の元素記号を数える（超簡易版）
        if (std::isupper(static_cast<unsigned char>(c))) ++n;
    }
    return n;
}
```

```cpp
// code/ch03/main.cpp
#include "molecule.hpp"
#include <iostream>

int main() {
    std::string aspirin = "CC(=O)Oc1ccccc1C(=O)O";
    std::cout << "heavy atoms (approx) = "
              << count_heavy_atoms(aspirin) << "\n";
    return 0;
}
```

ビルド:

```bash
g++ -std=c++20 -O2 main.cpp molecule.cpp -o mol
./mol
```

分割ビルド（実務ではこちら）:

```bash
g++ -std=c++20 -O2 -c molecule.cpp -o molecule.o
g++ -std=c++20 -O2 -c main.cpp     -o main.o
g++ molecule.o main.o -o mol
```

### なぜヘッダと実装を分けるのか

C++のコンパイラは**1ファイルずつ独立にコンパイル**します。
`main.cpp` をコンパイルしているとき、コンパイラは `molecule.cpp` の存在を知りません。

だから `main.cpp` の中で `count_heavy_atoms(...)` を呼びたければ、
「そういう名前で、`std::string` を受け取って `int` を返す関数がある」と
**事前に教えておく必要**があります。それがヘッダの役割です。

```
   main.cpp                molecule.cpp
      │                        │
   #include "molecule.hpp"  #include "molecule.hpp"
      │                        │
      ▼                        ▼
  「関数がある」と知る      「関数を実装する」
      │                        │
   main.o                  molecule.o
   （呼び出し先は空欄）      （実体がある）
      └──────────┬─────────────┘
                 ▼
              リンカが穴埋め
                 │
                 ▼
               mol（実行ファイル）
```

> 🐍 **Pythonとの対比**
>
> ヘッダ (`.hpp`) は **型スタブ (`.pyi`)** に相当します。
> 「シグネチャだけ書いた宣言ファイル」です。
>
> Pythonでは実行時に `import` で本体を読むので `.pyi` は任意ですが、
> C++では**必須**です。コンパイラが型を知らないと機械語を生成できないからです。

### `#pragma once` とは

同じヘッダが2回 include されるのを防ぐおまじないです。

```cpp
// A.hpp が B.hpp を include し、main.cpp が両方 include すると…
// A.hpp の中身が2回展開されて「二重定義」エラーになる
```

`#pragma once` を書いておくと、コンパイラが「このファイルは1回だけ」と覚えます。

古いコードでは **インクルードガード**という書き方も見ます（意味は同じ）:

```cpp
#ifndef MOLECULE_HPP
#define MOLECULE_HPP
// ... 中身 ...
#endif
```

`#pragma once` は厳密には規格外ですが、**主要コンパイラすべてが対応**しています。
本書では `#pragma once` を使います。短くてタイプミスしないからです。

### ヘッダに何を書き、何を書かないか

| ヘッダ (`.hpp`) に書く | 実装 (`.cpp`) に書く |
|---|---|
| 関数の**宣言**（`int f(int);`） | 関数の**定義**（本体 `{ ... }`） |
| クラスの定義（メンバの一覧） | メンバ関数の実装 |
| `inline` 関数、`constexpr` 関数 | 内部専用のヘルパ関数 |
| テンプレート（全部書く必要がある） | — |
| `using` 型エイリアス | — |
| 定数 (`inline constexpr`) | — |

> ⚠️ **ヘッダに書いてはいけないもの**
> - `using namespace std;`（そのヘッダを include した全員に感染します）
> - 普通の変数定義（`int g_count;` → 二重定義エラー。`inline int g_count;` ならOK）
> - 巨大な実装（コンパイルが遅くなる）

---

## 3.4 コメントと基本の書式

```cpp
// 1行コメント（Pythonの # と同じ）

/*
   複数行コメント
   （Pythonの """ ... """ に近い）
*/

/**
 * Doxygen形式のドキュメンテーションコメント
 * @param smiles SMILES文字列
 * @return 重原子数
 */
int count_heavy_atoms(const std::string& smiles);
```

Pythonのdocstringに相当するものは言語機能としてはありませんが、
**Doxygen** という慣習的な書式があり、ツールでHTMLドキュメントを生成できます。

---

## 3.5 実践: 分子の情報を表示するプログラム

ここまでの知識で書ける、少し実用的なプログラムです。

```cpp
// code/ch03/mol_info.cpp
#include <iostream>
#include <string>
#include <vector>

int main() {
    // 分子の名前とSMILES のペア
    std::vector<std::string> names = {
        "Ethanol", "Benzene", "Aspirin", "Caffeine"
    };
    std::vector<std::string> smiles = {
        "CCO",
        "c1ccccc1",
        "CC(=O)Oc1ccccc1C(=O)O",
        "Cn1cnc2c1c(=O)n(C)c(=O)n2C"
    };

    std::cout << "Name        SMILES                        Len  Rings\n";
    std::cout << "----------  ----------------------------  ---  -----\n";

    for (std::size_t i = 0; i < names.size(); ++i) {
        // 環の数を、数字の出現回数から雑に推定（実際は第20章で正しくやります）
        int ring_digits = 0;
        for (char c : smiles[i]) {
            if (c >= '1' && c <= '9') ++ring_digits;
        }

        std::cout << names[i];
        // 手動で桁揃え（後で std::format を使う方法を教えます）
        for (std::size_t k = names[i].size(); k < 12; ++k) std::cout << ' ';
        std::cout << smiles[i];
        for (std::size_t k = smiles[i].size(); k < 30; ++k) std::cout << ' ';
        std::cout << smiles[i].size() << "    " << ring_digits / 2 << "\n";
    }
    return 0;
}
```

```bash
g++ -std=c++20 -O2 -Wall mol_info.cpp -o mol_info
./mol_info
```

```
Name        SMILES                        Len  Rings
----------  ----------------------------  ---  -----
Ethanol     CCO                           3    0
Benzene     c1ccccc1                      8    1
Aspirin     CC(=O)Oc1ccccc1C(=O)O         21   1
Caffeine    Cn1cnc2c1c(=O)n(C)c(=O)n2C    26   2
```

桁揃えのループが醜いですね。C++20の `std::format` を使えばこうなります:

```cpp
#include <format>
// ...
std::cout << std::format("{:<12}{:<30}{:>3}  {:>5}\n",
                          names[i], smiles[i], smiles[i].size(), ring_digits/2);
```

Pythonのf-string / `str.format` とほぼ同じ書式です。第17章で詳しくやります。

> ⚠️ `std::format` は GCC 13以上 / Clang 17以上 / MSVC 2019 16.10以上が必要です。
> 使えない場合は [fmt](https://github.com/fmtlib/fmt) ライブラリで
> `fmt::format` を使ってください（APIはほぼ同一）。

---

## 3.6 よくある最初のエラー

### `error: expected ';' before ...`

セミコロン忘れです。**エラーが指す行の1つ上**を見てください。

```cpp
int a = 1     // ← ここ
int b = 2;    // ← エラーはここに出る
```

### `error: 'cout' was not declared in this scope`

`std::` を付け忘れ、または `#include <iostream>` 忘れ。

```cpp
cout << "x";       // ✗
std::cout << "x";  // ✓
```

### `undefined reference to 'count_heavy_atoms(std::string const&)'`

**リンクエラー**です。コンパイルは通ったが、実装が見つかりません。

原因の9割:
1. `molecule.cpp` をコンパイル対象に含め忘れた
   → `g++ main.cpp molecule.cpp -o mol`
2. ヘッダの宣言と実装のシグネチャが違う
   → `const std::string&` と `std::string` は**別の関数**として扱われます

### `error: redefinition of 'class Molecule'`

ヘッダに `#pragma once` を書き忘れています。

### 実行はできるが何も表示されない

`main` の中に書き忘れているか、出力後すぐウィンドウが閉じています。
Windowsでダブルクリック起動している場合は、ターミナルから実行してください。

---

## 3.7 この章のまとめ

- `#include` は「ファイルの中身をここに貼る」プリプロセッサ指令
- `main` がプログラムの入口。`int` を返し、0が成功
- `std::cout << x` で出力。**`std::endl` ではなく `"\n"` を使う**
- **`using namespace std;` は書かない**
- 文の終わりに `;`、ブロックは `{ }`、インデントに意味はない
- コンパイルは **プリプロセス → コンパイル → アセンブル → リンク** の4段階
- ヘッダ (`.hpp`) は宣言、実装 (`.cpp`) は定義。ヘッダには `#pragma once`
- `undefined reference` はリンクエラー = 「宣言はあるが実体がない」

> 📝 **練習問題 3-1**
>
> `mol_info.cpp` を改造して、SMILES中の芳香族原子（小文字の `c`, `n`, `o`, `s`）の
> 個数も表示するようにしてください。

> 📝 **練習問題 3-2**
>
> `count_heavy_atoms` を `molecule.hpp` / `molecule.cpp` / `main.cpp` に分けた状態で、
> わざと `molecule.cpp` をコンパイル対象から外してみてください。
> どんなエラーが出ますか? そのメッセージを覚えておいてください。
> 今後、何百回も見ることになります。

> 📝 **練習問題 3-3**
>
> `g++ -E hello.cpp | wc -l` で展開後の行数を数え、
> `#include <iostream>` を `#include <vector>` に変えると何行になるか比べてください。
> どのヘッダが重いか感覚をつかんでおくと、後でコンパイル時間の最適化に効きます。

---

→ [第4章 型と変数](ch04-types-and-variables.md)
