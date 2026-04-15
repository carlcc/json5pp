# Architecture & Design

This document describes the internal architecture of json5pp and the key
design decisions behind it.

## Overview

json5pp is structured as a **header-mostly** library: template-heavy code
(value types, converters, macros) lives in headers, while the performance-
critical, non-templated components (lexer, parser, writer) are compiled in
a single translation unit (`src/json5.cpp`).

```
                           User Code
                               │
                    ┌──────────▼──────────┐
                    │   json5/json5.hpp   │  (umbrella include)
                    └──────────┬──────────┘
          ┌────────┬───────┬───┴───┬────────┬─────────┬──────────┐
          ▼        ▼       ▼       ▼        ▼         ▼          ▼
       fwd.hpp  error.hpp value.hpp parser.hpp writer.hpp converter.hpp macros.hpp
          │                  │       │         │
          │            (header-only) │         │
          │                  │       ▼         ▼
          │                  │    ┌──────────────────┐
          │                  └───▶│   src/json5.cpp  │  (compiled)
          │                       │  Lexer + Parser  │
          │                       │  + Writer impl   │
          └───────────────────────┴──────────────────┘
```

## Module Responsibilities

| Module | File(s) | Responsibility |
|---|---|---|
| **Forward declarations** | `fwd.hpp` | Type aliases, `value_type` enum, `converter` forward decl |
| **Error handling** | `error.hpp` | `parse_error` (with line/column), `type_error` |
| **DOM value** | `value.hpp` | `json5::value` — the central variant-based DOM type |
| **Lexer** | `json5.cpp` (internal) | Hand-written lexer; tokenizes JSON5 input |
| **Parser** | `parser.hpp` + `json5.cpp` | Recursive-descent parser; builds DOM tree |
| **Writer** | `writer.hpp` + `json5.cpp` | Serializer with pluggable formatting options |
| **Type converter** | `converter.hpp` | `converter<T>` template + all built-in specializations |
| **Macros** | `macros.hpp` | `JSON5_DEFINE` / `JSON5_FIELDS` / `JSON5_ENUM` code generation |
| **Extensions** | `extension.hpp` | `make_keyword_extension`, `parse_with_transform` |

## Key Design Decisions

### 1. Variant-based DOM Value

`json5::value` stores its data in a `std::variant<nullptr_t, bool, int64_t,
double, string_t, array_t, object_t>`. The variant index directly
corresponds to the `value_type` enum, enabling `O(1)` type queries.

**Trade-off:** `std::variant` has a fixed size equal to the largest
alternative (typically `object_t`), so every value node carries that
overhead. In practice this is offset by the simplicity and
cache-friendliness compared to a union+tag or polymorphic approach.

### 2. Insertion-ordered Objects

`object_t` is defined as `std::vector<std::pair<string_t, value>>` rather
than `std::map`. This preserves insertion order — a strong expectation in
JSON and JSON5 ecosystems — and provides better cache locality for
small objects (the common case).

Key lookup is `O(n)`, which is optimal for typical JSON object sizes
(< 20 keys). For very large flat objects, users can convert to
`std::unordered_map` via `from_value()`.

### 3. Hand-written Lexer

The lexer in `json5.cpp` is a hand-crafted state machine that processes
input one character at a time. This approach was chosen over regex or
parser-generator tools for:

- **Performance** — no regex overhead, minimal allocations.
- **Precise error reporting** — the lexer tracks line/column as it scans.
- **Full JSON5 support** — JSON5's lexical grammar (hex numbers, multi-line
  strings, single-quoted strings, `Infinity`/`NaN`) doesn't map cleanly to
  standard tokenizer frameworks.

### 4. Number Handling with `std::from_chars` / `std::to_chars`

Number parsing and formatting use `<charconv>`, which avoids locale
sensitivity and is significantly faster than `strtod`/`snprintf` on modern
standard library implementations.

Integer and floating-point values are stored separately (`int64_t` vs
`double`) to preserve integer precision up to 2⁶³. The `as_double()`
accessor silently widens integers to doubles when needed.

### 5. Three-tier Type Conversion

The `converter<T>` architecture supports three ways to add JSON5
support for custom types, with clear priority ordering:

1. **Explicit specialization** — `json5::converter<MyType>` (highest priority)
2. **Member introspection** — `_json5_to_()` / `_json5_from_()` generated
   by `JSON5_DEFINE` macro
3. **ADL free functions** — `to_json5(value&, const T&)` /
   `from_json5(const value&, T&)`

The primary template uses `if constexpr` and SFINAE detection traits to
dispatch at compile time, with a `static_assert` fallback that provides
a clear error message when no conversion path exists.

The `detail::adl_invoker` struct is used to call ADL functions from within
the converter, avoiding issues with unqualified lookup inside the `json5`
namespace.

### 6. Macro + Variadic Template Code Generation

`JSON5_DEFINE`, `JSON5_FIELDS`, and `JSON5_ENUM` use a **macro + variadic
template** hybrid approach:

- **Macros** handle only what templates cannot: the `#` stringification
  operator (converting field/enumerator names to string literals) and the
  `&Type::field` member-pointer syntax.
- **Variadic templates** (`detail::to_fields`, `detail::from_fields`,
  `detail::make_enum_table`) handle the actual recursive expansion over
  all fields/values. This provides full type safety, clear compiler error
  messages, and no limit on the number of arguments (bounded only by
  compiler template instantiation depth, typically 900+).

The `__VA_OPT__`-based `FOR_EACH` macro is used solely to generate the
comma-separated `("name", &Type::name, "age", &Type::age, ...)` argument
list that feeds into the variadic templates.

**Previous approach:** The original implementation used manually numbered
`FOREACH_1` through `FOREACH_16` macros, limiting fields to 16. The new
design eliminates this constraint entirely.

### 7. Enum Serialization Strategy

`JSON5_ENUM` generates a `converter<EnumType>` specialization with two
high-performance mapping strategies:

- **`to_json5` (enum → string):** Uses a `switch` statement over all
  registered enumerators. Modern compilers optimize this to a jump table,
  yielding O(1) lookup.
- **`from_json5` (string → enum):** Uses a `constexpr` sorted
  `std::array<enum_entry, N>` with binary search (`O(log n)`). The array
  is sorted at compile time using a constexpr insertion sort, so runtime
  deserialization pays zero sorting cost.

Unknown enum strings during deserialization throw `type_error`.

### 8. Extensible Parser Hooks

The `parse_options::custom_value_parser` callback enables users to extend
the JSON5 grammar without modifying the library. When the parser encounters
an unknown identifier, it calls this hook before reporting an error.

`make_keyword_extension()` builds on this hook to provide a simple
keyword-to-value mapping. `parse_with_transform()` adds a post-parse
DOM transformation pass.

## Error Handling Strategy

- **Parsing errors** produce `parse_error` with line/column information.
  `try_parse()` provides a non-throwing alternative.
- **Type errors** (accessing the wrong alternative) throw `type_error`
  with a descriptive message including expected vs. actual types.
- **Non-throwing access** is available via `get_if_*()` pointer accessors
  that return `nullptr` on type mismatch.

## Performance Considerations

- **Zero-copy parsing:** The lexer uses `string_view` for token scanning;
  strings are only allocated when they contain escape sequences.
- **Minimal allocations:** The parser builds the DOM tree in a single pass
  with no intermediate representation.
- **`std::from_chars`:** Number parsing/formatting avoids locale overhead.
- **Reserve + push_back:** Container serialization reserves capacity
  before filling.
