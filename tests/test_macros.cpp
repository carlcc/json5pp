//
// test_macros.cpp — Unit tests for JSON5_DEFINE and JSON5_FIELDS macros
//

#include <json5/json5.hpp>
#include <cassert>
#include <iostream>
#include <vector>
#include <optional>

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

// ═══════════════════════════════════════════════════════════════════
//  Test structs — Intrusive (JSON5_DEFINE)
// ═══════════════════════════════════════════════════════════════════

struct Person {
    std::string name;
    int age;
    std::optional<std::string> email;

    JSON5_DEFINE(name, age, email)
};

struct Config {
    std::string host;
    int port;
    bool debug;
    std::vector<std::string> tags;

    JSON5_DEFINE(host, port, debug, tags)
};

struct Nested {
    std::string label;
    Person person;

    JSON5_DEFINE(label, person)
};

// ═══════════════════════════════════════════════════════════════════
//  Test structs — Non-intrusive (JSON5_FIELDS)
// ═══════════════════════════════════════════════════════════════════

struct Point {
    double x;
    double y;
};
JSON5_FIELDS(Point, x, y)

struct Rect {
    Point top_left;
    Point bottom_right;
};
JSON5_FIELDS(Rect, top_left, bottom_right)

struct Color {
    int r, g, b;
};
JSON5_FIELDS(Color, r, g, b)

// ═══════════════════════════════════════════════════════════════════
//  Tests — Intrusive
// ═══════════════════════════════════════════════════════════════════

static void test_intrusive_basic() {
    Person p{"Alice", 30, "alice@example.com"};
    auto j = json5::to_value(p);

    ASSERT_EQ(j["name"].as_string(), "Alice");
    ASSERT_EQ(j["age"].as_integer(), 30);
    ASSERT_EQ(j["email"].as_string(), "alice@example.com");

    // Round-trip
    Person p2 = json5::from_value<Person>(j);
    ASSERT_EQ(p2.name, "Alice");
    ASSERT_EQ(p2.age, 30);
    ASSERT_TRUE(p2.email.has_value());
    ASSERT_EQ(*p2.email, "alice@example.com");
}

static void test_intrusive_optional_null() {
    Person p{"Bob", 25, std::nullopt};
    auto j = json5::to_value(p);

    ASSERT_TRUE(j["email"].is_null());

    Person p2 = json5::from_value<Person>(j);
    ASSERT_TRUE(!p2.email.has_value());
}

static void test_intrusive_with_vector() {
    Config c{"localhost", 8080, true, {"web", "api"}};
    auto j = json5::to_value(c);

    ASSERT_EQ(j["host"].as_string(), "localhost");
    ASSERT_EQ(j["port"].as_integer(), 8080);
    ASSERT_EQ(j["debug"].as_bool(), true);
    ASSERT_EQ(j["tags"].size(), 2u);

    Config c2 = json5::from_value<Config>(j);
    ASSERT_EQ(c2.host, "localhost");
    ASSERT_EQ(c2.port, 8080);
    ASSERT_EQ(c2.tags.size(), 2u);
    ASSERT_EQ(c2.tags[0], "web");
}

static void test_intrusive_nested() {
    Nested n{"test", {"Charlie", 35, std::nullopt}};
    auto j = json5::to_value(n);

    ASSERT_EQ(j["label"].as_string(), "test");
    ASSERT_EQ(j["person"]["name"].as_string(), "Charlie");

    Nested n2 = json5::from_value<Nested>(j);
    ASSERT_EQ(n2.label, "test");
    ASSERT_EQ(n2.person.name, "Charlie");
    ASSERT_EQ(n2.person.age, 35);
}

// ═══════════════════════════════════════════════════════════════════
//  Tests — Non-intrusive
// ═══════════════════════════════════════════════════════════════════

static void test_nonintrusive_basic() {
    Point p{3.14, 2.71};
    auto j = json5::to_value(p);

    ASSERT_NEAR(j["x"].as_double(), 3.14, 1e-10);
    ASSERT_NEAR(j["y"].as_double(), 2.71, 1e-10);

    Point p2 = json5::from_value<Point>(j);
    ASSERT_NEAR(p2.x, 3.14, 1e-10);
    ASSERT_NEAR(p2.y, 2.71, 1e-10);
}

static void test_nonintrusive_nested() {
    Rect r{
        {0.0, 10.0},
        {100.0, 0.0}
    };
    auto j = json5::to_value(r);

    ASSERT_NEAR(j["top_left"]["x"].as_double(), 0.0, 1e-10);
    ASSERT_NEAR(j["top_left"]["y"].as_double(), 10.0, 1e-10);
    ASSERT_NEAR(j["bottom_right"]["x"].as_double(), 100.0, 1e-10);

    Rect r2 = json5::from_value<Rect>(j);
    ASSERT_NEAR(r2.top_left.x, 0.0, 1e-10);
    ASSERT_NEAR(r2.bottom_right.x, 100.0, 1e-10);
}

static void test_nonintrusive_three_fields() {
    Color c{255, 128, 0};
    auto j = json5::to_value(c);

    ASSERT_EQ(j["r"].as_integer(), 255);
    ASSERT_EQ(j["g"].as_integer(), 128);
    ASSERT_EQ(j["b"].as_integer(), 0);

    Color c2 = json5::from_value<Color>(j);
    ASSERT_EQ(c2.r, 255);
    ASSERT_EQ(c2.g, 128);
    ASSERT_EQ(c2.b, 0);
}

// ═══════════════════════════════════════════════════════════════════
//  Tests — Parse JSON5 into custom types
// ═══════════════════════════════════════════════════════════════════

static void test_parse_to_struct() {
    auto j = json5::parse(R"({
        name: 'Dave',
        age: 28,
        email: 'dave@example.com',
    })");

    Person p = json5::from_value<Person>(j);
    ASSERT_EQ(p.name, "Dave");
    ASSERT_EQ(p.age, 28);
    ASSERT_TRUE(p.email.has_value());
    ASSERT_EQ(*p.email, "dave@example.com");
}

static void test_stringify_struct() {
    Person p{"Eve", 22, std::nullopt};
    auto j = json5::to_value(p);
    auto s = json5::stringify(j, json5::write_options::pretty());

    // Parse back
    auto j2 = json5::parse(s);
    Person p2 = json5::from_value<Person>(j2);
    ASSERT_EQ(p2.name, "Eve");
    ASSERT_EQ(p2.age, 22);
    ASSERT_TRUE(!p2.email.has_value());
}

static void test_vector_of_structs() {
    std::vector<Point> points = {{1.0, 2.0}, {3.0, 4.0}};
    auto j = json5::to_value(points);
    auto points2 = json5::from_value<std::vector<Point>>(j);

    ASSERT_EQ(points2.size(), 2u);
    ASSERT_NEAR(points2[0].x, 1.0, 1e-10);
    ASSERT_NEAR(points2[1].y, 4.0, 1e-10);
}

static void test_missing_field_uses_default() {
    // When a field is missing from JSON, from_json5 should leave it default-initialized
    auto j = json5::parse(R"({name: 'Sparse'})");

    Person p = json5::from_value<Person>(j);
    ASSERT_EQ(p.name, "Sparse");
    ASSERT_EQ(p.age, 0);  // default initialized
    ASSERT_TRUE(!p.email.has_value());
}

// ── Entry point ───────────────────────────────────────────────────

int main() {
    // Intrusive
    RUN_TEST(intrusive_basic);
    RUN_TEST(intrusive_optional_null);
    RUN_TEST(intrusive_with_vector);
    RUN_TEST(intrusive_nested);

    // Non-intrusive
    RUN_TEST(nonintrusive_basic);
    RUN_TEST(nonintrusive_nested);
    RUN_TEST(nonintrusive_three_fields);

    // Integration
    RUN_TEST(parse_to_struct);
    RUN_TEST(stringify_struct);
    RUN_TEST(vector_of_structs);
    RUN_TEST(missing_field_uses_default);

    std::cout << "\nmacros tests: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
