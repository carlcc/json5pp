//
// Example: Custom type serialization with JSON5_DEFINE / JSON5_FIELDS / JSON5_ENUM
//

#include <json5/json5.hpp>
#include <iostream>
#include <optional>
#include <vector>

// ── Intrusive: JSON5_DEFINE inside the struct ────────────────────
struct Person {
    std::string name;
    int age;
    std::optional<std::string> email;

    JSON5_DEFINE(name, age, email)
};

// ── Non-intrusive: JSON5_FIELDS outside the struct ──────────────
struct Point {
    double x;
    double y;
};
JSON5_FIELDS(Point, x, y)

// ── Enum serialization: JSON5_ENUM ──────────────────────────────
enum class Color { Red, Green, Blue, Yellow, Cyan, Magenta };
JSON5_ENUM(Color, Red, Green, Blue, Yellow, Cyan, Magenta)

// ── Struct with json5::value member (schema-free field) ─────────
struct Config {
    std::string name;
    int version;
    json5::value metadata;  // arbitrary JSON5 data, no fixed schema

    JSON5_DEFINE(name, version, metadata)
};

// ── Struct containing an enum field ─────────────────────────────
struct Pixel {
    Point position;
    Color color;

    JSON5_DEFINE(position, color)
};

int main() {
    // Intrusive conversion
    std::cout << "--- Intrusive (JSON5_DEFINE) ---\n";
    Person alice{"Alice", 30, "alice@example.com"};
    json5::value alice_json = json5::to_value(alice);
    std::cout << json5::stringify(alice_json, json5::write_options::pretty()) << "\n";

    Person alice2 = json5::from_value<Person>(alice_json);
    std::cout << "Roundtrip: " << alice2.name << ", " << alice2.age;
    if (alice2.email) std::cout << ", " << *alice2.email;
    std::cout << "\n";

    // Non-intrusive conversion
    std::cout << "\n--- Non-intrusive (JSON5_FIELDS) ---\n";
    Point p{3.14, 2.71};
    json5::value p_json = json5::to_value(p);
    std::cout << json5::stringify(p_json, json5::write_options::pretty()) << "\n";

    Point p2 = json5::from_value<Point>(p_json);
    std::cout << "Roundtrip: (" << p2.x << ", " << p2.y << ")\n";

    // Struct with json5::value member (schema-free field)
    std::cout << "\n--- json5::value as member (schema-free) ---\n";

    // 1) Serialize: Config → json5::value → JSON5 string
    Config cfg;
    cfg.name = "my-app";
    cfg.version = 3;
    // metadata can hold any JSON5 structure — array, object, scalar, etc.
    cfg.metadata = json5::parse(R"({
        tags: ['production', 'v3'],
        debug: false,
        limits: { maxConn: 1024, timeout: 30 }
    })");

    json5::value cfg_json = json5::to_value(cfg);
    std::string cfg_str = json5::stringify(cfg_json, json5::write_options::pretty());
    std::cout << "Serialized:\n" << cfg_str << "\n";

    // 2) Deserialize: JSON5 string → json5::value → Config
    //    Simulate reading from a file / network / config source
    std::string input = R"({
        name: 'web-server',
        version: 5,
        metadata: {
            tags: ['staging', 'canary'],
            debug: true,
            limits: { maxConn: 2048, timeout: 60 },
            extra: [1, 2, 3]
        }
    })";
    Config cfg2 = json5::from_value<Config>(json5::parse(input));

    std::cout << "\nDeserialized from string:\n";
    std::cout << "  name:     " << cfg2.name << "\n";
    std::cout << "  version:  " << cfg2.version << "\n";
    std::cout << "  metadata: " << json5::stringify(cfg2.metadata) << "\n";

    // 3) Re-serialize to verify roundtrip
    std::string cfg2_str = json5::stringify(json5::to_value(cfg2), json5::write_options::pretty_json5());
    std::cout << "\nRe-serialized:\n" << cfg2_str << "\n";

    // Enum conversion
    std::cout << "\n--- Enum (JSON5_ENUM) ---\n";

    Color c = Color::Green;
    json5::value c_json = json5::to_value(c);
    std::cout << "Color::Green → " << json5::stringify(c_json) << "\n";

    Color c2 = json5::from_value<Color>(json5::parse("'Blue'"));
    std::cout << "'Blue' → Color::" << json5::stringify(json5::to_value(c2)) << "\n";

    // Enum in array
    std::vector<Color> palette = {Color::Red, Color::Green, Color::Blue};
    json5::value palette_json = json5::to_value(palette);
    std::cout << "Palette: " << json5::stringify(palette_json) << "\n";

    // Struct with enum member
    std::cout << "\n--- Struct with enum field ---\n";
    Pixel px{{10.5, 20.3}, Color::Magenta};
    json5::value px_json = json5::to_value(px);
    std::cout << json5::stringify(px_json, json5::write_options::pretty_json5()) << "\n";

    Pixel px2 = json5::from_value<Pixel>(px_json);
    std::cout << "Roundtrip: (" << px2.position.x << ", " << px2.position.y
              << ") color=" << json5::stringify(json5::to_value(px2.color)) << "\n";

    return 0;
}
