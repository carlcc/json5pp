# Tutorial: Getting Started with json5pp

This guide walks you through the core features of json5pp with practical examples.

## Prerequisites

- A C++20 compiler (Clang 15+, GCC 12+, MSVC 19.30+)
- [xmake](https://xmake.io) build system (optional — you can also compile manually)

## Installation

json5pp is a single-library with no external dependencies. Just copy two things into your project:

1. The `include/json5/` header directory
2. The `src/json5.cpp` source file

Then compile `json5.cpp` as a single translation unit and add `include/` to your header search path.

```cpp
#include <json5/json5.hpp>   // one include, everything available
```

## 1. Parsing JSON5

```cpp
auto val = json5::parse(R"({
    // JSON5 supports comments
    name: 'json5pp',            // unquoted keys, single-quoted strings
    version: 1,
    features: [
        'comments',
        'trailing commas',      // trailing comma — no error!
    ],
    metadata: {
        hex: 0xFF,              // hex literals
        infinity: Infinity,     // special numbers
    },
})");

val["name"].as_string();           // "json5pp"
val["version"].as_integer();       // 1
val["features"].size();            // 2
val["metadata"]["hex"].as_integer(); // 255
```

The `parse()` function throws `json5::parse_error` on invalid input. Use `try_parse()` if you prefer a non-throwing interface:

```cpp
std::string err;
auto result = json5::try_parse("{bad json", &err);
if (!result) {
    std::cerr << err << "\n";
    // "json5 parse error at line 1, column 10: ..."
}
```

## 2. Serializing to String

Three built-in presets cover the most common output formats:

```cpp
json5::value v = /* ... */;

// Compact (minified) — default
json5::stringify(v);
// {"name":"json5pp","version":1}

// Pretty-printed JSON
json5::stringify(v, json5::write_options::pretty());
// {
//   "name": "json5pp",
//   "version": 1
// }

// JSON5 style (unquoted keys, single quotes, trailing commas)
json5::stringify(v, json5::write_options::pretty_json5());
// {
//   name: 'json5pp',
//   version: 1,
// }
```

You can also construct fully custom options:

```cpp
json5::write_options opts;
opts.indent = "\t";
opts.newline = "\n";
opts.colon_space = " ";
json5::stringify(v, opts);
```

## 3. Working with the DOM

`json5::value` supports intuitive subscript access and mutation:

```cpp
// Build from scratch — null auto-converts to object/array
json5::value doc;
doc["name"] = "json5pp";
doc["version"] = 1;
doc["tags"] = json5::value::array({"parser", "serializer"});

// Query
doc["name"].as_string();   // "json5pp"
doc.contains("version");   // true
doc.size();                // 3

// Mutate
doc["tags"].push_back("fast");
doc.erase("version");

// Non-throwing access
if (auto* p = doc["name"].get_if_string()) {
    std::cout << *p << "\n";
}
```

## 4. Custom Type Serialization

### Intrusive: `JSON5_DEFINE`

Place inside your class if you own it:

```cpp
struct Person {
    std::string name;
    int age;
    std::optional<std::string> email;

    JSON5_DEFINE(name, age, email)
};

Person alice{"Alice", 30, "alice@example.com"};
json5::value j = json5::to_value(alice);
// {"name":"Alice","age":30,"email":"alice@example.com"}

Person copy = json5::from_value<Person>(j);
```

### Non-intrusive: `JSON5_FIELDS`

Use when you can't modify the class:

```cpp
struct Point { double x, y; };
JSON5_FIELDS(Point, x, y)

Point p{3.14, 2.71};
json5::value j = json5::to_value(p);
Point p2 = json5::from_value<Point>(j);
```

### Full manual control: `converter<T>` specialization

For complex custom logic:

```cpp
struct Color { uint8_t r, g, b; };

template<>
struct json5::converter<Color> {
    static json5::value to_json5(const Color& c) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r, c.g, c.b);
        return json5::value(std::string(buf));
    }
    static Color from_json5(const json5::value& j) {
        auto s = j.as_string();
        Color c;
        std::sscanf(s.c_str(), "#%02hhx%02hhx%02hhx", &c.r, &c.g, &c.b);
        return c;
    }
};
```

## 5. Enum Serialization

`JSON5_ENUM` maps enum values to and from JSON5 strings declaratively:

```cpp
enum class Color { Red, Green, Blue, Yellow };
JSON5_ENUM(Color, Red, Green, Blue, Yellow)

// Serialize: enum → string
json5::to_value(Color::Red);     // "Red"
json5::to_value(Color::Blue);    // "Blue"

// Deserialize: string → enum
json5::from_value<Color>(json5::value("Green"));  // Color::Green

// Works in arrays and structs
std::vector<Color> palette = {Color::Red, Color::Green};
json5::to_value(palette);  // ["Red", "Green"]

struct Pixel {
    double x, y;
    Color color;
    JSON5_DEFINE(x, y, color)
};
Pixel px{10.0, 20.0, Color::Blue};
json5::to_value(px);  // {x: 10, y: 20, color: "Blue"}
```

Unknown strings throw `json5::type_error`:

```cpp
try {
    json5::from_value<Color>(json5::value("Purple"));
} catch (const json5::type_error& e) {
    // "json5 type error: unknown Color string: \"Purple\""
}
```

**Performance:** Serialization uses a `switch` statement (O(1) via compiler
jump table). Deserialization uses a sorted array with binary search (O(log n)).

## 6. STL Container Support

All common STL containers work out of the box:

```cpp
// vector → array
std::vector<int> nums = {1, 2, 3};
json5::to_value(nums);   // [1, 2, 3]

// map → object
std::map<std::string, int> scores = {{"Alice", 95}, {"Bob", 87}};
json5::to_value(scores); // {"Alice": 95, "Bob": 87}

// tuple → array
auto t = std::make_tuple(1, "hello", 3.14);
json5::to_value(t);      // [1, "hello", 3.14]

// optional → T or null
std::optional<int> maybe = 42;
json5::to_value(maybe);  // 42
json5::to_value(std::optional<int>{});  // null
```

## 7. Extending the Parser

Register custom keywords that the parser recognizes:

```cpp
auto opts = json5::make_keyword_extension({
    {"undefined", json5::value(nullptr)},
    {"PI",        json5::value(3.14159265358979)},
});

auto v = json5::parse("{ val: undefined, pi: PI }", opts);
// v["val"] is null, v["pi"] is 3.14159...
```

For post-parse transformations:

```cpp
auto result = json5::parse_with_transform(input, [](json5::value& v) {
    // Clamp all integers to [0, 100]
    if (v.is_integer()) {
        auto n = v.as_integer();
        if (n < 0)   v = int64_t(0);
        if (n > 100) v = int64_t(100);
    }
});
```

## 8. Strict JSON Mode

Disable all JSON5 extensions to parse standard JSON only:

```cpp
auto v = json5::parse(input, json5::parse_options::strict_json());
```

Or equivalently, construct manually:

```cpp
json5::parse_options strict;
strict.allow_comments          = false;
strict.allow_trailing_comma    = false;
strict.allow_unquoted_keys     = false;
strict.allow_single_quotes     = false;
strict.allow_hex_numbers       = false;
strict.allow_infinity_nan      = false;
strict.allow_multiline_strings = false;

auto v = json5::parse(input, strict);
```

## Next Steps

- Browse the [API reference](api/html/index.html) generated from source comments
- Read the [Architecture Guide](architecture.md) to understand the internal design
- Check out the `examples/` directory for runnable demos
