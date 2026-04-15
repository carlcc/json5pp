//
// Example: Built-in STL type conversions
//

#include <json5/json5.hpp>
#include <iostream>
#include <vector>
#include <map>
#include <tuple>

int main() {
    // vector → JSON array
    std::vector<int> nums = {1, 2, 3, 4, 5};
    std::cout << "vector: " << json5::stringify(json5::to_value(nums)) << "\n";

    // map → JSON object
    std::map<std::string, int> scores = {{"Alice", 95}, {"Bob", 87}};
    std::cout << "map:\n" << json5::stringify(json5::to_value(scores),
                                              json5::write_options::pretty()) << "\n";

    // tuple → JSON array
    auto t = std::make_tuple(1, "hello", 3.14);
    std::cout << "tuple: " << json5::stringify(json5::to_value(t)) << "\n";

    return 0;
}
