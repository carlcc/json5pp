/// @file converter.hpp
/// @brief Type conversion layer: @ref json5::converter\<T\>, @ref json5::to_value(),
/// @ref json5::from_value(), and all built-in specializations for fundamental
/// and STL types.
///
/// ## Conversion Resolution Order
///
/// When you call `to_value(x)` or `from_value<T>(j)`, the library resolves
/// the conversion in the following priority order:
///
/// 1. **Explicit `converter<T>` specialization** — always wins.
/// 2. **`JSON5_DEFINE` member methods** — detected via `has_member_json5`.
/// 3. **ADL free functions** — `void to_json5(value&, const T&)` /
///    `void from_json5(const value&, T&)`.
/// 4. **Compile-time error** — a `static_assert` fires if none of the
///    above are found.
///
/// ## Adding support for your own types
///
/// Choose **one** of three approaches:
///
/// ### 1. Intrusive macro (simplest, requires class modification)
///
/// @code
/// struct Config {
///     std::string host;
///     int port;
///     JSON5_DEFINE(host, port)
/// };
/// @endcode
///
/// ### 2. Non-intrusive macro (no class modification, public fields only)
///
/// @code
/// struct Point { double x, y; };
/// JSON5_FIELDS(Point, x, y)
/// @endcode
///
/// ### 3. Full converter specialization (maximum control)
///
/// @code
/// struct Color { uint8_t r, g, b; };
///
/// template<>
/// struct json5::converter<Color> {
///     static json5::value to_json5(const Color& c) {
///         char buf[8];
///         std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r, c.g, c.b);
///         return json5::value(std::string(buf));
///     }
///     static Color from_json5(const json5::value& j) {
///         auto s = j.as_string();
///         Color c;
///         std::sscanf(s.c_str(), "#%02hhx%02hhx%02hhx", &c.r, &c.g, &c.b);
///         return c;
///     }
/// };
/// @endcode
///
/// ## Built-in Converter Type Map
///
/// | C++ type | JSON5 representation |
/// |---|---|
/// | `bool` | `true` / `false` |
/// | Integer types (`int`, `int64_t`, …) | integer |
/// | Floating types (`float`, `double`) | floating-point number |
/// | `std::string` | string |
/// | `std::string_view` | string (to_json5 only) |
/// | `const char*` | string (to_json5 only) |
/// | `json5::value` | passthrough (identity) |
/// | `std::optional<T>` | `T` or `null` |
/// | `std::vector<T>` | array |
/// | `std::deque<T>` | array |
/// | `std::list<T>` | array |
/// | `std::array<T, N>` | array (size must match on deserialization) |
/// | `std::set<T>` | array |
/// | `std::unordered_set<T>` | array |
/// | `std::map<std::string, V>` | object |
/// | `std::unordered_map<std::string, V>` | object |
/// | `std::pair<A, B>` | 2-element array |
/// | `std::tuple<Ts...>` | N-element array |
/// | `std::variant<Ts...>` | `{"index": i, "value": ...}` |
/// | Enum types (via `JSON5_ENUM`) | string (enumerator name) |

#pragma once

#include "fwd.hpp"
#include "value.hpp"

#include <type_traits>
#include <concepts>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <optional>
#include <variant>
#include <tuple>
#include <utility>
#include <deque>
#include <list>

namespace json5 {

// ═══════════════════════════════════════════════════════════════════
//  Detection helpers
// ═══════════════════════════════════════════════════════════════════

/// @cond INTERNAL
namespace detail {

// ADL detection: to_json5(value&, const T&)
template<typename T, typename = void>
struct has_adl_to_json5 : std::false_type {};

template<typename T>
struct has_adl_to_json5<T, std::void_t<decltype(to_json5(
    std::declval<value&>(), std::declval<const T&>()))>> : std::true_type {};

// ADL detection: from_json5(const value&, T&)
template<typename T, typename = void>
struct has_adl_from_json5 : std::false_type {};

template<typename T>
struct has_adl_from_json5<T, std::void_t<decltype(from_json5(
    std::declval<const value&>(), std::declval<T&>()))>> : std::true_type {};

// Member method detection: T::_json5_to_(value&) const
template<typename T, typename = void>
struct has_member_json5 : std::false_type {};

template<typename T>
struct has_member_json5<T, std::void_t<decltype(
    std::declval<const T&>()._json5_to_(std::declval<value&>()))>> : std::true_type {};

// Range concept
template<typename T>
concept range_like = requires(const T& t) {
    std::begin(t);
    std::end(t);
} && !std::is_same_v<std::decay_t<T>, std::string>
  && !std::is_same_v<std::decay_t<T>, std::string_view>;

// Map-like concept (has key_type and mapped_type)
template<typename T>
concept map_like = requires {
    typename T::key_type;
    typename T::mapped_type;
} && range_like<T>;

// Tuple-like concept
template<typename T>
concept tuple_like = requires {
    { std::tuple_size<T>::value } -> std::convertible_to<std::size_t>;
} && !std::is_same_v<std::decay_t<T>, std::array<typename T::value_type, std::tuple_size<T>::value>>;

} // namespace detail

namespace detail {

// ADL invokers — must be free functions in a namespace that doesn't
// contain the target to_json5/from_json5 overloads, so ADL works.
struct adl_invoker {
    template<typename T>
    static value call_to_json5(const T& v) {
        value j;
        to_json5(j, v);  // unqualified → ADL
        return j;
    }

    template<typename T>
    static T call_from_json5(const value& j) {
        T v{};
        from_json5(j, v);  // unqualified → ADL
        return v;
    }
};

} // namespace detail
/// @endcond

// ═══════════════════════════════════════════════════════════════════
//  Primary converter template — ADL fallback
// ═══════════════════════════════════════════════════════════════════

/// @brief Primary converter template (fallback).
///
/// Detects — in order — member methods generated by `JSON5_DEFINE`, then
/// ADL free functions. If neither is found, a `static_assert` fires at
/// compile time.
///
/// Specialize this template directly if you need full control over the
/// conversion logic for a custom type.
///
/// @tparam T  The C++ type to convert.
template<typename T, typename>
struct converter {
    /// @brief Serialize @p v to a @ref value.
    static value to_json5(const T& v) {
        if constexpr (detail::has_member_json5<T>::value) {
            value j;
            v._json5_to_(j);
            return j;
        } else if constexpr (detail::has_adl_to_json5<T>::value) {
            return detail::adl_invoker::call_to_json5(v);
        } else {
            static_assert(detail::has_member_json5<T>::value || detail::has_adl_to_json5<T>::value,
                "No converter, member _json5_to_, or ADL to_json5 found for this type. "
                "Use JSON5_DEFINE(...) inside the class, JSON5_FIELDS(Type, ...) outside, "
                "or provide void to_json5(json5::value&, const T&).");
        }
    }

    /// @brief Deserialize a @ref value into a @p T.
    static T from_json5(const value& j) {
        if constexpr (detail::has_member_json5<T>::value) {
            T v{};
            v._json5_from_(j);
            return v;
        } else if constexpr (detail::has_adl_from_json5<T>::value) {
            return detail::adl_invoker::call_from_json5<T>(j);
        } else {
            static_assert(detail::has_member_json5<T>::value || detail::has_adl_from_json5<T>::value,
                "No converter, member _json5_from_, or ADL from_json5 found for this type. "
                "Use JSON5_DEFINE(...) inside the class, JSON5_FIELDS(Type, ...) outside, "
                "or provide void from_json5(const json5::value&, T&).");
        }
    }
};

// ═══════════════════════════════════════════════════════════════════
//  Built-in type specializations
// ═══════════════════════════════════════════════════════════════════

/// @cond INTERNAL

// ── bool ──────────────────────────────────────────────────────────
template<>
struct converter<bool> {
    static value to_json5(bool v) { return value(v); }
    static bool from_json5(const value& j) { return j.as_bool(); }
};

// ── Integer types ─────────────────────────────────────────────────
template<typename T>
struct converter<T, std::enable_if_t<
    std::is_integral_v<T> && !std::is_same_v<T, bool>>> {
    static value to_json5(T v) { return value(static_cast<int64_t>(v)); }
    static T from_json5(const value& j) { return static_cast<T>(j.as_integer()); }
};

// ── Floating point types ──────────────────────────────────────────
template<typename T>
struct converter<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    static value to_json5(T v) { return value(static_cast<double>(v)); }
    static T from_json5(const value& j) { return static_cast<T>(j.as_double()); }
};

// ── std::string ───────────────────────────────────────────────────
template<>
struct converter<std::string> {
    static value to_json5(const std::string& v) { return value(v); }
    static std::string from_json5(const value& j) { return j.as_string(); }
};

// ── std::string_view (to_json5 only) ─────────────────────────────
template<>
struct converter<std::string_view> {
    static value to_json5(std::string_view v) { return value(v); }
    // Note: from_json5 cannot return string_view (dangling). Use string.
};

// ── const char* (to_json5 only) ──────────────────────────────────
template<>
struct converter<const char*> {
    static value to_json5(const char* v) { return value(v); }
};

// ── json5::value itself (passthrough) ─────────────────────────────
template<>
struct converter<value> {
    static value to_json5(const value& v) { return v; }
    static value from_json5(const value& j) { return j; }
};

// ═══════════════════════════════════════════════════════════════════
//  STL container specializations
// ═══════════════════════════════════════════════════════════════════

// ── std::optional<T> ──────────────────────────────────────────────
template<typename T>
struct converter<std::optional<T>> {
    static value to_json5(const std::optional<T>& v) {
        if (v.has_value()) return converter<T>::to_json5(*v);
        return value(nullptr);
    }
    static std::optional<T> from_json5(const value& j) {
        if (j.is_null()) return std::nullopt;
        return converter<T>::from_json5(j);
    }
};

// ── std::vector<T> ────────────────────────────────────────────────
template<typename T, typename Alloc>
struct converter<std::vector<T, Alloc>> {
    static value to_json5(const std::vector<T, Alloc>& v) {
        array_t arr;
        arr.reserve(v.size());
        for (const auto& elem : v)
            arr.push_back(converter<T>::to_json5(elem));
        return value(std::move(arr));
    }
    static std::vector<T, Alloc> from_json5(const value& j) {
        const auto& arr = j.as_array();
        std::vector<T, Alloc> result;
        result.reserve(arr.size());
        for (const auto& elem : arr)
            result.push_back(converter<T>::from_json5(elem));
        return result;
    }
};

// ── std::deque<T> ─────────────────────────────────────────────────
template<typename T, typename Alloc>
struct converter<std::deque<T, Alloc>> {
    static value to_json5(const std::deque<T, Alloc>& v) {
        array_t arr;
        for (const auto& elem : v)
            arr.push_back(converter<T>::to_json5(elem));
        return value(std::move(arr));
    }
    static std::deque<T, Alloc> from_json5(const value& j) {
        const auto& arr = j.as_array();
        std::deque<T, Alloc> result;
        for (const auto& elem : arr)
            result.push_back(converter<T>::from_json5(elem));
        return result;
    }
};

// ── std::list<T> ──────────────────────────────────────────────────
template<typename T, typename Alloc>
struct converter<std::list<T, Alloc>> {
    static value to_json5(const std::list<T, Alloc>& v) {
        array_t arr;
        for (const auto& elem : v)
            arr.push_back(converter<T>::to_json5(elem));
        return value(std::move(arr));
    }
    static std::list<T, Alloc> from_json5(const value& j) {
        const auto& arr = j.as_array();
        std::list<T, Alloc> result;
        for (const auto& elem : arr)
            result.push_back(converter<T>::from_json5(elem));
        return result;
    }
};

// ── std::array<T, N> ─────────────────────────────────────────────
template<typename T, std::size_t N>
struct converter<std::array<T, N>> {
    static value to_json5(const std::array<T, N>& v) {
        array_t arr;
        arr.reserve(N);
        for (const auto& elem : v)
            arr.push_back(converter<T>::to_json5(elem));
        return value(std::move(arr));
    }
    static std::array<T, N> from_json5(const value& j) {
        const auto& arr = j.as_array();
        if (arr.size() != N) {
            throw type_error("expected array of size " + std::to_string(N) +
                           ", got " + std::to_string(arr.size()));
        }
        std::array<T, N> result;
        for (std::size_t i = 0; i < N; ++i)
            result[i] = converter<T>::from_json5(arr[i]);
        return result;
    }
};

// ── std::set<T> ───────────────────────────────────────────────────
template<typename T, typename Cmp, typename Alloc>
struct converter<std::set<T, Cmp, Alloc>> {
    static value to_json5(const std::set<T, Cmp, Alloc>& v) {
        array_t arr;
        for (const auto& elem : v)
            arr.push_back(converter<T>::to_json5(elem));
        return value(std::move(arr));
    }
    static std::set<T, Cmp, Alloc> from_json5(const value& j) {
        const auto& arr = j.as_array();
        std::set<T, Cmp, Alloc> result;
        for (const auto& elem : arr)
            result.insert(converter<T>::from_json5(elem));
        return result;
    }
};

// ── std::unordered_set<T> ─────────────────────────────────────────
template<typename T, typename Hash, typename Eq, typename Alloc>
struct converter<std::unordered_set<T, Hash, Eq, Alloc>> {
    static value to_json5(const std::unordered_set<T, Hash, Eq, Alloc>& v) {
        array_t arr;
        for (const auto& elem : v)
            arr.push_back(converter<T>::to_json5(elem));
        return value(std::move(arr));
    }
    static std::unordered_set<T, Hash, Eq, Alloc> from_json5(const value& j) {
        const auto& arr = j.as_array();
        std::unordered_set<T, Hash, Eq, Alloc> result;
        for (const auto& elem : arr)
            result.insert(converter<T>::from_json5(elem));
        return result;
    }
};

// ── std::map<string, V> ──────────────────────────────────────────
template<typename V, typename Cmp, typename Alloc>
struct converter<std::map<std::string, V, Cmp, Alloc>> {
    static value to_json5(const std::map<std::string, V, Cmp, Alloc>& v) {
        object_t obj;
        for (const auto& [key, val] : v)
            obj.emplace_back(key, converter<V>::to_json5(val));
        return value(std::move(obj));
    }
    static std::map<std::string, V, Cmp, Alloc> from_json5(const value& j) {
        const auto& obj = j.as_object();
        std::map<std::string, V, Cmp, Alloc> result;
        for (const auto& [key, val] : obj)
            result.emplace(key, converter<V>::from_json5(val));
        return result;
    }
};

// ── std::unordered_map<string, V> ─────────────────────────────────
template<typename V, typename Hash, typename Eq, typename Alloc>
struct converter<std::unordered_map<std::string, V, Hash, Eq, Alloc>> {
    static value to_json5(const std::unordered_map<std::string, V, Hash, Eq, Alloc>& v) {
        object_t obj;
        for (const auto& [key, val] : v)
            obj.emplace_back(key, converter<V>::to_json5(val));
        return value(std::move(obj));
    }
    static std::unordered_map<std::string, V, Hash, Eq, Alloc> from_json5(const value& j) {
        const auto& obj = j.as_object();
        std::unordered_map<std::string, V, Hash, Eq, Alloc> result;
        for (const auto& [key, val] : obj)
            result.emplace(key, converter<V>::from_json5(val));
        return result;
    }
};

// ── std::pair<A, B> ──────────────────────────────────────────────
template<typename A, typename B>
struct converter<std::pair<A, B>> {
    static value to_json5(const std::pair<A, B>& v) {
        return value(array_t{
            converter<A>::to_json5(v.first),
            converter<B>::to_json5(v.second)
        });
    }
    static std::pair<A, B> from_json5(const value& j) {
        const auto& arr = j.as_array();
        if (arr.size() != 2)
            throw type_error("expected array of size 2 for pair, got " + std::to_string(arr.size()));
        return {
            converter<A>::from_json5(arr[0]),
            converter<B>::from_json5(arr[1])
        };
    }
};

// ── std::tuple<Ts...> ────────────────────────────────────────────
template<typename... Ts>
struct converter<std::tuple<Ts...>> {
    static value to_json5(const std::tuple<Ts...>& v) {
        array_t arr;
        arr.reserve(sizeof...(Ts));
        std::apply([&arr](const auto&... args) {
            (arr.push_back(converter<std::decay_t<decltype(args)>>::to_json5(args)), ...);
        }, v);
        return value(std::move(arr));
    }

    static std::tuple<Ts...> from_json5(const value& j) {
        const auto& arr = j.as_array();
        if (arr.size() != sizeof...(Ts))
            throw type_error("expected array of size " + std::to_string(sizeof...(Ts)) +
                           " for tuple, got " + std::to_string(arr.size()));
        return from_array(arr, std::index_sequence_for<Ts...>{});
    }

private:
    template<std::size_t... Is>
    static std::tuple<Ts...> from_array(const array_t& arr, std::index_sequence<Is...>) {
        return std::tuple<Ts...>{
            converter<Ts>::from_json5(arr[Is])...
        };
    }
};

// ── variant index helper (must precede variant converter) ─────────
namespace detail {
    template<typename Target, typename First, typename... Rest>
    constexpr std::size_t variant_index_of(std::size_t current = 0) {
        if constexpr (std::is_same_v<Target, First>)
            return current;
        else if constexpr (sizeof...(Rest) > 0)
            return variant_index_of<Target, Rest...>(current + 1);
        else
            return static_cast<std::size_t>(-1); // Should not happen
    }
} // namespace detail

// ── std::variant<Ts...> ──────────────────────────────────────────
template<typename... Ts>
struct converter<std::variant<Ts...>> {
    static value to_json5(const std::variant<Ts...>& v) {
        // Store as {"index": i, "value": ...}
        return std::visit([](const auto& inner) -> value {
            using Inner = std::decay_t<decltype(inner)>;
            object_t obj;
            obj.emplace_back("index", value(static_cast<int64_t>(
                detail::variant_index_of<Inner, Ts...>())));
            obj.emplace_back("value", converter<Inner>::to_json5(inner));
            return value(std::move(obj));
        }, v);
    }

    static std::variant<Ts...> from_json5(const value& j) {
        const auto& obj = j.as_object();
        int64_t idx = -1;
        const value* val_ptr = nullptr;
        for (const auto& [k, v] : obj) {
            if (k == "index") idx = v.as_integer();
            else if (k == "value") val_ptr = &v;
        }
        if (idx < 0 || !val_ptr)
            throw type_error("invalid variant encoding");
        return from_index(static_cast<std::size_t>(idx), *val_ptr,
                         std::index_sequence_for<Ts...>{});
    }

private:
    template<std::size_t... Is>
    static std::variant<Ts...> from_index(std::size_t idx, const value& j,
                                           std::index_sequence<Is...>) {
        std::variant<Ts...> result;
        bool found = false;
        (void)((Is == idx ? (result = converter<Ts>::from_json5(j), found = true, true) : false) || ...);
        if (!found) throw type_error("variant index out of range");
        return result;
    }
};

/// @endcond

// ═══════════════════════════════════════════════════════════════════
//  Convenience functions
// ═══════════════════════════════════════════════════════════════════

/// @brief Convert any supported C++ type to a @ref value.
///
/// Delegates to `converter<std::decay_t<T>>::to_json5()`.
///
/// @code
/// json5::value j = json5::to_value(42);          // integer
/// json5::value j = json5::to_value("hello");      // string
/// json5::value j = json5::to_value(my_struct);    // object (if macro defined)
/// @endcode
///
/// @tparam T  The source C++ type.
/// @param v   The value to convert.
/// @return The resulting @ref value.
template<typename T>
[[nodiscard]] value to_value(const T& v) {
    return converter<std::decay_t<T>>::to_json5(v);
}

/// @brief Convert a @ref value back to a C++ type.
///
/// Delegates to `converter<std::decay_t<T>>::from_json5()`.
///
/// @code
/// int n = json5::from_value<int>(j);
/// auto vec = json5::from_value<std::vector<int>>(j);
/// auto person = json5::from_value<Person>(j);
/// @endcode
///
/// @tparam T  The target C++ type.
/// @param j   The JSON5 value to convert from.
/// @return The deserialized C++ value.
template<typename T>
[[nodiscard]] T from_value(const value& j) {
    return converter<std::decay_t<T>>::from_json5(j);
}

} // namespace json5
