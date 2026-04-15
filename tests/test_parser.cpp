//
// test_parser.cpp — Unit tests for JSON5 parser
//

#include <json5/json5.hpp>
#include <cassert>
#include <iostream>
#include <cmath>

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

#define ASSERT_THROWS(expr, exc_type) do { \
    bool caught = false; \
    try { (void)(expr); } catch (const exc_type&) { caught = true; } \
    if (!caught) { \
        std::cerr << "FAIL: expected " << #exc_type << " from " << #expr << " at line " << __LINE__ << "\n"; \
        assert(false); \
    } \
} while(0)

static int passed = 0, failed = 0;

#define RUN_TEST(name) do { \
    try { test_##name(); ++passed; std::cout << "  PASS: " << #name << "\n"; } \
    catch (...) { ++failed; std::cout << "  FAIL: " << #name << "\n"; } \
} while(0)

// ── Basic JSON ────────────────────────────────────────────────────

static void test_parse_null() {
    auto v = json5::parse("null");
    ASSERT_TRUE(v.is_null());
}

static void test_parse_true() {
    auto v = json5::parse("true");
    ASSERT_TRUE(v.is_bool());
    ASSERT_EQ(v.as_bool(), true);
}

static void test_parse_false() {
    auto v = json5::parse("false");
    ASSERT_EQ(v.as_bool(), false);
}

static void test_parse_integer() {
    auto v = json5::parse("42");
    ASSERT_TRUE(v.is_integer());
    ASSERT_EQ(v.as_integer(), 42);
}

static void test_parse_negative_integer() {
    auto v = json5::parse("-123");
    ASSERT_EQ(v.as_integer(), -123);
}

static void test_parse_float() {
    auto v = json5::parse("3.14");
    ASSERT_TRUE(v.is_floating() || v.is_number());
    ASSERT_NEAR(v.as_double(), 3.14, 1e-10);
}

static void test_parse_exponent() {
    auto v = json5::parse("1.5e3");
    ASSERT_NEAR(v.as_double(), 1500.0, 1e-10);
}

static void test_parse_string_double_quote() {
    auto v = json5::parse("\"hello world\"");
    ASSERT_EQ(v.as_string(), "hello world");
}

static void test_parse_string_escapes() {
    auto v = json5::parse("\"line1\\nline2\\ttab\"");
    ASSERT_EQ(v.as_string(), "line1\nline2\ttab");
}

static void test_parse_empty_array() {
    auto v = json5::parse("[]");
    ASSERT_TRUE(v.is_array());
    ASSERT_EQ(v.size(), 0u);
}

static void test_parse_array() {
    auto v = json5::parse("[1, 2, 3]");
    ASSERT_EQ(v.size(), 3u);
    ASSERT_EQ(v[0].as_integer(), 1);
    ASSERT_EQ(v[2].as_integer(), 3);
}

static void test_parse_empty_object() {
    auto v = json5::parse("{}");
    ASSERT_TRUE(v.is_object());
    ASSERT_EQ(v.size(), 0u);
}

static void test_parse_object() {
    auto v = json5::parse("{\"name\": \"Alice\", \"age\": 30}");
    ASSERT_EQ(v["name"].as_string(), "Alice");
    ASSERT_EQ(v["age"].as_integer(), 30);
}

static void test_parse_nested() {
    auto v = json5::parse("{\"arr\": [1, {\"x\": true}], \"obj\": {\"y\": null}}");
    ASSERT_EQ(v["arr"][0].as_integer(), 1);
    ASSERT_EQ(v["arr"][1]["x"].as_bool(), true);
    ASSERT_TRUE(v["obj"]["y"].is_null());
}

// ── JSON5 features ────────────────────────────────────────────────

static void test_single_line_comment() {
    auto v = json5::parse(R"(
        // This is a comment
        42
    )");
    ASSERT_EQ(v.as_integer(), 42);
}

static void test_multi_line_comment() {
    auto v = json5::parse(R"(
        /* This is
           a multi-line comment */
        "hello"
    )");
    ASSERT_EQ(v.as_string(), "hello");
}

static void test_trailing_comma_array() {
    auto v = json5::parse("[1, 2, 3,]");
    ASSERT_EQ(v.size(), 3u);
}

static void test_trailing_comma_object() {
    auto v = json5::parse("{\"a\": 1, \"b\": 2,}");
    ASSERT_EQ(v.size(), 2u);
}

static void test_unquoted_keys() {
    auto v = json5::parse("{name: \"Alice\", age: 30}");
    ASSERT_EQ(v["name"].as_string(), "Alice");
    ASSERT_EQ(v["age"].as_integer(), 30);
}

static void test_single_quote_string() {
    auto v = json5::parse("'hello world'");
    ASSERT_EQ(v.as_string(), "hello world");
}

static void test_hex_number() {
    auto v = json5::parse("0xFF");
    ASSERT_EQ(v.as_integer(), 255);
}

static void test_hex_negative() {
    auto v = json5::parse("-0x1A");
    ASSERT_EQ(v.as_integer(), -26);
}

static void test_positive_sign() {
    auto v = json5::parse("+42");
    ASSERT_EQ(v.as_integer(), 42);
}

static void test_infinity() {
    auto v = json5::parse("Infinity");
    ASSERT_TRUE(std::isinf(v.as_double()));
    ASSERT_TRUE(v.as_double() > 0);
}

static void test_negative_infinity() {
    auto v = json5::parse("-Infinity");
    ASSERT_TRUE(std::isinf(v.as_double()));
    ASSERT_TRUE(v.as_double() < 0);
}

static void test_nan() {
    auto v = json5::parse("NaN");
    ASSERT_TRUE(std::isnan(v.as_double()));
}

static void test_leading_dot_number() {
    auto v = json5::parse(".5");
    ASSERT_NEAR(v.as_double(), 0.5, 1e-10);
}

static void test_unicode_escape() {
    auto v = json5::parse("\"\\u0041\"");
    ASSERT_EQ(v.as_string(), "A");
}

// ── Complex JSON5 document ────────────────────────────────────────

static void test_complex_document() {
    auto v = json5::parse(R"({
        // Configuration
        appName: 'MyApp',
        version: 2,
        debug: true,
        database: {
            host: 'localhost',
            port: 5432,
            options: {
                timeout: 30,
                retries: 3,
            },
        },
        tags: ['production', 'v2',],
        metadata: null,
    })");

    ASSERT_EQ(v["appName"].as_string(), "MyApp");
    ASSERT_EQ(v["version"].as_integer(), 2);
    ASSERT_EQ(v["debug"].as_bool(), true);
    ASSERT_EQ(v["database"]["host"].as_string(), "localhost");
    ASSERT_EQ(v["database"]["port"].as_integer(), 5432);
    ASSERT_EQ(v["database"]["options"]["timeout"].as_integer(), 30);
    ASSERT_EQ(v["tags"].size(), 2u);
    ASSERT_EQ(v["tags"][0].as_string(), "production");
    ASSERT_TRUE(v["metadata"].is_null());
}

// ── Error cases ───────────────────────────────────────────────────

static void test_parse_error_unexpected() {
    ASSERT_THROWS(json5::parse("{"), json5::parse_error);
}

static void test_parse_error_trailing() {
    ASSERT_THROWS(json5::parse("42 extra"), json5::parse_error);
}

static void test_try_parse_error() {
    std::string err;
    auto result = json5::try_parse("{bad", &err);
    ASSERT_TRUE(!result.has_value());
    ASSERT_TRUE(!err.empty());
}

// ── Extension ─────────────────────────────────────────────────────

static void test_custom_keyword() {
    auto opts = json5::make_keyword_extension({
        {"undefined", json5::value(nullptr)},
        {"PI",        json5::value(3.14159)},
    });

    auto v = json5::parse("{a: undefined, b: PI}", opts);
    ASSERT_TRUE(v["a"].is_null());
    ASSERT_NEAR(v["b"].as_double(), 3.14159, 1e-5);
}

// ── Entry point ───────────────────────────────────────────────────

int main() {
    RUN_TEST(parse_null);
    RUN_TEST(parse_true);
    RUN_TEST(parse_false);
    RUN_TEST(parse_integer);
    RUN_TEST(parse_negative_integer);
    RUN_TEST(parse_float);
    RUN_TEST(parse_exponent);
    RUN_TEST(parse_string_double_quote);
    RUN_TEST(parse_string_escapes);
    RUN_TEST(parse_empty_array);
    RUN_TEST(parse_array);
    RUN_TEST(parse_empty_object);
    RUN_TEST(parse_object);
    RUN_TEST(parse_nested);

    // JSON5 features
    RUN_TEST(single_line_comment);
    RUN_TEST(multi_line_comment);
    RUN_TEST(trailing_comma_array);
    RUN_TEST(trailing_comma_object);
    RUN_TEST(unquoted_keys);
    RUN_TEST(single_quote_string);
    RUN_TEST(hex_number);
    RUN_TEST(hex_negative);
    RUN_TEST(positive_sign);
    RUN_TEST(infinity);
    RUN_TEST(negative_infinity);
    RUN_TEST(nan);
    RUN_TEST(leading_dot_number);
    RUN_TEST(unicode_escape);
    RUN_TEST(complex_document);

    // Error cases
    RUN_TEST(parse_error_unexpected);
    RUN_TEST(parse_error_trailing);
    RUN_TEST(try_parse_error);

    // Extension
    RUN_TEST(custom_keyword);

    std::cout << "\nparser tests: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
