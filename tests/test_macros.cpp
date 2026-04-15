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
//  Test enums — JSON5_ENUM
// ═══════════════════════════════════════════════════════════════════

enum class Direction { North, South, East, West };
JSON5_ENUM(Direction, North, South, East, West)

enum class LogLevel { Trace, Debug, Info, Warn, Error, Fatal };
JSON5_ENUM(LogLevel, Trace, Debug, Info, Warn, Error, Fatal)

// A single-value enum to test edge case
enum class Singleton { Only };
JSON5_ENUM(Singleton, Only)

// ═══════════════════════════════════════════════════════════════════
//  Test struct — More than 16 fields (unlimited expansion)
// ═══════════════════════════════════════════════════════════════════

struct ManyFields {
    int f1{}, f2{}, f3{}, f4{}, f5{}, f6{}, f7{}, f8{};
    int f9{}, f10{}, f11{}, f12{}, f13{}, f14{}, f15{}, f16{};
    int f17{}, f18{}, f19{}, f20{};

    JSON5_DEFINE(f1, f2, f3, f4, f5, f6, f7, f8,
                 f9, f10, f11, f12, f13, f14, f15, f16,
                 f17, f18, f19, f20)
};

struct ManyFieldsExt {
    int f1{}, f2{}, f3{}, f4{}, f5{}, f6{}, f7{}, f8{};
    int f9{}, f10{}, f11{}, f12{}, f13{}, f14{}, f15{}, f16{};
    int f17{}, f18{}, f19{}, f20{};
};
JSON5_FIELDS(ManyFieldsExt, f1, f2, f3, f4, f5, f6, f7, f8,
             f9, f10, f11, f12, f13, f14, f15, f16,
             f17, f18, f19, f20)

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

// ═══════════════════════════════════════════════════════════════════
//  Tests — Enum
// ═══════════════════════════════════════════════════════════════════

static void test_enum_roundtrip() {
    // to_json5 → string
    auto j_north = json5::to_value(Direction::North);
    ASSERT_EQ(j_north.as_string(), "North");

    auto j_west = json5::to_value(Direction::West);
    ASSERT_EQ(j_west.as_string(), "West");

    // from_json5 → enum
    auto d1 = json5::from_value<Direction>(j_north);
    ASSERT_TRUE(d1 == Direction::North);

    auto d2 = json5::from_value<Direction>(j_west);
    ASSERT_TRUE(d2 == Direction::West);
}

static void test_enum_all_values() {
    // Verify all Direction values roundtrip
    Direction dirs[] = {Direction::North, Direction::South, Direction::East, Direction::West};
    const char* names[] = {"North", "South", "East", "West"};
    for (int i = 0; i < 4; ++i) {
        auto j = json5::to_value(dirs[i]);
        ASSERT_EQ(j.as_string(), names[i]);
        auto d = json5::from_value<Direction>(j);
        ASSERT_TRUE(d == dirs[i]);
    }
}

static void test_enum_six_values() {
    // LogLevel has 6 values
    LogLevel levels[] = {LogLevel::Trace, LogLevel::Debug, LogLevel::Info,
                         LogLevel::Warn, LogLevel::Error, LogLevel::Fatal};
    const char* names[] = {"Trace", "Debug", "Info", "Warn", "Error", "Fatal"};
    for (int i = 0; i < 6; ++i) {
        auto j = json5::to_value(levels[i]);
        ASSERT_EQ(j.as_string(), names[i]);
        auto l = json5::from_value<LogLevel>(j);
        ASSERT_TRUE(l == levels[i]);
    }
}

static void test_enum_unknown_string_throws() {
    // from_json5 with unknown string should throw type_error
    bool threw = false;
    try {
        (void)json5::from_value<Direction>(json5::value("NorthEast"));
    } catch (const json5::type_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

static void test_enum_singleton() {
    auto j = json5::to_value(Singleton::Only);
    ASSERT_EQ(j.as_string(), "Only");
    auto s = json5::from_value<Singleton>(j);
    ASSERT_TRUE(s == Singleton::Only);
}

static void test_enum_as_struct_member() {
    // A struct containing an enum field
    struct WithEnum {
        std::string label;
        Direction dir;
        JSON5_DEFINE(label, dir)
    };

    WithEnum we{"forward", Direction::East};
    auto j = json5::to_value(we);
    ASSERT_EQ(j["label"].as_string(), "forward");
    ASSERT_EQ(j["dir"].as_string(), "East");

    WithEnum we2 = json5::from_value<WithEnum>(j);
    ASSERT_EQ(we2.label, "forward");
    ASSERT_TRUE(we2.dir == Direction::East);
}

static void test_enum_vector() {
    std::vector<Direction> dirs = {Direction::North, Direction::South, Direction::East};
    auto j = json5::to_value(dirs);
    ASSERT_EQ(j.size(), 3u);
    ASSERT_EQ(j[0].as_string(), "North");
    ASSERT_EQ(j[1].as_string(), "South");
    ASSERT_EQ(j[2].as_string(), "East");

    auto dirs2 = json5::from_value<std::vector<Direction>>(j);
    ASSERT_EQ(dirs2.size(), 3u);
    ASSERT_TRUE(dirs2[0] == Direction::North);
    ASSERT_TRUE(dirs2[1] == Direction::South);
    ASSERT_TRUE(dirs2[2] == Direction::East);
}

static void test_enum_parse_json5() {
    auto j = json5::parse(R"({ dir: 'West' })");
    auto d = json5::from_value<Direction>(j["dir"]);
    ASSERT_TRUE(d == Direction::West);
}

// ═══════════════════════════════════════════════════════════════════
//  Tests — More than 16 fields
// ═══════════════════════════════════════════════════════════════════

static void test_many_fields_intrusive() {
    ManyFields m{};
    m.f1 = 1; m.f2 = 2; m.f3 = 3; m.f4 = 4;
    m.f5 = 5; m.f6 = 6; m.f7 = 7; m.f8 = 8;
    m.f9 = 9; m.f10 = 10; m.f11 = 11; m.f12 = 12;
    m.f13 = 13; m.f14 = 14; m.f15 = 15; m.f16 = 16;
    m.f17 = 17; m.f18 = 18; m.f19 = 19; m.f20 = 20;

    auto j = json5::to_value(m);
    ASSERT_EQ(j["f1"].as_integer(), 1);
    ASSERT_EQ(j["f10"].as_integer(), 10);
    ASSERT_EQ(j["f17"].as_integer(), 17);
    ASSERT_EQ(j["f20"].as_integer(), 20);

    ManyFields m2 = json5::from_value<ManyFields>(j);
    ASSERT_EQ(m2.f1, 1);
    ASSERT_EQ(m2.f10, 10);
    ASSERT_EQ(m2.f17, 17);
    ASSERT_EQ(m2.f20, 20);
}

static void test_many_fields_nonintrusive() {
    ManyFieldsExt m{};
    m.f1 = 100; m.f16 = 160; m.f17 = 170; m.f20 = 200;

    auto j = json5::to_value(m);
    ASSERT_EQ(j["f1"].as_integer(), 100);
    ASSERT_EQ(j["f16"].as_integer(), 160);
    ASSERT_EQ(j["f17"].as_integer(), 170);
    ASSERT_EQ(j["f20"].as_integer(), 200);

    ManyFieldsExt m2 = json5::from_value<ManyFieldsExt>(j);
    ASSERT_EQ(m2.f1, 100);
    ASSERT_EQ(m2.f16, 160);
    ASSERT_EQ(m2.f17, 170);
    ASSERT_EQ(m2.f20, 200);
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

    // Enum
    RUN_TEST(enum_roundtrip);
    RUN_TEST(enum_all_values);
    RUN_TEST(enum_six_values);
    RUN_TEST(enum_unknown_string_throws);
    RUN_TEST(enum_singleton);
    RUN_TEST(enum_as_struct_member);
    RUN_TEST(enum_vector);
    RUN_TEST(enum_parse_json5);

    // More than 16 fields
    RUN_TEST(many_fields_intrusive);
    RUN_TEST(many_fields_nonintrusive);

    // Integration
    RUN_TEST(parse_to_struct);
    RUN_TEST(stringify_struct);
    RUN_TEST(vector_of_structs);
    RUN_TEST(missing_field_uses_default);

    std::cout << "\nmacros tests: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
