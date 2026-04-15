# json5pp

A high-performance **C++20** JSON5 parsing and serialization library.

json5pp provides a DOM-style `json5::value` type (similar to [nlohmann/json](https://github.com/nlohmann/json)), declarative macros for custom-type serialization, full STL container support, and an extensible parser architecture — all with a focus on speed.

## Features

- **Full JSON5 support** — comments, trailing commas, unquoted keys, single-quote strings, hex numbers, `Infinity`/`NaN`, multiline strings, Unicode escapes
- **DOM value type** — intuitive `json5::value` with subscript access, iteration, and mutation
- **Three output styles** — compact, pretty-printed, and JSON5-idiomatic
- **Declarative serialization** — `JSON5_DEFINE(...)` (intrusive) and `JSON5_FIELDS(Type, ...)` (non-intrusive) macros
- **STL coverage** — `vector`, `deque`, `list`, `array`, `set`, `unordered_set`, `map`, `unordered_map`, `optional`, `variant`, `tuple`, `pair`
- **Extensible** — `converter<T>` specialization, ADL free functions, custom keyword/syntax hooks
- **Performance** — hand-written lexer, `std::from_chars` / `std::to_chars`, zero-copy string views

## Requirements

| Requirement | Minimum |
|---|---|
| C++ standard | C++20 |
| Build system | [xmake](https://xmake.io) (or any system that can compile a single `.cpp`) |

Tested with Clang 15+, GCC 12+, and MSVC 19.30+.

## Quick Start

### Build

```bash
xmake build json5pp              # static library
xmake build -a                   # library + all examples + tests
xmake run example_01_parsing     # run a specific example

# Generate API reference from source comments (requires Doxygen)
doxygen Doxyfile                 # output: docs/api/html/
```

### Integrate into your project

1. Copy the `include/json5/` headers and `src/json5.cpp` into your project.
2. Compile `json5.cpp` as a single translation unit.
3. Add the `include/` directory to your header search path.
4. `#include <json5/json5.hpp>` — done.

### Parse JSON5

```cpp
#include <json5/json5.hpp>

auto val = json5::parse(R"({
    // JSON5 document
    name: 'json5pp',
    version: 1,
    tags: ['fast', 'modern',],
})");

std::cout << val["name"].as_string();   // "json5pp"
std::cout << val["version"].as_integer(); // 1
```

### Serialize to string

```cpp
// Compact
json5::stringify(val);

// Pretty-printed JSON
json5::stringify(val, json5::write_options::pretty());

// Compact JSON5 (unquoted keys, single quotes, minified)
json5::stringify(val, json5::write_options::compact_json5());

// Pretty JSON5 (unquoted keys, single quotes, trailing commas)
json5::stringify(val, json5::write_options::pretty_json5());

// Strict JSON parsing
auto v = json5::parse(input, json5::parse_options::strict_json());
```

### Custom types

```cpp
// Intrusive — place inside the class body
struct Person {
    std::string name;
    int age;
    JSON5_DEFINE(name, age)
};

// Non-intrusive — place outside the class
struct Point { double x, y; };
JSON5_FIELDS(Point, x, y)

// Round-trip
Person alice{"Alice", 30};
json5::value j = json5::to_value(alice);
Person copy  = json5::from_value<Person>(j);
```

### STL containers

```cpp
std::vector<int> nums = {1, 2, 3};
json5::value j = json5::to_value(nums);       // [1,2,3]
auto back = json5::from_value<std::vector<int>>(j); // {1,2,3}

std::map<std::string, int> m = {{"a", 1}};
json5::value jm = json5::to_value(m);         // {"a":1}
```

### Extend with custom keywords

```cpp
auto opts = json5::make_keyword_extension({
    {"undefined", json5::value(nullptr)},
    {"PI",        json5::value(3.14159265358979)},
});
auto v = json5::parse("{ val: undefined, pi: PI }", opts);
```

## Project Layout

```
json5pp/
├── include/json5/
│   ├── json5.hpp        ← single include entry point
│   ├── fwd.hpp          ← forward declarations & type aliases
│   ├── error.hpp        ← parse_error, type_error
│   ├── value.hpp        ← json5::value DOM type
│   ├── parser.hpp       ← parse(), try_parse()
│   ├── writer.hpp       ← stringify(), write_options
│   ├── converter.hpp    ← converter<T>, to_value(), from_value()
│   ├── macros.hpp       ← JSON5_DEFINE, JSON5_FIELDS
│   └── extension.hpp    ← make_keyword_extension(), parse_with_transform()
├── src/
│   └── json5.cpp        ← core compiled unit (lexer + parser + writer)
├── examples/
│   ├── 01_parsing.cpp       ← parsing & error handling
│   ├── 02_stringify.cpp     ← serialization styles
│   ├── 03_custom_types.cpp  ← JSON5_DEFINE / JSON5_FIELDS
│   ├── 04_stl_types.cpp     ← STL container round-trips
│   ├── 05_dom.cpp           ← building a document from scratch
│   └── 06_extension.cpp     ← custom keyword extension
├── docs/
│   ├── tutorial.md          ← getting started guide
│   └── architecture.md      ← internal design & decisions
├── tests/               ← unit tests (106 cases)
├── Doxyfile             ← Doxygen config (generates docs/api/)
└── xmake.lua
```
