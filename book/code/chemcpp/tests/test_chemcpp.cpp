// 第29章: テストスイート
//
// Catch2 v3 を使う。CMake が FetchContent で自動取得する。
//   cmake --build build && ctest --test-dir build --output-on-failure
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <chemcpp/smiles.hpp>
#include <chemcpp/element.hpp>
#include <chemcpp/graph.hpp>
#include <chemcpp/fingerprint.hpp>
#include <chemcpp/similarity.hpp>
#include <chemcpp/descriptors.hpp>
#include <chemcpp/database.hpp>
#include <chemcpp/search.hpp>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

using namespace chemcpp;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// =====================================================================
// パーサ
// =====================================================================

TEST_CASE("simple aliphatic molecules", "[smiles]") {
    const auto ethanol = parse_smiles("CCO");
    REQUIRE(ethanol.has_value());
    CHECK(ethanol->num_atoms() == 3);
    CHECK(ethanol->num_bonds() == 2);
    CHECK(ethanol->atom(0).num_h == 3);      // CH3
    CHECK(ethanol->atom(1).num_h == 2);      // CH2
    CHECK(ethanol->atom(2).num_h == 1);      // OH
    CHECK_THAT(molecular_weight(*ethanol), WithinAbs(46.069, 0.02));
    CHECK(molecular_formula(*ethanol) == "C2H6O");
}

TEST_CASE("aromatic ring", "[smiles]") {
    const auto benzene = parse_smiles("c1ccccc1");
    REQUIRE(benzene);
    CHECK(benzene->num_atoms() == 6);
    CHECK(benzene->num_bonds() == 6);
    for (std::size_t i = 0; i < 6; ++i) {
        INFO("atom " << i);
        CHECK(benzene->atom(i).aromatic);
        CHECK(benzene->atom(i).in_ring);
        CHECK(benzene->atom(i).num_h == 1);
    }
    CHECK(ring_count(*benzene) == 1);
    CHECK_THAT(molecular_weight(*benzene), WithinAbs(78.11, 0.05));
}

TEST_CASE("branches", "[smiles]") {
    const auto isobutane = parse_smiles("CC(C)C");
    REQUIRE(isobutane);
    CHECK(isobutane->num_atoms() == 4);
    CHECK(isobutane->degree(1) == 3);
    CHECK(isobutane->atom(1).num_h == 1);
}

TEST_CASE("two-character elements", "[smiles]") {
    const auto m = parse_smiles("ClCCBr");
    REQUIRE(m);
    CHECK(m->num_atoms() == 4);
    CHECK(m->atom(0).atomic_num == 17);   // Cl
    CHECK(m->atom(3).atomic_num == 35);   // Br
}

TEST_CASE("bracket atoms", "[smiles]") {
    SECTION("ammonium") {
        const auto m = parse_smiles("[NH4+]");
        REQUIRE(m);
        CHECK(m->num_atoms() == 1);
        CHECK(m->atom(0).atomic_num == 7);
        CHECK(m->atom(0).num_h == 4);
        CHECK(m->atom(0).charge == 1);
    }
    SECTION("isotope") {
        const auto m = parse_smiles("[13CH4]");
        REQUIRE(m);
        CHECK(m->atom(0).isotope == 13);
        CHECK(m->atom(0).num_h == 4);
    }
    SECTION("negative charge") {
        const auto m = parse_smiles("[O-]");
        REQUIRE(m);
        CHECK(m->atom(0).charge == -1);
    }
    SECTION("double charge") {
        const auto m = parse_smiles("[Fe+2]");
        REQUIRE(m);
        CHECK(m->atom(0).atomic_num == 26);
        CHECK(m->atom(0).charge == 2);
    }
    SECTION("aromatic nitrogen with H") {
        const auto m = parse_smiles("c1cc[nH]c1");
        REQUIRE(m);
        CHECK(m->num_atoms() == 5);
    }
}

TEST_CASE("disconnected structures", "[smiles]") {
    const auto salt = parse_smiles("[Na+].[Cl-]");
    REQUIRE(salt);
    CHECK(salt->num_atoms() == 2);
    CHECK(salt->num_bonds() == 0);
    CHECK(num_components(*salt) == 2);
}

TEST_CASE("ring membership detection", "[smiles][graph]") {
    const auto toluene = parse_smiles("Cc1ccccc1");
    REQUIRE(toluene);
    CHECK_FALSE(toluene->atom(0).in_ring);   // メチル基は環外
    CHECK(toluene->atom(1).in_ring);

    int ring_bonds = 0;
    for (const auto& b : toluene->bonds()) if (b.in_ring) ++ring_bonds;
    CHECK(ring_bonds == 6);
    CHECK(ring_count(*toluene) == 1);
}

TEST_CASE("fused rings", "[smiles][graph]") {
    const auto naphthalene = parse_smiles("c1ccc2ccccc2c1");
    REQUIRE(naphthalene);
    CHECK(naphthalene->num_atoms() == 10);
    CHECK(naphthalene->num_bonds() == 11);
    CHECK(ring_count(*naphthalene) == 2);
}

TEST_CASE("high ring numbers", "[smiles]") {
    const auto m = parse_smiles("C%10CCCC%10");
    REQUIRE(m);
    CHECK(m->num_atoms() == 5);
    CHECK(ring_count(*m) == 1);
}

TEST_CASE("parse errors", "[smiles]") {
    SmilesError err;
    CHECK_FALSE(parse_smiles("", &err).has_value());
    CHECK_FALSE(parse_smiles("CC(=O", &err).has_value());
    CHECK_FALSE(parse_smiles("CCO)", &err).has_value());
    CHECK_FALSE(parse_smiles("c1cccc", &err).has_value());
    CHECK_FALSE(parse_smiles("CCXO", &err).has_value());
    CHECK_FALSE(parse_smiles("[Xyz]", &err).has_value());
    CHECK_FALSE(parse_smiles("C1", &err).has_value());       // 環が閉じない
}

TEST_CASE("real drug molecules", "[smiles][integration]") {
    struct Case { const char* smiles; std::size_t atoms; double mw; };
    const auto c = GENERATE(
        Case{"CC(=O)Oc1ccccc1C(=O)O",       13, 180.16},   // aspirin
        Case{"Cn1cnc2c1c(=O)n(C)c(=O)n2C",  14, 194.19},   // caffeine
        Case{"CC(C)Cc1ccc(cc1)C(C)C(=O)O",  15, 206.28},   // ibuprofen
        Case{"CC(=O)Nc1ccc(O)cc1",          11, 151.16},   // paracetamol
        Case{"O=C(O)c1ccccc1O",             10, 138.12}    // salicylic acid
    );

    INFO("SMILES: " << c.smiles);
    const auto m = parse_smiles(c.smiles);
    REQUIRE(m);
    CHECK(m->num_atoms() == c.atoms);
    CHECK_THAT(molecular_weight(*m), WithinRel(c.mw, 0.01));
}

// =====================================================================
// フィンガープリント
// =====================================================================

TEST_CASE("identical molecules give identical fingerprints", "[fp]") {
    const auto m1 = parse_smiles("CC(=O)Oc1ccccc1C(=O)O");
    const auto m2 = parse_smiles("CC(=O)Oc1ccccc1C(=O)O");
    REQUIRE(m1); REQUIRE(m2);
    CHECK(morgan_fingerprint(*m1) == morgan_fingerprint(*m2));
}

TEST_CASE("atom ordering does not change the feature set", "[fp]") {
    // 同じ分子を別の順序で書いた SMILES
    const auto a = parse_smiles("CCO");
    const auto b = parse_smiles("OCC");
    REQUIRE(a); REQUIRE(b);
    CHECK(morgan_hashes(*a) == morgan_hashes(*b));
}

TEST_CASE("larger radius gives more features", "[fp]") {
    const auto m = parse_smiles("CC(=O)Oc1ccccc1C(=O)O");
    REQUIRE(m);
    std::size_t prev = 0;
    for (int r = 0; r <= 3; ++r) {
        const auto h = morgan_hashes(*m, {.radius = r});
        INFO("radius " << r);
        CHECK(h.size() > prev);
        prev = h.size();
    }
}

TEST_CASE("similarity ordering makes chemical sense", "[fp][similarity]") {
    const auto benzene = parse_smiles("c1ccccc1");
    const auto toluene = parse_smiles("Cc1ccccc1");
    const auto xylene  = parse_smiles("Cc1ccccc1C");
    const auto ethanol = parse_smiles("CCO");
    REQUIRE(benzene); REQUIRE(toluene); REQUIRE(xylene); REQUIRE(ethanol);

    const auto fb = morgan_fingerprint(*benzene);
    const auto ft = morgan_fingerprint(*toluene);
    const auto fx = morgan_fingerprint(*xylene);
    const auto fe = morgan_fingerprint(*ethanol);

    CHECK(tanimoto(fb, fb) == 1.0);
    CHECK(tanimoto(fb, ft) > tanimoto(fb, fe));
    CHECK(tanimoto(ft, fx) > tanimoto(fb, fx));
}

TEST_CASE("empty molecule", "[fp][edge]") {
    const Molecule empty;
    CHECK(popcount(morgan_fingerprint(empty)) == 0);
    CHECK(molecular_weight(empty) == 0.0);
    CHECK(ring_count(empty) == 0);
    CHECK(num_components(empty) == 0);
}

// =====================================================================
// 類似度指標の不変条件（プロパティテスト）
// =====================================================================

namespace {
Fingerprint random_fp(std::mt19937_64& rng, int nbits) {
    Fingerprint fp{};
    for (int i = 0; i < nbits; ++i) set_bit(fp, rng() % FP_BITS);
    return fp;
}
}  // namespace

TEST_CASE("similarity metric properties", "[similarity][property]") {
    std::mt19937_64 rng(20240101);

    for (int trial = 0; trial < 500; ++trial) {
        const auto a = random_fp(rng, 10 + static_cast<int>(rng() % 90));
        const auto b = random_fp(rng, 10 + static_cast<int>(rng() % 90));

        INFO("trial " << trial);

        const double t = tanimoto(a, b);
        const double d = dice(a, b);
        const double c = cosine(a, b);

        // 値域
        CHECK(t >= 0.0); CHECK(t <= 1.0);
        CHECK(d >= 0.0); CHECK(d <= 1.0);
        CHECK(c >= 0.0); CHECK(c <= 1.0);

        // 対称性
        CHECK_THAT(t, WithinAbs(tanimoto(b, a), 1e-12));
        CHECK_THAT(d, WithinAbs(dice(b, a),     1e-12));

        // 反射性
        CHECK_THAT(tanimoto(a, a), WithinAbs(1.0, 1e-12));

        // Dice >= Tanimoto は常に成り立つ
        CHECK(d >= t - 1e-12);

        // Tversky の特殊化
        CHECK_THAT(tversky(a, b, 1.0, 1.0), WithinAbs(t, 1e-12));
        CHECK_THAT(tversky(a, b, 0.5, 0.5), WithinAbs(d, 1e-12));

        // 集合の等式
        const int inter = intersection(a, b);
        const int uni   = union_count(a, b);
        CHECK(hamming(a, b) == uni - inter);
        CHECK(popcount(a) + popcount(b) == uni + inter);

        // 事前計算版が同じ値を返す
        CHECK_THAT(tanimoto_fast(a, popcount(a), b, popcount(b)),
                   WithinAbs(t, 1e-12));

        // 自分自身は自分の部分集合
        CHECK(is_subset(a, a));
    }
}

// =====================================================================
// 検索エンジン
// =====================================================================

TEST_CASE("threshold search with and without pruning agree", "[search]") {
    std::mt19937_64 rng(7);
    FingerprintDB db;
    db.reserve(2000);
    for (int i = 0; i < 2000; ++i) {
        db.add(random_fp(rng, 20 + static_cast<int>(rng() % 60)),
               "MOL" + std::to_string(i));
    }
    db.sort_by_count();

    const auto query = random_fp(rng, 40);

    for (double th : {0.2, 0.3, 0.4, 0.5}) {
        INFO("threshold " << th);
        const auto a = search_threshold(db, query, th, /*pruning=*/false);
        const auto b = search_threshold(db, query, th, /*pruning=*/true);
        REQUIRE(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i) {
            CHECK(a[i].index == b[i].index);
            CHECK_THAT(a[i].score, WithinAbs(b[i].score, 1e-12));
        }
    }
}

TEST_CASE("top-k search agrees with full sort", "[search]") {
    std::mt19937_64 rng(11);
    FingerprintDB db;
    db.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        db.add(random_fp(rng, 20 + static_cast<int>(rng() % 60)),
               "MOL" + std::to_string(i));
    }
    const auto query = random_fp(rng, 40);

    // 全件を計算してソートした参照結果
    std::vector<Hit> all;
    all.reserve(db.size());
    for (std::size_t i = 0; i < db.size(); ++i) {
        Fingerprint fp{};
        std::copy_n(db.fp_ptr(i), FP_WORDS, fp.begin());
        all.push_back({tanimoto(query, fp), static_cast<std::uint32_t>(i)});
    }
    std::sort(all.begin(), all.end(), std::greater<Hit>{});

    const std::size_t K = 20;
    const auto topk = search_topk(db, query, K);
    REQUIRE(topk.size() == K);
    for (std::size_t i = 0; i < K; ++i) {
        INFO("rank " << i);
        CHECK_THAT(topk[i].score, WithinAbs(all[i].score, 1e-12));
    }
    // 降順であること
    for (std::size_t i = 1; i < topk.size(); ++i) {
        CHECK(topk[i - 1].score >= topk[i].score);
    }
}

TEST_CASE("database roundtrip through binary file", "[db]") {
    std::mt19937_64 rng(3);
    FingerprintDB db;
    for (int i = 0; i < 100; ++i) {
        db.add(random_fp(rng, 30), "ID" + std::to_string(i));
    }

    const std::string path = "test_db.bin";
    db.save(path);

    FingerprintDB loaded;
    loaded.load(path);

    REQUIRE(loaded.size() == db.size());
    for (std::size_t i = 0; i < db.size(); ++i) {
        INFO("record " << i);
        CHECK(loaded.id(i) == db.id(i));
        CHECK(loaded.count(i) == db.count(i));
        for (std::size_t w = 0; w < FP_WORDS; ++w) {
            CHECK(loaded.fp_ptr(i)[w] == db.fp_ptr(i)[w]);
        }
    }
    std::remove(path.c_str());
}

// =====================================================================
// 記述子
// =====================================================================

TEST_CASE("descriptor sanity", "[descriptors]") {
    const auto aspirin = parse_smiles("CC(=O)Oc1ccccc1C(=O)O");
    REQUIRE(aspirin);
    const auto d = compute_descriptors(*aspirin);

    CHECK(d.heavy_atoms == 13);
    CHECK(d.n_rings == 1);
    CHECK(d.hbd >= 1);
    CHECK(d.hba >= 3);
    CHECK(d.tpsa > 50.0);
    CHECK(d.tpsa < 80.0);
    CHECK(passes_ro5(d));
    CHECK(qed_score(d) >= 0.0);
    CHECK(qed_score(d) <= 1.0);
}

TEST_CASE("lipinski violations", "[descriptors]") {
    Descriptors d;
    d.mw = 300; d.logp = 2.0; d.hbd = 2; d.hba = 4;
    CHECK(lipinski_violations(d) == 0);
    CHECK(passes_ro5(d));

    d.mw = 600;
    CHECK(lipinski_violations(d) == 1);
    CHECK(passes_ro5(d));           // 1違反までは許容

    d.logp = 7.0;
    CHECK(lipinski_violations(d) == 2);
    CHECK_FALSE(passes_ro5(d));
}

// =====================================================================
// ファジング（クラッシュしないことの確認）
// =====================================================================

TEST_CASE("parser never crashes on arbitrary input", "[smiles][fuzz]") {
    std::mt19937_64 rng(42);
    const std::string alphabet = "CNOSPFIcnosp()[]=#$:1234567890+-@/\\.% HBrClXyz";

    for (int trial = 0; trial < 5000; ++trial) {
        const std::size_t len = rng() % 40;
        std::string s;
        s.reserve(len);
        for (std::size_t i = 0; i < len; ++i)
            s += alphabet[rng() % alphabet.size()];

        INFO("input: " << s);
        const auto mol = parse_smiles(s);
        if (mol) {
            CHECK_NOTHROW(molecular_weight(*mol));
            CHECK_NOTHROW(morgan_fingerprint(*mol));
            CHECK_NOTHROW(compute_descriptors(*mol));
            CHECK_NOTHROW(ring_count(*mol));
        }
    }
}
