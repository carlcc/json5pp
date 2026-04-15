//
// test_writer.cpp — Unit tests for JSON5 writer/serializer
//

#include <json5/json5.hpp>
#include <cassert>
#include <iostream>
#include <cmath>

#define ASSERT_EQ(a, b) do { \
    if (!((a) == (b))) { \
        std::cerr << "FAIL: " << #a << " == " << #b << "\n  got: " << (a) << "\n  at line " << __LINE__ << "\n"; \
        assert(false); \
    } \
} while(0)

#define ASSERT_TRUE(x) do { \
    if (!(x)) { \
        std::cerr << "FAIL: " << #x << " at line " << __LINE__ << "\n"; \
        assert(false); \
    } \
} while(0)

static int passed = 0, failed = 0;

#define RUN_TEST(name) do { \
    try { test_##name(); ++passed; std::cout << "  PASS: " << #name << "\n"; } \
    catch (...) { ++failed; std::cout << "  FAIL: " << #name << "\n"; } \
} while(0)

// ── Basic types ───────────────────────────────────────────────────

static void test_write_null() {
    ASSERT_EQ(json5::stringify(json5::value(nullptr)), "null");
}

static void test_write_true() {
    ASSERT_EQ(json5::stringify(json5::value(true)), "true");
}

static void test_write_false() {
    ASSERT_EQ(json5::stringify(json5::value(false)), "false");
}

static void test_write_integer() {
    ASSERT_EQ(json5::stringify(json5::value(42)), "42");
}

static void test_write_negative() {
    ASSERT_EQ(json5::stringify(json5::value(-123)), "-123");
}

static void test_write_double() {
    auto s = json5::stringify(json5::value(3.14));
    // Should parse back to the same value
    auto v = json5::parse(s);
    ASSERT_TRUE(std::abs(v.as_double() - 3.14) < 1e-10);
}

static void test_write_infinity() {
    ASSERT_EQ(json5::stringify(json5::value(std::numeric_limits<double>::infinity())), "Infinity");
    ASSERT_EQ(json5::stringify(json5::value(-std::numeric_limits<double>::infinity())), "-Infinity");
}

static void test_write_nan() {
    ASSERT_EQ(json5::stringify(json5::value(std::numeric_limits<double>::quiet_NaN())), "NaN");
}

static void test_write_string() {
    ASSERT_EQ(json5::stringify(json5::value("hello")), "\"hello\"");
}

static void test_write_string_escapes() {
    auto v = json5::value("line1\nline2\ttab\\slash\"quote");
    auto s = json5::stringify(v);
    // Parse back and verify
    auto v2 = json5::parse(s);
    ASSERT_EQ(v2.as_string(), v.as_string());
}

static void test_write_empty_array() {
    ASSERT_EQ(json5::stringify(json5::value::array()), "[]");
}

static void test_write_array() {
    auto s = json5::stringify(json5::value::array({1, 2, 3}));
    ASSERT_EQ(s, "[1,2,3]");
}

static void test_write_empty_object() {
    ASSERT_EQ(json5::stringify(json5::value::object()), "{}");
}

static void test_write_object() {
    auto v = json5::value::object({{"a", 1}});
    auto s = json5::stringify(v);
    ASSERT_EQ(s, "{\"a\":1}");
}

// ── Pretty printing ───────────────────────────────────────────────

static void test_pretty_object() {
    auto v = json5::value::object({{"name", "test"}, {"value", 42}});
    auto s = json5::stringify(v, json5::write_options::pretty());
    ASSERT_TRUE(s.find('\n') != std::string::npos);
    ASSERT_TRUE(s.find("  ") != std::string::npos);
    // Parse back
    auto v2 = json5::parse(s);
    ASSERT_EQ(v2["name"].as_string(), "test");
    ASSERT_EQ(v2["value"].as_integer(), 42);
}

// ── JSON5 style ───────────────────────────────────────────────────

static void test_pretty_json5_single_quotes() {
    auto v = json5::value("hello");
    auto s = json5::stringify(v, json5::write_options::pretty_json5());
    ASSERT_TRUE(s.find('\'') != std::string::npos);
}

static void test_pretty_json5_unquoted_keys() {
    auto v = json5::value::object({{"name", "test"}});
    auto opts = json5::write_options::pretty_json5();
    auto s = json5::stringify(v, opts);
    // "name" should not be quoted (it's a valid identifier)
    ASSERT_TRUE(s.find("name:") != std::string::npos || s.find("name :") != std::string::npos);
}

static void test_pretty_json5_trailing_comma() {
    auto v = json5::value::array({1, 2});
    auto opts = json5::write_options::pretty_json5();
    auto s = json5::stringify(v, opts);
    ASSERT_TRUE(s.find(",\n]") != std::string::npos || s.find(",]") != std::string::npos);
}

static void test_compact_json5() {
    auto v = json5::value::object({{"name", "test"}, {"val", 42}});
    auto s = json5::stringify(v, json5::write_options::compact_json5());
    // Should be minified (no newlines)
    ASSERT_TRUE(s.find('\n') == std::string::npos);
    // Should use single quotes
    ASSERT_TRUE(s.find('\'') != std::string::npos);
    // Keys should be unquoted
    ASSERT_TRUE(s.find("name:") != std::string::npos);
}

// ── Round-trip ────────────────────────────────────────────────────

static void test_roundtrip_complex() {
    auto input = R"({
        name: 'test',
        values: [1, 2.5, true, null, 'hello'],
        nested: {
            inner: [1, 2, 3,],
        },
    })";

    auto v1 = json5::parse(input);
    auto s = json5::stringify(v1, json5::write_options::pretty());
    auto v2 = json5::parse(s);

    ASSERT_EQ(v1["name"].as_string(), v2["name"].as_string());
    ASSERT_EQ(v1["values"].size(), v2["values"].size());
    ASSERT_EQ(v1["nested"]["inner"].size(), v2["nested"]["inner"].size());
}

// ── Entry point ───────────────────────────────────────────────────

int main() {
    RUN_TEST(write_null);
    RUN_TEST(write_true);
    RUN_TEST(write_false);
    RUN_TEST(write_integer);
    RUN_TEST(write_negative);
    RUN_TEST(write_double);
    RUN_TEST(write_infinity);
    RUN_TEST(write_nan);
    RUN_TEST(write_string);
    RUN_TEST(write_string_escapes);
    RUN_TEST(write_empty_array);
    RUN_TEST(write_array);
    RUN_TEST(write_empty_object);
    RUN_TEST(write_object);
    RUN_TEST(pretty_object);
    RUN_TEST(pretty_json5_single_quotes);
    RUN_TEST(pretty_json5_unquoted_keys);
    RUN_TEST(pretty_json5_trailing_comma);
    RUN_TEST(compact_json5);
    RUN_TEST(roundtrip_complex);

    std::cout << "\nwriter tests: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
