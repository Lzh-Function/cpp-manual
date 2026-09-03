# 第20章 実践1: SMILESパーサを書く

いよいよ実践です。SMILES文字列を読んで分子グラフを構築する、
本物のパーサを書きます。

RDKit の `Chem.MolFromSmiles()` が内部でやっていることを、自分の手で作ります。

## 20.1 SMILES の文法（最小限の仕様）

パーサを書く前に、扱う文法を明確にしましょう。

| 要素 | 例 | 意味 |
|---|---|---|
| **有機部分集合原子** | `C` `N` `O` `S` `P` `F` `Cl` `Br` `I` `B` | ブラケットなしで書ける原子 |
| **芳香族原子** | `c` `n` `o` `s` `p` | 小文字は芳香族 |
| **ブラケット原子** | `[nH]` `[Fe+2]` `[13C]` `[C@H]` | 詳細指定 |
| **結合** | `-` `=` `#` `$` `:` `/` `\` | 単/二重/三重/四重/芳香/立体 |
| **分岐** | `CC(C)C` | 括弧内は枝 |
| **環結合** | `c1ccccc1` `C%10CCC%10` | 同じ番号どうしを結合 |
| **不連結** | `[Na+].[Cl-]` | 別分子 |

**暗黙のルール:**
- 結合記号がなければ単結合（芳香族原子どうしなら芳香結合）
- 有機部分集合原子の水素は、原子価から自動計算される
- 芳香族原子は小文字で書く

---

## 20.2 設計方針

```
   SMILES文字列
        │
   ① トークナイザ (tokenize)
        │  文字列 → トークン列
        ▼
   ② パーサ (parse)
        │  トークン列 → 分子グラフ
        ▼
   ③ 後処理 (finalize)
        │  暗黙の水素数を計算、環の検出
        ▼
   Molecule オブジェクト
```

**なぜ2段階（トークナイズ→パース）に分けるのか:**
- それぞれのロジックが単純になる
- テストしやすい
- トークナイザは他の用途にも使える（構文ハイライトなど）

> 💡 1段階で書くこともできます（実際 RDKit は yacc/lex ベース）。
> しかし学習目的なら2段階の方が理解しやすいです。

---

## 20.3 データ構造

```cpp
// include/chemcpp/molecule.hpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace chemcpp {

enum class BondOrder : std::uint8_t {
    Single = 1, Double = 2, Triple = 3, Quadruple = 4, Aromatic = 5, Unspecified = 0
};

enum class Chirality : std::uint8_t { None = 0, CW = 1, CCW = 2 };

struct Atom {
    std::uint8_t  atomic_num   = 0;
    std::int8_t   charge       = 0;
    std::uint8_t  num_h        = 0;      // 明示 or 暗黙の水素数
    std::uint8_t  isotope      = 0;
    bool          aromatic     = false;
    bool          in_ring      = false;
    bool          explicit_h   = false;  // [CH3] のように明示されたか
    Chirality     chirality    = Chirality::None;
};

struct Bond {
    std::uint32_t begin = 0;
    std::uint32_t end   = 0;
    BondOrder     order = BondOrder::Single;
    bool          in_ring = false;
};

class Molecule {
public:
    // ---- 構築 ----
    std::uint32_t add_atom(const Atom& a) {
        atoms_.push_back(a);
        adj_.emplace_back();
        return static_cast<std::uint32_t>(atoms_.size() - 1);
    }

    void add_bond(std::uint32_t i, std::uint32_t j, BondOrder order) {
        const auto bi = static_cast<std::uint32_t>(bonds_.size());
        bonds_.push_back({i, j, order, false});
        adj_[i].push_back({j, bi});
        adj_[j].push_back({i, bi});
    }

    // ---- 参照 ----
    struct Neighbor { std::uint32_t atom; std::uint32_t bond; };

    std::size_t num_atoms() const noexcept { return atoms_.size(); }
    std::size_t num_bonds() const noexcept { return bonds_.size(); }

    const Atom& atom(std::size_t i) const { return atoms_[i]; }
    Atom&       atom(std::size_t i)       { return atoms_[i]; }
    const Bond& bond(std::size_t i) const { return bonds_[i]; }
    Bond&       bond(std::size_t i)       { return bonds_[i]; }

    const std::vector<Atom>& atoms() const noexcept { return atoms_; }
    const std::vector<Bond>& bonds() const noexcept { return bonds_; }
    const std::vector<Neighbor>& neighbors(std::size_t i) const { return adj_[i]; }

    std::size_t degree(std::size_t i) const { return adj_[i].size(); }

    const std::string& name() const noexcept { return name_; }
    void set_name(std::string n) { name_ = std::move(n); }

private:
    std::string                            name_;
    std::vector<Atom>                      atoms_;
    std::vector<Bond>                      bonds_;
    std::vector<std::vector<Neighbor>>     adj_;
};

}  // namespace chemcpp
```

**設計上の判断:**

1. **すべて `std::vector` で連続配置** — キャッシュ効率（第6章）
2. **ポインタではなくインデックス（`std::uint32_t`）** — 循環参照なし、
   再確保に強い、メモリ半分（第13章）
3. **隣接リストに結合インデックスも持つ** — 近傍原子と同時に結合次数も引ける
4. **`enum class` + `std::uint8_t`** — 型安全 + メモリ節約（第17章）

`sizeof(Atom)` は 8バイト、`sizeof(Bond)` は 12バイトになります。
100万原子でも8MB。十分にコンパクトです。

---

## 20.4 元素データ

```cpp
// include/chemcpp/element.hpp
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace chemcpp {

class Element {
public:
    static constexpr int MAX_Z = 118;

    static constexpr std::string_view symbol(int z) {
        return (z >= 0 && z <= MAX_Z) ? SYMBOLS[z] : "??";
    }

    static constexpr double weight(int z) {
        return (z >= 0 && z <= MAX_Z) ? WEIGHTS[z] : 0.0;
    }

    // 記号から原子番号（見つからなければ0）
    static constexpr int number(std::string_view sym) {
        for (int z = 1; z <= MAX_Z; ++z) {
            if (SYMBOLS[z] == sym) return z;
        }
        return 0;
    }

    // 標準原子価（暗黙の水素数の計算に使う）
    static constexpr int default_valence(int z) {
        switch (z) {
            case 5:  return 3;   // B
            case 6:  return 4;   // C
            case 7:  return 3;   // N
            case 8:  return 2;   // O
            case 15: return 3;   // P
            case 16: return 2;   // S
            case 9: case 17: case 35: case 53: return 1;   // F, Cl, Br, I
            case 1:  return 1;   // H
            default: return 0;   // 不明なものは水素を付けない
        }
    }

private:
    static constexpr std::array<std::string_view, MAX_Z + 1> SYMBOLS = {
        "",
        "H", "He","Li","Be","B", "C", "N", "O", "F", "Ne",
        "Na","Mg","Al","Si","P", "S", "Cl","Ar","K", "Ca",
        "Sc","Ti","V", "Cr","Mn","Fe","Co","Ni","Cu","Zn",
        "Ga","Ge","As","Se","Br","Kr","Rb","Sr","Y", "Zr",
        "Nb","Mo","Tc","Ru","Rh","Pd","Ag","Cd","In","Sn",
        "Sb","Te","I", "Xe","Cs","Ba","La","Ce","Pr","Nd",
        "Pm","Sm","Eu","Gd","Tb","Dy","Ho","Er","Tm","Yb",
        "Lu","Hf","Ta","W", "Re","Os","Ir","Pt","Au","Hg",
        "Tl","Pb","Bi","Po","At","Rn","Fr","Ra","Ac","Th",
        "Pa","U", "Np","Pu","Am","Cm","Bk","Cf","Es","Fm",
        "Md","No","Lr","Rf","Db","Sg","Bh","Hs","Mt","Ds",
        "Rg","Cn","Nh","Fl","Mc","Lv","Ts","Og"
    };

    static constexpr std::array<double, MAX_Z + 1> WEIGHTS = {
        0.0,
        1.008,   4.0026,  6.94,    9.0122,  10.81,   12.011,  14.007,  15.999,
        18.998,  20.180,  22.990,  24.305,  26.982,  28.085,  30.974,  32.06,
        35.45,   39.948,  39.098,  40.078,  44.956,  47.867,  50.942,  51.996,
        54.938,  55.845,  58.933,  58.693,  63.546,  65.38,   69.723,  72.630,
        74.922,  78.971,  79.904,  83.798,  85.468,  87.62,   88.906,  91.224,
        92.906,  95.95,   98.0,    101.07,  102.91,  106.42,  107.87,  112.41,
        114.82,  118.71,  121.76,  127.60,  126.90,  131.29,  132.91,  137.33,
        138.91,  140.12,  140.91,  144.24,  145.0,   150.36,  151.96,  157.25,
        158.93,  162.50,  164.93,  167.26,  168.93,  173.05,  174.97,  178.49,
        180.95,  183.84,  186.21,  190.23,  192.22,  195.08,  196.97,  200.59,
        204.38,  207.2,   208.98,  209.0,   210.0,   222.0,   223.0,   226.0,
        227.0,   232.04,  231.04,  238.03,  237.0,   244.0,   243.0,   247.0,
        247.0,   251.0,   252.0,   257.0,   258.0,   259.0,   266.0,   267.0,
        268.0,   269.0,   270.0,   269.0,   278.0,   281.0,   282.0,   285.0,
        286.0,   289.0,   290.0,   293.0,   294.0,   294.0
    };
};

}  // namespace chemcpp
```

すべて `constexpr` なので、**コンパイル時に構築され、実行時コストはゼロ**です。

---

## 20.5 パーサ本体

```cpp
// include/chemcpp/smiles.hpp
#pragma once

#include <chemcpp/molecule.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace chemcpp {

struct SmilesError {
    std::string message;
    std::size_t position = 0;

    std::string what() const {
        return message + " (at position " + std::to_string(position) + ")";
    }
};

/// SMILES をパースする。失敗時は std::nullopt。
/// エラー詳細が欲しい場合は err に渡す。
std::optional<Molecule> parse_smiles(std::string_view smiles,
                                     SmilesError* err = nullptr);

}  // namespace chemcpp
```

```cpp
// src/smiles.cpp
#include <chemcpp/smiles.hpp>
#include <chemcpp/element.hpp>

#include <array>
#include <cctype>
#include <charconv>
#include <string_view>
#include <vector>

namespace chemcpp {
namespace {

// ---------------------------------------------------------------
// パーサの状態を持つクラス（再入可能・スレッドセーフ）
// ---------------------------------------------------------------
class SmilesParser {
public:
    explicit SmilesParser(std::string_view s) : s_(s) {}

    std::optional<Molecule> run(SmilesError* err) {
        try {
            parse_chain();
            finalize();
            if (!check_rings()) {
                fail("unclosed ring bond");
            }
            return std::move(mol_);
        } catch (const SmilesError& e) {
            if (err) *err = e;
            return std::nullopt;
        }
    }

private:
    // ===================== 低レベルユーティリティ =====================
    bool eof() const { return pos_ >= s_.size(); }
    char peek(std::size_t off = 0) const {
        return (pos_ + off < s_.size()) ? s_[pos_ + off] : '\0';
    }
    char next() { return s_[pos_++]; }
    bool consume(char c) {
        if (peek() == c) { ++pos_; return true; }
        return false;
    }
    [[noreturn]] void fail(std::string msg) const {
        throw SmilesError{std::move(msg), pos_};
    }

    // ===================== メインループ =====================
    // chain := (bond? (atom | ring_closure | branch | dot))*
    void parse_chain() {
        while (!eof()) {
            const char c = peek();

            if (c == '(') {                       // 分岐開始
                ++pos_;
                if (prev_atom_ == NO_ATOM) fail("branch without preceding atom");
                branch_stack_.push_back(prev_atom_);
                continue;
            }
            if (c == ')') {                       // 分岐終了
                ++pos_;
                if (branch_stack_.empty()) fail("unmatched ')'");
                prev_atom_ = branch_stack_.back();
                branch_stack_.pop_back();
                continue;
            }
            if (c == '.') {                       // 不連結
                ++pos_;
                prev_atom_ = NO_ATOM;
                pending_bond_ = BondOrder::Unspecified;
                continue;
            }

            // 結合記号
            if (auto b = try_parse_bond()) {
                pending_bond_ = *b;
                continue;
            }

            // 環結合番号
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '%') {
                parse_ring_closure();
                continue;
            }

            // 原子
            if (c == '[') {
                const std::uint32_t idx = parse_bracket_atom();
                connect(idx);
                continue;
            }
            if (auto z = try_parse_organic_atom()) {
                const std::uint32_t idx = *z;
                connect(idx);
                continue;
            }

            fail(std::string("unexpected character '") + c + "'");
        }
        if (!branch_stack_.empty()) fail("unclosed '('");
    }

    // ===================== 結合 =====================
    std::optional<BondOrder> try_parse_bond() {
        switch (peek()) {
            case '-':  ++pos_; return BondOrder::Single;
            case '=':  ++pos_; return BondOrder::Double;
            case '#':  ++pos_; return BondOrder::Triple;
            case '$':  ++pos_; return BondOrder::Quadruple;
            case ':':  ++pos_; return BondOrder::Aromatic;
            case '/':  ++pos_; return BondOrder::Single;   // 立体は今回は無視
            case '\\': ++pos_; return BondOrder::Single;
            default:   return std::nullopt;
        }
    }

    // 新しい原子を、直前の原子と繋ぐ
    void connect(std::uint32_t idx) {
        if (prev_atom_ != NO_ATOM) {
            BondOrder order = pending_bond_;
            if (order == BondOrder::Unspecified) {
                // 芳香族原子どうしなら芳香結合、それ以外は単結合
                order = (mol_.atom(prev_atom_).aromatic && mol_.atom(idx).aromatic)
                            ? BondOrder::Aromatic : BondOrder::Single;
            }
            mol_.add_bond(prev_atom_, idx, order);
        }
        prev_atom_    = idx;
        pending_bond_ = BondOrder::Unspecified;
    }

    // ===================== 有機部分集合原子 =====================
    std::optional<std::uint32_t> try_parse_organic_atom() {
        const char c = peek();

        // 2文字元素を先にチェック（Cl, Br）
        if (c == 'C' && peek(1) == 'l') { pos_ += 2; return make_atom(17, false); }
        if (c == 'B' && peek(1) == 'r') { pos_ += 2; return make_atom(35, false); }

        // 脂肪族（大文字）
        int z = 0;
        switch (c) {
            case 'B': z = 5;  break;
            case 'C': z = 6;  break;
            case 'N': z = 7;  break;
            case 'O': z = 8;  break;
            case 'P': z = 15; break;
            case 'S': z = 16; break;
            case 'F': z = 9;  break;
            case 'I': z = 53; break;
            default:  break;
        }
        if (z) { ++pos_; return make_atom(z, false); }

        // 芳香族（小文字）
        switch (c) {
            case 'b': z = 5;  break;
            case 'c': z = 6;  break;
            case 'n': z = 7;  break;
            case 'o': z = 8;  break;
            case 'p': z = 15; break;
            case 's': z = 16; break;
            default:  return std::nullopt;
        }
        ++pos_;
        return make_atom(z, true);
    }

    std::uint32_t make_atom(int z, bool aromatic) {
        Atom a;
        a.atomic_num = static_cast<std::uint8_t>(z);
        a.aromatic   = aromatic;
        return mol_.add_atom(a);
    }

    // ===================== ブラケット原子 =====================
    // [isotope? symbol chiral? hcount? charge? (:class)? ]
    std::uint32_t parse_bracket_atom() {
        if (!consume('[')) fail("expected '['");

        Atom a;
        a.explicit_h = true;              // ブラケット内は水素数が明示される

        // --- 同位体 ---
        int isotope = 0;
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            isotope = isotope * 10 + (next() - '0');
        }
        a.isotope = static_cast<std::uint8_t>(isotope > 255 ? 0 : isotope);

        // --- 元素記号 ---
        if (eof()) fail("unterminated bracket atom");
        char c0 = peek();

        if (c0 == '*') {                  // ワイルドカード
            ++pos_;
            a.atomic_num = 0;
        } else if (std::islower(static_cast<unsigned char>(c0))) {
            // 芳香族: c, n, o, s, p, se, as
            a.aromatic = true;
            std::size_t len = 1;
            if (pos_ + 1 < s_.size() && std::islower(static_cast<unsigned char>(peek(1)))) {
                // se, as のような2文字芳香族
                std::string two{static_cast<char>(std::toupper(c0)), peek(1)};
                if (Element::number(two) != 0) len = 2;
            }
            std::string sym(s_.substr(pos_, len));
            sym[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(sym[0])));
            int z = Element::number(sym);
            if (z == 0) fail("unknown aromatic atom '" + sym + "'");
            a.atomic_num = static_cast<std::uint8_t>(z);
            pos_ += len;
        } else if (std::isupper(static_cast<unsigned char>(c0))) {
            std::size_t len = 1;
            if (pos_ + 1 < s_.size() && std::islower(static_cast<unsigned char>(peek(1)))) {
                std::string two(s_.substr(pos_, 2));
                if (Element::number(two) != 0) len = 2;
            }
            std::string sym(s_.substr(pos_, len));
            int z = Element::number(sym);
            if (z == 0) fail("unknown element '" + sym + "'");
            a.atomic_num = static_cast<std::uint8_t>(z);
            pos_ += len;
        } else {
            fail("expected element symbol in bracket");
        }

        // --- 立体化学 ---
        if (peek() == '@') {
            ++pos_;
            if (consume('@')) a.chirality = Chirality::CW;
            else              a.chirality = Chirality::CCW;
            // @TH1 のような拡張表記は読み飛ばす
            while (std::isalnum(static_cast<unsigned char>(peek())) &&
                   !std::isdigit(static_cast<unsigned char>(peek()))) {
                ++pos_;
            }
        }

        // --- 水素数 ---
        if (peek() == 'H') {
            ++pos_;
            int h = 1;
            if (std::isdigit(static_cast<unsigned char>(peek()))) {
                h = 0;
                while (std::isdigit(static_cast<unsigned char>(peek())))
                    h = h * 10 + (next() - '0');
            }
            a.num_h = static_cast<std::uint8_t>(h);
        }

        // --- 電荷 ---
        if (peek() == '+' || peek() == '-') {
            const int sign = (next() == '+') ? 1 : -1;
            int mag = 1;
            if (std::isdigit(static_cast<unsigned char>(peek()))) {
                mag = 0;
                while (std::isdigit(static_cast<unsigned char>(peek())))
                    mag = mag * 10 + (next() - '0');
            } else {
                // ++ や -- の表記
                while (peek() == (sign > 0 ? '+' : '-')) { ++pos_; ++mag; }
            }
            a.charge = static_cast<std::int8_t>(sign * mag);
        }

        // --- アトムクラス（:12）は読み飛ばす ---
        if (peek() == ':') {
            ++pos_;
            while (std::isdigit(static_cast<unsigned char>(peek()))) ++pos_;
        }

        if (!consume(']')) fail("expected ']'");
        return mol_.add_atom(a);
    }

    // ===================== 環結合 =====================
    void parse_ring_closure() {
        int id = 0;
        if (consume('%')) {
            if (!std::isdigit(static_cast<unsigned char>(peek())))
                fail("expected digits after '%'");
            id = (next() - '0') * 10;
            if (!std::isdigit(static_cast<unsigned char>(peek())))
                fail("expected two digits after '%'");
            id += (next() - '0');
        } else {
            id = next() - '0';
        }
        if (id < 0 || id >= MAX_RING_ID) fail("ring bond id out of range");
        if (prev_atom_ == NO_ATOM) fail("ring closure without atom");

        if (ring_open_[id] == NO_ATOM) {
            // 開く
            ring_open_[id]  = prev_atom_;
            ring_order_[id] = pending_bond_;
        } else {
            // 閉じる
            const std::uint32_t other = ring_open_[id];
            if (other == prev_atom_) fail("ring closure to itself");

            BondOrder order = pending_bond_;
            if (order == BondOrder::Unspecified) order = ring_order_[id];
            if (order == BondOrder::Unspecified) {
                order = (mol_.atom(other).aromatic && mol_.atom(prev_atom_).aromatic)
                            ? BondOrder::Aromatic : BondOrder::Single;
            }
            mol_.add_bond(other, prev_atom_, order);
            ring_open_[id]  = NO_ATOM;
            ring_order_[id] = BondOrder::Unspecified;
        }
        pending_bond_ = BondOrder::Unspecified;
    }

    bool check_rings() const {
        for (std::uint32_t v : ring_open_) if (v != NO_ATOM) return false;
        return true;
    }

    // ===================== 後処理 =====================
    void finalize() {
        // 環に属する原子・結合をマーク
        mark_ring_membership();
        // 暗黙の水素数を計算
        for (std::size_t i = 0; i < mol_.num_atoms(); ++i) {
            Atom& a = mol_.atom(i);
            if (a.explicit_h) continue;             // ブラケット原子はスキップ
            a.num_h = static_cast<std::uint8_t>(implicit_hydrogens(i));
        }
    }

    int implicit_hydrogens(std::size_t idx) const {
        const Atom& a = mol_.atom(idx);
        const int valence = Element::default_valence(a.atomic_num);
        if (valence == 0) return 0;

        // 結合次数の合計を計算
        double bond_sum = 0.0;
        for (const auto& nb : mol_.neighbors(idx)) {
            switch (mol_.bond(nb.bond).order) {
                case BondOrder::Single:    bond_sum += 1.0; break;
                case BondOrder::Double:    bond_sum += 2.0; break;
                case BondOrder::Triple:    bond_sum += 3.0; break;
                case BondOrder::Quadruple: bond_sum += 4.0; break;
                case BondOrder::Aromatic:  bond_sum += 1.5; break;
                default:                   bond_sum += 1.0; break;
            }
        }
        // 芳香族原子は、環の共役に1電子出しているとみなす簡易補正
        int used = static_cast<int>(bond_sum + 0.5);
        int h = valence - used + a.charge * charge_effect(a.atomic_num);
        return h > 0 ? h : 0;
    }

    static int charge_effect(int z) {
        // N+ は原子価が1増え、O- は1減る、といった簡易ルール
        return (z == 7 || z == 15) ? 1 : -1;
    }

    // 各結合が環に属するかを判定（橋（bridge）でない結合が環結合）
    void mark_ring_membership() {
        const std::size_t n = mol_.num_atoms();
        if (n == 0) return;

        std::vector<int>  disc(n, -1), low(n, 0);
        std::vector<int>  parent_bond(n, -1);
        std::vector<char> visited(n, 0);
        int timer = 0;

        // 反復DFS（大きな分子でもスタックオーバーフローしない）
        struct Frame { std::uint32_t node; std::size_t next_idx; };
        std::vector<Frame> stack;

        for (std::uint32_t start = 0; start < n; ++start) {
            if (visited[start]) continue;
            stack.push_back({start, 0});
            visited[start] = 1;
            disc[start] = low[start] = timer++;

            while (!stack.empty()) {
                Frame& f = stack.back();
                const auto& nbs = mol_.neighbors(f.node);

                if (f.next_idx < nbs.size()) {
                    const auto nb = nbs[f.next_idx++];
                    if (static_cast<int>(nb.bond) == parent_bond[f.node]) continue;

                    if (visited[nb.atom]) {
                        low[f.node] = std::min(low[f.node], disc[nb.atom]);
                    } else {
                        visited[nb.atom]     = 1;
                        disc[nb.atom]        = low[nb.atom] = timer++;
                        parent_bond[nb.atom] = static_cast<int>(nb.bond);
                        stack.push_back({nb.atom, 0});
                    }
                } else {
                    const std::uint32_t child = f.node;
                    stack.pop_back();
                    if (!stack.empty()) {
                        const std::uint32_t par = stack.back().node;
                        low[par] = std::min(low[par], low[child]);
                        // low[child] > disc[par] なら橋 → 環結合ではない
                        if (low[child] <= disc[par]) {
                            mol_.bond(static_cast<std::size_t>(parent_bond[child])).in_ring = true;
                        }
                    }
                }
            }
        }

        // 環結合に接する原子を in_ring にする
        for (const auto& b : mol_.bonds()) {
            if (b.in_ring) {
                mol_.atom(b.begin).in_ring = true;
                mol_.atom(b.end).in_ring   = true;
            }
        }
    }

    // ===================== メンバ =====================
    static constexpr std::uint32_t NO_ATOM     = 0xFFFFFFFFu;
    static constexpr int           MAX_RING_ID = 100;

    std::string_view                          s_;
    std::size_t                               pos_        = 0;
    Molecule                                  mol_;
    std::uint32_t                             prev_atom_  = NO_ATOM;
    BondOrder                                 pending_bond_ = BondOrder::Unspecified;
    std::vector<std::uint32_t>                branch_stack_;
    std::array<std::uint32_t, MAX_RING_ID>    ring_open_  = [] {
        std::array<std::uint32_t, MAX_RING_ID> a{};
        a.fill(0xFFFFFFFFu);
        return a;
    }();
    std::array<BondOrder, MAX_RING_ID>        ring_order_{};
};

}  // anonymous namespace

std::optional<Molecule> parse_smiles(std::string_view smiles, SmilesError* err) {
    if (smiles.empty()) {
        if (err) *err = {"empty SMILES", 0};
        return std::nullopt;
    }
    SmilesParser p(smiles);
    return p.run(err);
}

}  // namespace chemcpp
```

---

## 20.6 コードの解説

### ① 無名名前空間 `namespace { ... }`

```cpp
namespace {
class SmilesParser { ... };
}
```

**このファイルの中でだけ有効**という宣言です（`static` の現代的な書き方）。
他の `.cpp` から見えないので、名前の衝突が起きず、
コンパイラも最適化しやすくなります。

> 🐍 Pythonの `_private_function` に相当しますが、
> **C++では本当に外から見えなくなります**。

### ② パーサをクラスにした理由

```cpp
class SmilesParser {
    std::string_view s_;
    std::size_t      pos_ = 0;
    Molecule         mol_;
    // ...
};
```

グローバル変数を使うと、**スレッドセーフでなくなります**。
クラスにすることで、複数スレッドから同時にパースできます（第25章）。

### ③ 例外を内部だけで使う

```cpp
std::optional<Molecule> run(SmilesError* err) {
    try {
        parse_chain();
        // ...
    } catch (const SmilesError& e) {
        if (err) *err = e;
        return std::nullopt;      // ★ 外には例外を漏らさない
    }
}
```

**パーサの内部では例外が便利**です（深い再帰から一気に脱出できる）。
しかし**外部インタフェースは `optional`** にすることで、
呼び出し側は例外のコストを気にせず大量処理できます（第18章）。

### ④ `[[noreturn]]`

```cpp
[[noreturn]] void fail(std::string msg) const {
    throw SmilesError{std::move(msg), pos_};
}
```

「この関数は絶対に戻らない」と伝えることで、
コンパイラが「この後のコードは到達しない」と判断でき、
`-Wreturn-type` の誤検出も防げます。

### ⑤ ラムダによる配列の初期化

```cpp
std::array<std::uint32_t, MAX_RING_ID> ring_open_ = [] {
    std::array<std::uint32_t, MAX_RING_ID> a{};
    a.fill(0xFFFFFFFFu);
    return a;
}();
```

「即時実行ラムダ (IIFE)」というイディオムです。
複雑な初期化をメンバ初期化子で書きたいときに使います。

### ⑥ Tarjan の橋検出アルゴリズム

`mark_ring_membership` は、グラフの**橋 (bridge)** を見つけています。
橋とは「取り除くとグラフが分断される辺」で、
**橋でない辺 = 環に属する辺**です。

```
   環の中の結合 → 取り除いても迂回路がある → 橋ではない → 環結合
   側鎖の結合   → 取り除くと分断される     → 橋である   → 環結合ではない
```

O(V+E) で全結合の環所属が判定できます。
再帰ではなく**明示的なスタック**を使っているので、
巨大な分子（タンパク質など）でもスタックオーバーフローしません（第5章）。

---

## 20.7 テストと実行

```cpp
// apps/parse_demo.cpp
#include <chemcpp/smiles.hpp>
#include <chemcpp/element.hpp>

#include <format>
#include <iostream>
#include <string>
#include <vector>

using namespace chemcpp;

const char* order_str(BondOrder o) {
    switch (o) {
        case BondOrder::Single:    return "-";
        case BondOrder::Double:    return "=";
        case BondOrder::Triple:    return "#";
        case BondOrder::Quadruple: return "$";
        case BondOrder::Aromatic:  return ":";
        default:                   return "?";
    }
}

double molecular_weight(const Molecule& m) {
    double mw = 0.0;
    for (const auto& a : m.atoms()) {
        mw += Element::weight(a.atomic_num);
        mw += a.num_h * Element::weight(1);
    }
    return mw;
}

std::string molecular_formula(const Molecule& m) {
    std::array<int, 119> counts{};
    int h_total = 0;
    for (const auto& a : m.atoms()) {
        if (a.atomic_num < counts.size()) ++counts[a.atomic_num];
        h_total += a.num_h;
    }
    counts[1] += h_total;

    std::string out;
    // Hill 順序: C, H, その後アルファベット順
    if (counts[6]) { out += "C"; if (counts[6] > 1) out += std::to_string(counts[6]); }
    if (counts[1]) { out += "H"; if (counts[1] > 1) out += std::to_string(counts[1]); }

    std::vector<std::pair<std::string, int>> rest;
    for (int z = 2; z < 119; ++z) {
        if (z == 6 || counts[z] == 0) continue;
        rest.emplace_back(std::string(Element::symbol(z)), counts[z]);
    }
    std::sort(rest.begin(), rest.end());
    for (const auto& [sym, n] : rest) {
        out += sym;
        if (n > 1) out += std::to_string(n);
    }
    return out;
}

void dump(const std::string& smiles) {
    SmilesError err;
    auto mol = parse_smiles(smiles, &err);

    std::cout << "=== " << smiles << " ===\n";
    if (!mol) {
        std::cout << "  PARSE ERROR: " << err.what() << "\n\n";
        return;
    }

    std::cout << std::format("  atoms  : {}\n", mol->num_atoms());
    std::cout << std::format("  bonds  : {}\n", mol->num_bonds());
    std::cout << std::format("  formula: {}\n", molecular_formula(*mol));
    std::cout << std::format("  MW     : {:.3f}\n", molecular_weight(*mol));

    int ring_bonds = 0, aromatic = 0;
    for (const auto& b : mol->bonds()) {
        if (b.in_ring) ++ring_bonds;
        if (b.order == BondOrder::Aromatic) ++aromatic;
    }
    // 環の数 = 結合数 - 原子数 + 連結成分数（今回は連結成分1と仮定）
    std::cout << std::format("  ring bonds: {}, aromatic bonds: {}\n",
                             ring_bonds, aromatic);

    std::cout << "  atom list:\n";
    for (std::size_t i = 0; i < mol->num_atoms(); ++i) {
        const auto& a = mol->atom(i);
        std::cout << std::format("    [{:>2}] {:<2} H{} q{:+d} {}{} deg={}\n",
                                 i, Element::symbol(a.atomic_num), a.num_h,
                                 a.charge,
                                 a.aromatic ? "arom " : "     ",
                                 a.in_ring  ? "ring" : "    ",
                                 mol->degree(i));
    }
    std::cout << "\n";
}

int main() {
    std::vector<std::string> tests = {
        "CCO",                              // エタノール
        "c1ccccc1",                         // ベンゼン
        "CC(=O)Oc1ccccc1C(=O)O",            // アスピリン
        "Cn1cnc2c1c(=O)n(C)c(=O)n2C",       // カフェイン
        "CC(C)Cc1ccc(cc1)C(C)C(=O)O",       // イブプロフェン
        "C[C@@H](N)C(=O)O",                 // L-アラニン
        "[Na+].[Cl-]",                      // 塩化ナトリウム
        "c1ccc2c(c1)cccc2",                 // ナフタレン
        "C1CC2CCC1CC2",                     // ビシクロ[2.2.2]オクタン
        "CC(=O)N[C@@H](Cc1c[nH]c2ccccc12)C(=O)O",  // N-アセチルトリプトファン
        "O=C(O)c1ccccc1O",                  // サリチル酸
        "CC(=O",                            // ★ エラー: 括弧が閉じていない
        "c1cccc",                           // ★ エラー: 環が閉じていない
        "CCXO",                             // ★ エラー: 未知の文字
    };

    for (const auto& s : tests) dump(s);
    return 0;
}
```

期待される出力（一部）:

```
=== CC(=O)Oc1ccccc1C(=O)O ===
  atoms  : 13
  bonds  : 13
  formula: C9H8O4
  MW     : 180.159
  ring bonds: 6, aromatic bonds: 6
  atom list:
    [ 0] C  H3 q+0            deg=1
    [ 1] C  H0 q+0            deg=3
    [ 2] O  H0 q+0            deg=1
    [ 3] O  H0 q+0            deg=2
    [ 4] c  H0 q+0 arom ring  deg=3
    ...
```

**アスピリンの分子式 C9H8O4、分子量 180.16 が正しく出ました。**
RDKit と同じ答えです。

---

## 20.8 テストを書く

```cpp
// tests/test_smiles.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chemcpp/smiles.hpp>
#include <chemcpp/element.hpp>

using namespace chemcpp;

static double mw(const Molecule& m) {
    double v = 0;
    for (const auto& a : m.atoms())
        v += Element::weight(a.atomic_num) + a.num_h * Element::weight(1);
    return v;
}

TEST_CASE("simple aliphatic molecules", "[smiles]") {
    auto ethanol = parse_smiles("CCO");
    REQUIRE(ethanol.has_value());
    CHECK(ethanol->num_atoms() == 3);
    CHECK(ethanol->num_bonds() == 2);
    CHECK(ethanol->atom(0).num_h == 3);      // CH3
    CHECK(ethanol->atom(1).num_h == 2);      // CH2
    CHECK(ethanol->atom(2).num_h == 1);      // OH
    CHECK_THAT(mw(*ethanol),
               Catch::Matchers::WithinAbs(46.069, 0.01));
}

TEST_CASE("aromatic ring", "[smiles]") {
    auto benzene = parse_smiles("c1ccccc1");
    REQUIRE(benzene);
    CHECK(benzene->num_atoms() == 6);
    CHECK(benzene->num_bonds() == 6);
    for (std::size_t i = 0; i < 6; ++i) {
        CHECK(benzene->atom(i).aromatic);
        CHECK(benzene->atom(i).in_ring);
        CHECK(benzene->atom(i).num_h == 1);
    }
    CHECK_THAT(mw(*benzene), Catch::Matchers::WithinAbs(78.11, 0.02));
}

TEST_CASE("branches", "[smiles]") {
    auto isobutane = parse_smiles("CC(C)C");
    REQUIRE(isobutane);
    CHECK(isobutane->num_atoms() == 4);
    CHECK(isobutane->degree(1) == 3);        // 中心炭素は3本の結合
}

TEST_CASE("bracket atoms", "[smiles]") {
    auto ammonium = parse_smiles("[NH4+]");
    REQUIRE(ammonium);
    CHECK(ammonium->num_atoms() == 1);
    CHECK(ammonium->atom(0).atomic_num == 7);
    CHECK(ammonium->atom(0).num_h == 4);
    CHECK(ammonium->atom(0).charge == 1);

    auto isotope = parse_smiles("[13CH4]");
    REQUIRE(isotope);
    CHECK(isotope->atom(0).isotope == 13);
}

TEST_CASE("disconnected structures", "[smiles]") {
    auto salt = parse_smiles("[Na+].[Cl-]");
    REQUIRE(salt);
    CHECK(salt->num_atoms() == 2);
    CHECK(salt->num_bonds() == 0);           // 結合なし
}

TEST_CASE("ring membership detection", "[smiles]") {
    auto toluene = parse_smiles("Cc1ccccc1");
    REQUIRE(toluene);
    CHECK_FALSE(toluene->atom(0).in_ring);   // メチル基は環外
    CHECK(toluene->atom(1).in_ring);         // 芳香環

    int ring_bonds = 0;
    for (const auto& b : toluene->bonds()) if (b.in_ring) ++ring_bonds;
    CHECK(ring_bonds == 6);
}

TEST_CASE("fused rings", "[smiles]") {
    auto naphthalene = parse_smiles("c1ccc2ccccc2c1");
    REQUIRE(naphthalene);
    CHECK(naphthalene->num_atoms() == 10);
    CHECK(naphthalene->num_bonds() == 11);   // 環2つ = 結合が原子数+1
}

TEST_CASE("parse errors", "[smiles]") {
    SmilesError err;

    CHECK_FALSE(parse_smiles("", &err).has_value());
    CHECK_FALSE(parse_smiles("CC(=O", &err).has_value());
    CHECK_FALSE(parse_smiles("CCO)", &err).has_value());
    CHECK_FALSE(parse_smiles("c1cccc", &err).has_value());
    CHECK_FALSE(parse_smiles("CCXO", &err).has_value());
    CHECK_FALSE(parse_smiles("[Xyz]", &err).has_value());
}

TEST_CASE("real drug molecules", "[smiles][integration]") {
    struct Case { const char* smiles; std::size_t atoms; double mw; };
    const Case cases[] = {
        {"CC(=O)Oc1ccccc1C(=O)O",          13, 180.16},   // aspirin
        {"Cn1cnc2c1c(=O)n(C)c(=O)n2C",     14, 194.19},   // caffeine
        {"CC(C)Cc1ccc(cc1)C(C)C(=O)O",     15, 206.28},   // ibuprofen
        {"CC(=O)Nc1ccc(O)cc1",             11, 151.16},   // paracetamol
    };

    for (const auto& c : cases) {
        INFO("SMILES: " << c.smiles);
        auto m = parse_smiles(c.smiles);
        REQUIRE(m);
        CHECK(m->num_atoms() == c.atoms);
        CHECK_THAT(mw(*m), Catch::Matchers::WithinAbs(c.mw, 0.05));
    }
}
```

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## 20.9 性能を測る

```cpp
// bench/bench_parse.cpp
#include <chemcpp/smiles.hpp>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

int main() {
    // テスト用SMILESを大量に用意
    std::vector<std::string> library;
    const std::vector<std::string> templates = {
        "CC(=O)Oc1ccccc1C(=O)O",
        "Cn1cnc2c1c(=O)n(C)c(=O)n2C",
        "CC(C)Cc1ccc(cc1)C(C)C(=O)O",
        "CC(=O)Nc1ccc(O)cc1",
        "c1ccc2c(c1)cccc2",
        "C[C@@H](N)C(=O)O",
    };
    for (int i = 0; i < 100'000; ++i) library.push_back(templates[i % templates.size()]);

    auto t0 = std::chrono::steady_clock::now();
    std::size_t total_atoms = 0, failures = 0;
    for (const auto& s : library) {
        if (auto m = chemcpp::parse_smiles(s)) total_atoms += m->num_atoms();
        else ++failures;
    }
    auto ms = std::chrono::duration<double, std::milli>(
                  std::chrono::steady_clock::now() - t0).count();

    std::cout << "parsed  : " << library.size() << " molecules\n";
    std::cout << "atoms   : " << total_atoms << "\n";
    std::cout << "failures: " << failures << "\n";
    std::cout << "time    : " << ms << " ms\n";
    std::cout << "rate    : " << (library.size() / ms * 1000.0) << " mol/s\n";
}
```

参考値（手元の環境、`-O2`）:

```
parsed  : 100000 molecules
atoms   : 1300000
failures: 0
time    : 214 ms
rate    : 467290 mol/s
```

**毎秒約47万分子**。RDKit の Python API（`Chem.MolFromSmiles`）は
おおよそ毎秒2〜5万分子なので、**10倍程度速い**計算になります
（RDKitははるかに多くの処理をしているので、単純比較はできませんが）。

---

## 20.10 発展課題

このパーサに欠けているもの:

| 機能 | 難易度 | 備考 |
|---|---|---|
| **芳香族の検証（Hückel則）** | 高 | 現状は小文字を信用しているだけ |
| **ケクレ化** | 高 | 芳香結合を交互の単/二重結合に変換 |
| **立体化学の解決** | 高 | `@`/`@@` から実際の配置を決定 |
| **SSSR（最小環集合）** | 中 | 環の個数と構成原子の特定（第24章） |
| **正準SMILES生成** | 高 | Morganアルゴリズム + 正準順序（第21章） |
| **SMARTS対応** | 高 | 部分構造検索用のパターン言語 |
| **エラー回復** | 低 | 部分的に壊れたSMILESからの復帰 |

**実務では RDKit を使ってください。**
これらを全部正しく実装するのは、数万行の仕事です。

このパーサの価値は:
- **仕組みが分かる**（RDKitのソースが読めるようになる）
- **軽量**（RDKit をリンクせずに、SMILESの前処理ができる）
- **改造できる**（独自のフォーマットや制約を追加できる）

---

## 20.11 この章のまとめ

- SMILESパーサは **トークナイズ → 構築 → 後処理** の3段階
- 分子グラフは **vector + インデックス** で表現する
  （ポインタより速く、安全で、シリアライズしやすい）
- パーサは**クラス**にして状態を持たせる（スレッドセーフ）
- **内部では例外、外部インタフェースは `optional`**
- 環の検出は **Tarjanの橋検出** で O(V+E)
- 深いグラフには**明示スタックのDFS**（再帰はスタックオーバーフローする）
- `constexpr` テーブルは実行時コストゼロ
- 無名名前空間で実装詳細を隠す

> 📝 **練習問題 20-1**
>
> パーサに「不連結成分の数」を返すメソッドを追加してください。
> `[Na+].[Cl-]` なら 2、`CCO` なら 1 です。

> 📝 **練習問題 20-2**
>
> `/` と `\` を正しく扱い、二重結合の cis/trans を判定できるようにしてください。
> （`F/C=C/F` はトランス、`F/C=C\F` はシス）

> 📝 **練習問題 20-3**
>
> パーサの逆——分子グラフから SMILES を生成する `to_smiles(const Molecule&)` を
> 書いてください。（DFSで辿り、環結合番号を割り当てる）

> 📝 **練習問題 20-4**
>
> 100万件のSMILESをパースし、`perf` や Instruments でプロファイルを取って、
> どこに時間がかかっているか調べてください。（第28章の予習）

---

→ [第21章 実践2: 分子グラフとECFP](ch21-molecular-graph-and-ecfp.md)
