//
// Example: Serializing values to JSON5 / JSON strings
//

#include <json5/json5.hpp>
#include <iostream>

int main() {
    auto val = json5::parse(R"({
        name: 'json5pp',
        version: 1,
        features: ['comments', 'trailing commas'],
        metadata: { hex: 0xFF, infinity: Infinity },
    })");

    // Compact output (default)
    std::cout << "--- compact ---\n";
    std::cout << json5::stringify(val) << "\n";

    // Pretty-printed JSON
    std::cout << "\n--- pretty ---\n";
    std::cout << json5::stringify(val, json5::write_options::pretty()) << "\n";

    // JSON5 style — pretty (unquoted keys, single-quoted strings, trailing commas)
    std::cout << "\n--- pretty json5 ---\n";
    std::cout << json5::stringify(val, json5::write_options::pretty_json5()) << "\n";

    // JSON5 style — compact (minified, unquoted keys, single-quoted strings)
    std::cout << "\n--- compact json5 ---\n";
    std::cout << json5::stringify(val, json5::write_options::compact_json5()) << "\n";

    return 0;
}
