// 第20章: SMILES パーサの実装
#include <chemcpp/smiles.hpp>
#include <chemcpp/element.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>     // std::abs(int)
#include <string>
#include <string_view>
#include <vector>

namespace chemcpp {
namespace {

// このファイルの中だけで有効（無名名前空間）
class SmilesParser {
public:
    explicit SmilesParser(std::string_view s) : s_(s) {
        ring_open_.fill(NO_ATOM);
        ring_order_.fill(BondOrder::Unspecified);
    }

    std::optional<Molecule> run(SmilesError* err) {
        try {
            parse_chain();
            if (!rings_all_closed()) fail("unclosed ring bond");
            finalize();
            return std::move(mol_);
        } catch (const SmilesError& e) {
            if (err) *err = e;
            return std::nullopt;
        }
    }

private:
    // ================= 低レベルユーティリティ =================
    bool eof() const noexcept { return pos_ >= s_.size(); }

    char peek(std::size_t off = 0) const noexcept {
        return (pos_ + off < s_.size()) ? s_[pos_ + off] : '\0';
    }
    char next() noexcept { return s_[pos_++]; }

    bool consume(char c) noexcept {
        if (peek() == c) { ++pos_; return true; }
        return false;
    }

    [[noreturn]] void fail(std::string msg) const {
        throw SmilesError{std::move(msg), pos_};
    }

    static bool is_digit(char c) noexcept {
        return std::isdigit(static_cast<unsigned char>(c)) != 0;
    }
    static bool is_upper(char c) noexcept {
        return std::isupper(static_cast<unsigned char>(c)) != 0;
    }
    static bool is_lower(char c) noexcept {
        return std::islower(static_cast<unsigned char>(c)) != 0;
    }

    // ================= メインループ =================
    void parse_chain() {
        while (!eof()) {
            const char c = peek();

            if (c == '(') {
                ++pos_;
                if (prev_atom_ == NO_ATOM) fail("branch without preceding atom");
                branch_stack_.push_back(prev_atom_);
                continue;
            }
            if (c == ')') {
                ++pos_;
                if (branch_stack_.empty()) fail("unmatched ')'");
                prev_atom_ = branch_stack_.back();
                branch_stack_.pop_back();
                continue;
            }
            if (c == '.') {
                ++pos_;
                prev_atom_    = NO_ATOM;
                pending_bond_ = BondOrder::Unspecified;
                continue;
            }

            if (auto b = try_parse_bond()) { pending_bond_ = *b; continue; }

            if (is_digit(c) || c == '%') { parse_ring_closure(); continue; }

            if (c == '[') { connect(parse_bracket_atom()); continue; }

            if (auto idx = try_parse_organic_atom()) { connect(*idx); continue; }

            fail(std::string("unexpected character '") + c + "'");
        }
        if (!branch_stack_.empty()) fail("unclosed '('");
    }

    // ================= 結合 =================
    std::optional<BondOrder> try_parse_bond() {
        switch (peek()) {
            case '-':  ++pos_; return BondOrder::Single;
            case '=':  ++pos_; return BondOrder::Double;
            case '#':  ++pos_; return BondOrder::Triple;
            case '$':  ++pos_; return BondOrder::Quadruple;
            case ':':  ++pos_; return BondOrder::Aromatic;
            case '/':  ++pos_; return BondOrder::Single;   // 立体は本実装では無視
            case '\\': ++pos_; return BondOrder::Single;
            default:   return std::nullopt;
        }
    }

    void connect(std::uint32_t idx) {
        if (prev_atom_ != NO_ATOM) {
            BondOrder order = pending_bond_;
            if (order == BondOrder::Unspecified) {
                order = (mol_.atom(prev_atom_).aromatic && mol_.atom(idx).aromatic)
                            ? BondOrder::Aromatic : BondOrder::Single;
            }
            mol_.add_bond(prev_atom_, idx, order);
        }
        prev_atom_    = idx;
        pending_bond_ = BondOrder::Unspecified;
    }

    // ================= 有機部分集合原子 =================
    std::optional<std::uint32_t> try_parse_organic_atom() {
        const char c = peek();

        // 2文字元素を先に判定
        if (c == 'C' && peek(1) == 'l') { pos_ += 2; return make_atom(17, false); }
        if (c == 'B' && peek(1) == 'r') { pos_ += 2; return make_atom(35, false); }

        int z = 0;
        switch (c) {                       // 脂肪族（大文字）
            case 'B': z = 5;  break;
            case 'C': z = 6;  break;
            case 'N': z = 7;  break;
            case 'O': z = 8;  break;
            case 'F': z = 9;  break;
            case 'P': z = 15; break;
            case 'S': z = 16; break;
            case 'I': z = 53; break;
            default:  break;
        }
        if (z != 0) { ++pos_; return make_atom(z, false); }

        switch (c) {                       // 芳香族（小文字）
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

    // ================= ブラケット原子 =================
    // [isotope? symbol chiral? Hcount? charge? (:class)? ]
    std::uint32_t parse_bracket_atom() {
        if (!consume('[')) fail("expected '['");

        Atom a;
        a.explicit_h = true;      // ブラケット内は水素数が明示される

        // --- 同位体 ---
        int isotope = 0;
        while (is_digit(peek())) isotope = isotope * 10 + (next() - '0');
        a.isotope = static_cast<std::uint8_t>(isotope > 255 ? 0 : isotope);

        // --- 元素記号 ---
        if (eof()) fail("unterminated bracket atom");
        const char c0 = peek();

        if (c0 == '*') {
            ++pos_;
            a.atomic_num = 0;
        } else if (is_lower(c0)) {
            a.aromatic = true;
            std::size_t len = 1;
            if (is_lower(peek(1))) {                 // se, as
                std::string two;
                two += static_cast<char>(std::toupper(static_cast<unsigned char>(c0)));
                two += peek(1);
                if (Element::number(two) != 0) len = 2;
            }
            std::string sym(s_.substr(pos_, len));
            sym[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(sym[0])));
            const int z = Element::number(sym);
            if (z == 0) fail("unknown aromatic atom '" + sym + "'");
            a.atomic_num = static_cast<std::uint8_t>(z);
            pos_ += len;
        } else if (is_upper(c0)) {
            std::size_t len = 1;
            if (is_lower(peek(1))) {
                const std::string two(s_.substr(pos_, 2));
                if (Element::number(two) != 0) len = 2;
            }
            const std::string sym(s_.substr(pos_, len));
            const int z = Element::number(sym);
            if (z == 0) fail("unknown element '" + sym + "'");
            a.atomic_num = static_cast<std::uint8_t>(z);
            pos_ += len;
        } else {
            fail("expected element symbol in bracket");
        }

        // --- 立体化学 ---
        // @ / @@ のみ対応。@TH1 / @AL2 などの拡張表記は本実装では扱わない
        // （実データの大半は @ / @@ で書かれている）。
        if (peek() == '@') {
            ++pos_;
            a.chirality = consume('@') ? Chirality::CW : Chirality::CCW;
        }

        // --- 水素数 ---
        if (peek() == 'H') {
            ++pos_;
            int h = 1;
            if (is_digit(peek())) {
                h = 0;
                while (is_digit(peek())) h = h * 10 + (next() - '0');
            }
            a.num_h = static_cast<std::uint8_t>(h);
        }

        // --- 電荷 ---
        if (peek() == '+' || peek() == '-') {
            const char sign_ch = next();
            const int  sign    = (sign_ch == '+') ? 1 : -1;
            int mag = 1;
            if (is_digit(peek())) {
                mag = 0;
                while (is_digit(peek())) mag = mag * 10 + (next() - '0');
            } else {
                while (peek() == sign_ch) { ++pos_; ++mag; }
            }
            a.charge = static_cast<std::int8_t>(sign * mag);
        }

        // --- アトムクラス（:12）は読み飛ばす ---
        if (peek() == ':') {
            ++pos_;
            while (is_digit(peek())) ++pos_;
        }

        if (!consume(']')) fail("expected ']'");
        return mol_.add_atom(a);
    }

    // ================= 環結合 =================
    void parse_ring_closure() {
        int id = 0;
        if (consume('%')) {
            if (!is_digit(peek())) fail("expected digits after '%'");
            id = (next() - '0') * 10;
            if (!is_digit(peek())) fail("expected two digits after '%'");
            id += (next() - '0');
        } else {
            id = next() - '0';
        }
        if (id < 0 || id >= static_cast<int>(MAX_RING_ID))
            fail("ring bond id out of range");
        if (prev_atom_ == NO_ATOM) fail("ring closure without preceding atom");

        const auto ri = static_cast<std::size_t>(id);
        if (ring_open_[ri] == NO_ATOM) {
            ring_open_[ri]  = prev_atom_;
            ring_order_[ri] = pending_bond_;
        } else {
            const std::uint32_t other = ring_open_[ri];
            if (other == prev_atom_) fail("ring closure to itself");

            BondOrder order = pending_bond_;
            if (order == BondOrder::Unspecified) order = ring_order_[ri];
            if (order == BondOrder::Unspecified) {
                order = (mol_.atom(other).aromatic && mol_.atom(prev_atom_).aromatic)
                            ? BondOrder::Aromatic : BondOrder::Single;
            }
            mol_.add_bond(other, prev_atom_, order);
            ring_open_[ri]  = NO_ATOM;
            ring_order_[ri] = BondOrder::Unspecified;
        }
        pending_bond_ = BondOrder::Unspecified;
    }

    bool rings_all_closed() const noexcept {
        return std::all_of(ring_open_.begin(), ring_open_.end(),
                           [](std::uint32_t v) { return v == NO_ATOM; });
    }

    // ================= 後処理 =================
    void finalize() {
        mark_ring_membership();
        for (std::size_t i = 0; i < mol_.num_atoms(); ++i) {
            Atom& a = mol_.atom(i);
            if (a.explicit_h) continue;
            a.num_h = static_cast<std::uint8_t>(implicit_hydrogens(i));
        }
    }

    int implicit_hydrogens(std::size_t idx) const {
        const Atom& a = mol_.atom(idx);
        const int valence = Element::default_valence(a.atomic_num);
        if (valence == 0) return 0;

        double bond_sum = 0.0;
        for (const auto& nb : mol_.neighbors(idx)) {
            bond_sum += order_value(mol_.bond(nb.bond).order);
        }
        // 芳香族原子は共役に電子を出しているとみなす簡易補正
        if (a.aromatic) bond_sum += 0.5;

        const int used = static_cast<int>(bond_sum + 0.4);
        int h = valence - used;
        if (a.charge != 0) {
            // N+ / P+ は原子価が1増える。O- などは1減る、という簡易ルール
            h += (a.atomic_num == 7 || a.atomic_num == 15)
                     ? a.charge : -std::abs(a.charge);
        }
        return h > 0 ? h : 0;
    }

    /// Tarjan の橋検出。橋でない結合＝環に属する結合。
    /// 再帰ではなく明示スタックを使う（巨大な分子でも安全）。
    void mark_ring_membership() {
        const std::size_t n = mol_.num_atoms();
        if (n == 0) return;

        std::vector<int>  disc(n, -1), low(n, 0);
        std::vector<int>  parent_bond(n, -1);
        std::vector<char> visited(n, 0);
        int timer = 0;

        struct Frame { std::uint32_t node; std::size_t next_idx; };
        std::vector<Frame> stack;
        stack.reserve(n);

        for (std::uint32_t start = 0; start < n; ++start) {
            if (visited[start]) continue;
            visited[start] = 1;
            disc[start] = low[start] = timer++;
            stack.push_back({start, 0});

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
                        if (low[child] <= disc[par] && parent_bond[child] >= 0) {
                            mol_.bond(static_cast<std::size_t>(parent_bond[child]))
                                .in_ring = true;
                        }
                    }
                }
            }
        }

        for (std::size_t b = 0; b < mol_.num_bonds(); ++b) {
            const Bond& bond = mol_.bond(b);
            if (bond.in_ring) {
                mol_.atom(bond.begin).in_ring = true;
                mol_.atom(bond.end).in_ring   = true;
            }
        }
    }

    // ================= メンバ =================
    static constexpr std::uint32_t NO_ATOM     = 0xFFFFFFFFu;
    static constexpr std::size_t   MAX_RING_ID = 100;

    std::string_view                            s_;
    std::size_t                                 pos_          = 0;
    Molecule                                    mol_;
    std::uint32_t                               prev_atom_    = NO_ATOM;
    BondOrder                                   pending_bond_ = BondOrder::Unspecified;
    std::vector<std::uint32_t>                  branch_stack_;
    std::array<std::uint32_t, MAX_RING_ID>      ring_open_{};
    std::array<BondOrder,     MAX_RING_ID>      ring_order_{};
};

}  // anonymous namespace

std::optional<Molecule> parse_smiles(std::string_view smiles, SmilesError* err) {
    if (smiles.empty()) {
        if (err) *err = SmilesError{"empty SMILES", 0};
        return std::nullopt;
    }
    SmilesParser parser(smiles);
    return parser.run(err);
}

}  // namespace chemcpp
