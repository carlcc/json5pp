/// @file fwd.hpp
/// @brief Forward declarations, type aliases, and the value_type enumeration.
///
/// This header is included by every other json5pp header. It provides the
/// minimum set of declarations needed to reference the library's core types
/// without pulling in their full definitions.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <functional>
#include <optional>

namespace json5 {

// Forward declarations
class value;

/// @name Type Aliases
/// @{

/// String type used throughout the library (UTF-8).
using string_t = std::string;

/// Array storage — an ordered sequence of @ref value.
using array_t  = std::vector<value>;

/// Object storage — an ordered sequence of key-value pairs.
///
/// Uses `std::vector` (not `std::map`) to **preserve insertion order**,
/// which is a JSON5 convention. Key lookup is O(n), efficient for typical
/// JSON object sizes.
using object_t = std::vector<std::pair<string_t, value>>;

/// @}

/// @brief Discriminator for the seven JSON5 value kinds.
///
/// The numeric values match the alternative index inside @ref value's
/// internal `std::variant`, so `static_cast<value_type>(v.index())` works.
///
/// | Enumerator | Underlying C++ type |
/// |---|---|
/// | `null`     | `std::nullptr_t`    |
/// | `boolean`  | `bool`              |
/// | `integer`  | `int64_t`           |
/// | `floating` | `double`            |
/// | `string`   | `std::string`       |
/// | `array`    | `std::vector<value>` |
/// | `object`   | `std::vector<std::pair<std::string, value>>` |
enum class value_type : uint8_t {
    null,       ///< JSON `null`.
    boolean,    ///< `true` / `false`.
    integer,    ///< 64-bit signed integer (`int64_t`).
    floating,   ///< Double-precision IEEE 754 floating-point.
    string,     ///< UTF-8 encoded string.
    array,      ///< Ordered list of values.
    object      ///< Ordered list of key-value pairs.
};

/// @brief Return a human-readable name for a @ref value_type enumerator.
///
/// Useful for diagnostic messages and error reporting.
///
/// @param t  The value type to name.
/// @return A `string_view` such as `"null"`, `"integer"`, `"object"`, etc.
constexpr std::string_view type_name(value_type t) noexcept {
    switch (t) {
        case value_type::null:     return "null";
        case value_type::boolean:  return "boolean";
        case value_type::integer:  return "integer";
        case value_type::floating: return "floating";
        case value_type::string:   return "string";
        case value_type::array:    return "array";
        case value_type::object:   return "object";
    }
    return "unknown";
}

// Forward declare parse/write options
struct parse_options;
struct write_options;

/// @brief Primary converter template — the extensibility point for type
/// conversion between C++ types and @ref value.
///
/// See converter.hpp for the full definition, built-in specializations,
/// and instructions on how to add support for your own types.
///
/// @tparam T     The C++ type to convert.
/// @tparam SFINAE  Reserved for SFINAE-based partial specializations.
template<typename T, typename = void>
struct converter;

} // namespace json5
