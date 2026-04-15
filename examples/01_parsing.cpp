//
// Example: Parsing JSON5 documents
//

#include <json5/json5.hpp>
#include <iostream>

int main() {
    // Parse a JSON5 document with comments, trailing commas,
    // unquoted keys, single-quoted strings, and special numbers.
    auto val = json5::parse(R"({
        // This is a JSON5 document
        name: 'json5pp',
        version: 1,
        features: [
            'comments',
            'trailing commas',
            'unquoted keys',
            'single quotes',
        ],
        metadata: {
            hex: 0xFF,
            infinity: Infinity,
            nan: NaN,
        },
    })");

    std::cout << "name: " << val["name"].as_string() << "\n";
    std::cout << "version: " << val["version"].as_integer() << "\n";
    std::cout << "features count: " << val["features"].size() << "\n";
    std::cout << "hex value: " << val["metadata"]["hex"].as_integer() << "\n";

    // ── Error handling with try_parse ────────────────────────────
    std::string err;
    auto bad = json5::try_parse("{invalid json5", &err);
    if (!bad) {
        std::cout << "\nParse failed (expected): " << err << "\n";
    }

    return 0;
}
