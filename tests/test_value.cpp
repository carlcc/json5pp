//
// test_value.cpp — Unit tests for json5::value
//

#include <json5/json5.hpp>
#include <cassert>
#include <iostream>
#include <cmath>

#define TEST(name) \
    static void test_##name(); \
    struct test_reg_##name { test_reg_##name() { tests.push_back({#name, test_##name}); } } reg_##name; \
    static void test_##name()

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

#define ASSERT_FALSE(x) ASSERT_TRUE(!(x))

#define ASSERT_THROWS(expr, exc_type) do { \
    bool caught = false; \
    try { (void)(expr); } catch (const exc_type&) { caught = true; } \
    if (!caught) { \
        std::cerr << "FAIL: expected " << #exc_type << " from " << #expr << " at line " << __LINE__ << "\n"; \
        assert(false); \
    } \
} while(0)

static std::vector<std::pair<const char*, void(*)()>> tests;

// ── Null ──────────────────────────────────────────────────────────
TEST(null_default) {
    json5::value v;
    ASSERT_TRUE(v.is_null());
    ASSERT_EQ(v.get_type(), json5::value_type::null);
}

TEST(null_explicit) {
    json5::value v(nullptr);
    ASSERT_TRUE(v.is_null());
}

// ── Boolean ───────────────────────────────────────────────────────
TEST(bool_true) {
    json5::value v(true);
    ASSERT_TRUE(v.is_bool());
    ASSERT_EQ(v.as_bool(), true);
    ASSERT_EQ(v.get<bool>(), true);
}

TEST(bool_false) {
    json5::value v(false);
    ASSERT_TRUE(v.is_bool());
    ASSERT_EQ(v.as_bool(), false);
}

// ── Integer ───────────────────────────────────────────────────────
TEST(integer_positive) {
    json5::value v(42);
    ASSERT_TRUE(v.is_integer());
    ASSERT_TRUE(v.is_number());
    ASSERT_EQ(v.as_integer(), 42);
    ASSERT_EQ(v.get<int>(), 42);
}

TEST(integer_negative) {
    json5::value v(-100);
    ASSERT_TRUE(v.is_integer());
    ASSERT_EQ(v.as_integer(), -100);
}

TEST(integer_64bit) {
    int64_t big = 9'000'000'000'000'000'000LL;
    json5::value v(big);
    ASSERT_EQ(v.as_integer(), big);
}

// ── Floating ──────────────────────────────────────────────────────
TEST(floating_basic) {
    json5::value v(3.14);
    ASSERT_TRUE(v.is_floating());
    ASSERT_TRUE(v.is_number());
    ASSERT_TRUE(std::abs(v.as_double() - 3.14) < 1e-10);
}

TEST(integer_as_double) {
    json5::value v(42);
    ASSERT_TRUE(std::abs(v.as_double() - 42.0) < 1e-10);
}

// ── String ────────────────────────────────────────────────────────
TEST(string_basic) {
    json5::value v("hello");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string(), "hello");
}

TEST(string_std) {
    std::string s = "world";
    json5::value v(s);
    ASSERT_EQ(v.as_string(), "world");
}

// ── Array ─────────────────────────────────────────────────────────
TEST(array_basic) {
    json5::value v = json5::value::array({1, 2, 3});
    ASSERT_TRUE(v.is_array());
    ASSERT_EQ(v.size(), 3u);
    ASSERT_EQ(v[0].as_integer(), 1);
    ASSERT_EQ(v[1].as_integer(), 2);
    ASSERT_EQ(v[2].as_integer(), 3);
}

TEST(array_empty) {
    json5::value v = json5::value::array();
    ASSERT_TRUE(v.is_array());
    ASSERT_TRUE(v.empty());
    ASSERT_EQ(v.size(), 0u);
}

TEST(array_push_back) {
    json5::value v = json5::value::array();
    v.push_back(1);
    v.push_back("hello");
    ASSERT_EQ(v.size(), 2u);
    ASSERT_EQ(v[0].as_integer(), 1);
    ASSERT_EQ(v[1].as_string(), "hello");
}

// ── Object ────────────────────────────────────────────────────────
TEST(object_basic) {
    json5::value v = json5::value::object({{"name", "Alice"}, {"age", 30}});
    ASSERT_TRUE(v.is_object());
    ASSERT_EQ(v.size(), 2u);
    ASSERT_EQ(v["name"].as_string(), "Alice");
    ASSERT_EQ(v["age"].as_integer(), 30);
}

TEST(object_empty) {
    json5::value v = json5::value::object();
    ASSERT_TRUE(v.is_object());
    ASSERT_TRUE(v.empty());
}

TEST(object_subscript_create) {
    json5::value v;
    v["key"] = "value";
    ASSERT_TRUE(v.is_object());
    ASSERT_EQ(v["key"].as_string(), "value");
}

TEST(object_contains) {
    json5::value v = json5::value::object({{"a", 1}});
    ASSERT_TRUE(v.contains("a"));
    ASSERT_FALSE(v.contains("b"));
}

TEST(object_insert_and_erase) {
    json5::value v = json5::value::object();
    v.insert("x", 10);
    ASSERT_TRUE(v.contains("x"));
    ASSERT_EQ(v["x"].as_integer(), 10);

    v.insert("x", 20);  // overwrite
    ASSERT_EQ(v["x"].as_integer(), 20);

    ASSERT_TRUE(v.erase("x"));
    ASSERT_FALSE(v.contains("x"));
    ASSERT_FALSE(v.erase("nonexistent"));
}

// ── Copy/Move ─────────────────────────────────────────────────────
TEST(copy) {
    json5::value v = json5::value::object({{"a", 1}});
    json5::value v2 = v;
    ASSERT_EQ(v, v2);
    v2["a"] = 2;
    ASSERT_EQ(v["a"].as_integer(), 1);
    ASSERT_EQ(v2["a"].as_integer(), 2);
}

TEST(move) {
    json5::value v = json5::value::array({1, 2, 3});
    json5::value v2 = std::move(v);
    ASSERT_TRUE(v2.is_array());
    ASSERT_EQ(v2.size(), 3u);
}

// ── Type errors ───────────────────────────────────────────────────
TEST(type_error_access) {
    json5::value v(42);
    ASSERT_THROWS(v.as_string(), json5::type_error);
    ASSERT_THROWS(v.as_array(), json5::type_error);
    ASSERT_THROWS(v.as_object(), json5::type_error);
    ASSERT_THROWS(v.as_bool(), json5::type_error);
}

// ── Comparison ────────────────────────────────────────────────────
TEST(equality) {
    ASSERT_TRUE(json5::value(42) == json5::value(42));
    ASSERT_TRUE(json5::value("hi") == json5::value("hi"));
    ASSERT_FALSE(json5::value(42) == json5::value(43));
    ASSERT_TRUE(json5::value(42) == json5::value(42.0));  // int vs double
}

// ── Entry point ───────────────────────────────────────────────────
int main() {
    int passed = 0, failed = 0;
    for (auto& [name, fn] : tests) {
        try {
            fn();
            ++passed;
            std::cout << "  PASS: " << name << "\n";
        } catch (...) {
            ++failed;
            std::cout << "  FAIL: " << name << "\n";
        }
    }
    std::cout << "\nvalue tests: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
