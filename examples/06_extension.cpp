//
// Example: Extending the parser with custom keywords
//

#include <json5/json5.hpp>
#include <iostream>

int main() {
    // Register custom keywords: undefined → null, PI → 3.14159...
    auto opts = json5::make_keyword_extension({
        {"undefined", json5::value(nullptr)},
        {"PI",        json5::value(3.14159265358979)},
    });

    auto val = json5::parse(R"({
        value: undefined,
        pi: PI,
    })", opts);

    std::cout << json5::stringify(val, json5::write_options::pretty()) << "\n";

    return 0;
}
