//
// test_converter.cpp — Unit tests for type converter
//

#include <json5/json5.hpp>
#include <cassert>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <optional>
#include <variant>
#include <tuple>
#include <deque>
#include <list>
#include <array>

#define ASSERT_EQ(a, b) do { \
    if (!((a) == (b))) { \
        std::cerr << "FAIL: " << #a << " == " << #b << " at line " << __LINE__ << "\n"; \
        assert(false); \
    } \
} while(0)

#define ASSERT_TRUE(x) do { \
    if (!(x)) { \
        std::cerr << "FAIL: " << #x << " at line " << __LINE__ << "\n"; \
        assert(false); \
    } \
} while(0)

#define ASSERT_NEAR(a, b, eps) do { \
    if (std::abs((a) - (b)) > (eps)) { \
        std::cerr << "FAIL: " << #a << " ~= " << #b << " at line " << __LINE__ << "\n"; \
        assert(false); \
    } \
} while(0)

static int passed = 0, failed = 0;

#define RUN_TEST(name) do { \
    try { test_##name(); ++passed; std::cout << "  PASS: " << #name << "\n"; } \
    catch (const std::exception& e) { ++failed; std::cout << "  FAIL: " << #name << " (" << e.what() << ")\n"; } \
    catch (...) { ++failed; std::cout << "  FAIL: " << #name << "\n"; } \
} while(0)

// ── Basic types ───────────────────────────────────────────────────

static void test_bool() {
    auto j = json5::to_value(true);
    ASSERT_EQ(json5::from_value<bool>(j), true);

    j = json5::to_value(false);
    ASSERT_EQ(json5::from_value<bool>(j), false);
}

static void test_int() {
    auto j = json5::to_value(42);
    ASSERT_EQ(json5::from_value<int>(j), 42);
}

static void test_int64() {
    int64_t big = 9'000'000'000'000LL;
    auto j = json5::to_value(big);
    ASSERT_EQ(json5::from_value<int64_t>(j), big);
}

static void test_double() {
    auto j = json5::to_value(3.14);
    ASSERT_NEAR(json5::from_value<double>(j), 3.14, 1e-10);
}

static void test_string() {
    auto j = json5::to_value(std::string("hello"));
    ASSERT_EQ(json5::from_value<std::string>(j), "hello");
}

// ── Containers ────────────────────────────────────────────────────

static void test_vector() {
    std::vector<int> v = {1, 2, 3, 4, 5};
    auto j = json5::to_value(v);
    auto v2 = json5::from_value<std::vector<int>>(j);
    ASSERT_EQ(v, v2);
}

static void test_vector_string() {
    std::vector<std::string> v = {"hello", "world"};
    auto j = json5::to_value(v);
    auto v2 = json5::from_value<std::vector<std::string>>(j);
    ASSERT_EQ(v, v2);
}

static void test_deque() {
    std::deque<int> d = {10, 20, 30};
    auto j = json5::to_value(d);
    auto d2 = json5::from_value<std::deque<int>>(j);
    ASSERT_EQ(d, d2);
}

static void test_list() {
    std::list<int> l = {1, 2, 3};
    auto j = json5::to_value(l);
    auto l2 = json5::from_value<std::list<int>>(j);
    ASSERT_EQ(l, l2);
}

static void test_std_array() {
    std::array<int, 3> a = {1, 2, 3};
    auto j = json5::to_value(a);
    auto a2 = json5::from_value<std::array<int, 3>>(j);
    ASSERT_EQ(a, a2);
}

static void test_set() {
    std::set<int> s = {3, 1, 4, 1, 5};
    auto j = json5::to_value(s);
    auto s2 = json5::from_value<std::set<int>>(j);
    ASSERT_EQ(s, s2);
}

static void test_map() {
    std::map<std::string, int> m = {{"a", 1}, {"b", 2}};
    auto j = json5::to_value(m);
    auto m2 = json5::from_value<std::map<std::string, int>>(j);
    ASSERT_EQ(m, m2);
}

static void test_unordered_map() {
    std::unordered_map<std::string, int> m = {{"x", 10}, {"y", 20}};
    auto j = json5::to_value(m);
    auto m2 = json5::from_value<std::unordered_map<std::string, int>>(j);
    ASSERT_EQ(m, m2);
}

// ── Optional ──────────────────────────────────────────────────────

static void test_optional_value() {
    std::optional<int> o = 42;
    auto j = json5::to_value(o);
    auto o2 = json5::from_value<std::optional<int>>(j);
    ASSERT_TRUE(o2.has_value());
    ASSERT_EQ(*o2, 42);
}

static void test_optional_null() {
    std::optional<int> o = std::nullopt;
    auto j = json5::to_value(o);
    ASSERT_TRUE(j.is_null());
    auto o2 = json5::from_value<std::optional<int>>(j);
    ASSERT_TRUE(!o2.has_value());
}

// ── Pair ──────────────────────────────────────────────────────────

static void test_pair() {
    auto p = std::make_pair(std::string("key"), 42);
    auto j = json5::to_value(p);
    auto p2 = json5::from_value<std::pair<std::string, int>>(j);
    ASSERT_EQ(p, p2);
}

// ── Tuple ─────────────────────────────────────────────────────────

static void test_tuple() {
    auto t = std::make_tuple(1, std::string("hello"), 3.14);
    auto j = json5::to_value(t);
    auto t2 = json5::from_value<std::tuple<int, std::string, double>>(j);
    ASSERT_EQ(std::get<0>(t2), 1);
    ASSERT_EQ(std::get<1>(t2), "hello");
    ASSERT_NEAR(std::get<2>(t2), 3.14, 1e-10);
}

// ── Variant ───────────────────────────────────────────────────────

static void test_variant() {
    using V = std::variant<int, std::string>;

    V v1 = 42;
    auto j1 = json5::to_value(v1);
    auto v1_back = json5::from_value<V>(j1);
    ASSERT_EQ(std::get<int>(v1_back), 42);

    V v2 = std::string("hello");
    auto j2 = json5::to_value(v2);
    auto v2_back = json5::from_value<V>(j2);
    ASSERT_EQ(std::get<std::string>(v2_back), "hello");
}

// ── Nested containers ─────────────────────────────────────────────

static void test_nested_vector() {
    std::vector<std::vector<int>> v = {{1, 2}, {3, 4, 5}};
    auto j = json5::to_value(v);
    auto v2 = json5::from_value<std::vector<std::vector<int>>>(j);
    ASSERT_EQ(v, v2);
}

static void test_map_of_vectors() {
    std::map<std::string, std::vector<int>> m = {
        {"odds", {1, 3, 5}},
        {"evens", {2, 4, 6}}
    };
    auto j = json5::to_value(m);
    auto m2 = json5::from_value<std::map<std::string, std::vector<int>>>(j);
    ASSERT_EQ(m, m2);
}

// ── Entry point ───────────────────────────────────────────────────

int main() {
    RUN_TEST(bool);
    RUN_TEST(int);
    RUN_TEST(int64);
    RUN_TEST(double);
    RUN_TEST(string);
    RUN_TEST(vector);
    RUN_TEST(vector_string);
    RUN_TEST(deque);
    RUN_TEST(list);
    RUN_TEST(std_array);
    RUN_TEST(set);
    RUN_TEST(map);
    RUN_TEST(unordered_map);
    RUN_TEST(optional_value);
    RUN_TEST(optional_null);
    RUN_TEST(pair);
    RUN_TEST(tuple);
    RUN_TEST(variant);
    RUN_TEST(nested_vector);
    RUN_TEST(map_of_vectors);

    std::cout << "\nconverter tests: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
