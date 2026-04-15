//
// Example: Building and manipulating a JSON DOM
//

#include <json5/json5.hpp>
#include <iostream>

int main() {
    // Build a document from scratch
    json5::value doc;
    doc["name"] = "json5pp";
    doc["version"] = 1;
    doc["tags"] = json5::value::array({"parser", "serializer"});

    std::cout << json5::stringify(doc, json5::write_options::pretty()) << "\n";

    return 0;
}
