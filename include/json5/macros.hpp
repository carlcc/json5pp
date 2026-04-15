/// @file macros.hpp
/// @brief Declarative macros for automatic struct and enum serialization.
///
/// ## JSON5_DEFINE — Intrusive Macro
///
/// Place **inside** a class/struct body. Generates `_json5_to_()` and
/// `_json5_from_()` member methods that the primary @ref converter\<T\>
/// template detects automatically.
///
/// @code
/// struct Config {
///     std::string host = "localhost";
///     int port = 8080;
///     std::optional<std::string> token;
///
///     JSON5_DEFINE(host, port, token)
/// };
/// @endcode
///
/// ### Requirements
/// - The class must be **default-constructible** for `from_value()` to work.
/// - Each field type must itself have a working @ref converter.
/// - **No limit** on the number of fields.
///
/// ## JSON5_FIELDS — Non-intrusive Macro
///
/// Place **outside** the class at namespace scope. Generates a full
/// `converter<Type>` specialization.
///
/// @code
/// struct Point3D { double x, y, z; };
/// JSON5_FIELDS(Point3D, x, y, z)
/// @endcode
///
/// ### Requirements
/// - Fields must be **publicly accessible**.
/// - The class must be **default-constructible** for deserialization.
/// - Place this macro **after** the class definition and **after**
///   including `<json5/json5.hpp>`.
/// - **No limit** on the number of fields.
///
/// ## JSON5_ENUM — Enum Serialization Macro
///
/// Place **outside** the enum at namespace scope. Generates a
/// `converter<EnumType>` specialization that maps enum values to/from
/// strings.
///
/// @code
/// enum class Color { Red, Green, Blue };
/// JSON5_ENUM(Color, Red, Green, Blue)
/// @endcode
///
/// - **to_json5** uses a `switch` statement → O(1) via compiler jump table.
/// - **from_json5** uses a sorted array + binary search → O(log n).
/// - Throws `type_error` on unknown string during deserialization.
///
/// ## When to use which
///
/// | Scenario | Macro |
/// |---|---|
/// | You own the class and can modify it | `JSON5_DEFINE` |
/// | You cannot modify the class (third-party, legacy) | `JSON5_FIELDS` |
/// | Enum type ↔ string mapping | `JSON5_ENUM` |
/// | You need custom logic (not just field mapping) | `converter<T>` specialization |

#pragma once

#include "fwd.hpp"
#include "value.hpp"
#include "converter.hpp"

#include <algorithm>
#include <array>
#include <string_view>

// ═══════════════════════════════════════════════════════════════════
//  Internal: variadic template helpers for struct field serialization
// ═══════════════════════════════════════════════════════════════════

/// @cond INTERNAL
namespace json5::detail {

// ── to_fields: serialize struct fields to a json5::value (object) ──

/// Base case: no more fields.
inline void to_fields([[maybe_unused]] value& j,
                      [[maybe_unused]] const auto& self) {}

/// Recursive case: process one (name, member-pointer) pair, then recurse.
template <typename T, typename MemPtr, typename... Rest>
void to_fields(value& j, const T& self,
               std::string_view name, MemPtr ptr, Rest&&... rest) {
    j.insert(std::string(name), ::json5::to_value(self.*ptr));
    if constexpr (sizeof...(rest) > 0) {
        to_fields(j, self, std::forward<Rest>(rest)...);
    }
}

// ── from_fields: deserialize json5::value (object) into struct fields ──

/// Base case: no more fields.
inline void from_fields([[maybe_unused]] const value& j,
                        [[maybe_unused]] auto& self) {}

/// Recursive case: process one (name, member-pointer) pair, then recurse.
template <typename T, typename MemPtr, typename... Rest>
void from_fields(const value& j, T& self,
                 std::string_view name, MemPtr ptr, Rest&&... rest) {
    if (j.contains(std::string(name))) {
        self.*ptr = ::json5::from_value<
            std::decay_t<decltype(self.*ptr)>>(j[std::string(name)]);
    }
    if constexpr (sizeof...(rest) > 0) {
        from_fields(j, self, std::forward<Rest>(rest)...);
    }
}

// ── enum_entry: a (string_view, enum_value) pair for binary search ──

template <typename E>
struct enum_entry {
    std::string_view name;
    E value;

    constexpr bool operator<(const enum_entry& rhs) const noexcept {
        return name < rhs.name;
    }
};

/// Build a sorted std::array of enum_entry from variadic (name, value) pairs.
template <typename E, typename... Args>
constexpr auto make_enum_table(Args... args) {
    constexpr std::size_t N = sizeof...(Args) / 2;
    // Collect pairs into array
    std::array<enum_entry<E>, N> table{};
    fill_enum_table<E>(table.data(), args...);
    // Sort by name for binary search
    // Use a simple insertion sort (constexpr-friendly)
    for (std::size_t i = 1; i < N; ++i) {
        auto key = table[i];
        std::size_t j = i;
        while (j > 0 && table[j - 1].name > key.name) {
            table[j] = table[j - 1];
            --j;
        }
        table[j] = key;
    }
    return table;
}

/// Helper: fill array from (name, value, name, value, ...) arg list.
template <typename E>
constexpr void fill_enum_table([[maybe_unused]] enum_entry<E>* out) {}

template <typename E, typename... Rest>
constexpr void fill_enum_table(enum_entry<E>* out,
                               std::string_view name, E val, Rest... rest) {
    out->name = name;
    out->value = val;
    if constexpr (sizeof...(rest) > 0) {
        fill_enum_table<E>(out + 1, rest...);
    }
}

/// Binary search in a sorted enum table. Returns pointer or nullptr.
template <typename E, std::size_t N>
constexpr const enum_entry<E>* find_enum_by_name(
        const std::array<enum_entry<E>, N>& table,
        std::string_view name) {
    std::size_t lo = 0, hi = N;
    while (lo < hi) {
        std::size_t mid = lo + (hi - lo) / 2;
        if (table[mid].name < name) lo = mid + 1;
        else if (table[mid].name > name) hi = mid;
        else return &table[mid];
    }
    return nullptr;
}

} // namespace json5::detail
/// @endcond

// ═══════════════════════════════════════════════════════════════════
//  Helper macro: stringify each field and pair with its member pointer
// ═══════════════════════════════════════════════════════════════════

/// @cond INTERNAL

// Expands field → "field", &Type::field  (for JSON5_FIELDS)
#define JSON5_DETAIL_FIELD_PAIR(Type, field) #field, &Type::field

// Expands field → "field", &_json5_self_type_::field  (for JSON5_DEFINE — deduced type)
#define JSON5_DETAIL_MEMBER_PAIR(field) #field, &_json5_self_type_::field

// Expands enum value → #value, Type::value  (for JSON5_ENUM)
#define JSON5_DETAIL_ENUM_PAIR(Type, val) std::string_view(#val), Type::val

// ── __VA_OPT__ recursive FOREACH (unlimited arguments) ──────────

// Apply a 2-arg macro (macro(ctx, elem)) to each element in __VA_ARGS__.
// Uses the standard __VA_OPT__ + deferred evaluation trick for recursion.
#define JSON5_DETAIL_PARENS ()

// Expand 6*6*6*6 = 1296 times to handle up to 1296 args
#define JSON5_DETAIL_EXPAND4(...) JSON5_DETAIL_EXPAND3(JSON5_DETAIL_EXPAND3(JSON5_DETAIL_EXPAND3(JSON5_DETAIL_EXPAND3(JSON5_DETAIL_EXPAND3(JSON5_DETAIL_EXPAND3(__VA_ARGS__))))))
#define JSON5_DETAIL_EXPAND3(...) JSON5_DETAIL_EXPAND2(JSON5_DETAIL_EXPAND2(JSON5_DETAIL_EXPAND2(JSON5_DETAIL_EXPAND2(JSON5_DETAIL_EXPAND2(JSON5_DETAIL_EXPAND2(__VA_ARGS__))))))
#define JSON5_DETAIL_EXPAND2(...) JSON5_DETAIL_EXPAND1(JSON5_DETAIL_EXPAND1(JSON5_DETAIL_EXPAND1(JSON5_DETAIL_EXPAND1(JSON5_DETAIL_EXPAND1(JSON5_DETAIL_EXPAND1(__VA_ARGS__))))))
#define JSON5_DETAIL_EXPAND1(...) JSON5_DETAIL_EXPAND0(JSON5_DETAIL_EXPAND0(JSON5_DETAIL_EXPAND0(JSON5_DETAIL_EXPAND0(JSON5_DETAIL_EXPAND0(JSON5_DETAIL_EXPAND0(__VA_ARGS__))))))
#define JSON5_DETAIL_EXPAND0(...) __VA_ARGS__

#define JSON5_DETAIL_FOR_EACH(macro, ctx, ...)                                 \
    __VA_OPT__(JSON5_DETAIL_EXPAND4(JSON5_DETAIL_FOR_EACH_IMPL(macro, ctx, __VA_ARGS__)))

#define JSON5_DETAIL_FOR_EACH_IMPL(macro, ctx, a, ...)                         \
    macro(ctx, a)                                                              \
    __VA_OPT__(, JSON5_DETAIL_FOR_EACH_AGAIN JSON5_DETAIL_PARENS (macro, ctx, __VA_ARGS__))

#define JSON5_DETAIL_FOR_EACH_AGAIN() JSON5_DETAIL_FOR_EACH_IMPL

// Single-arg variant for JSON5_DEFINE (no ctx parameter, just field names)
#define JSON5_DETAIL_FOR_EACH1(macro, ...)                                     \
    __VA_OPT__(JSON5_DETAIL_EXPAND4(JSON5_DETAIL_FOR_EACH1_IMPL(macro, __VA_ARGS__)))

#define JSON5_DETAIL_FOR_EACH1_IMPL(macro, a, ...)                             \
    macro(a)                                                                   \
    __VA_OPT__(, JSON5_DETAIL_FOR_EACH1_AGAIN JSON5_DETAIL_PARENS (macro, __VA_ARGS__))

#define JSON5_DETAIL_FOR_EACH1_AGAIN() JSON5_DETAIL_FOR_EACH1_IMPL

/// @endcond

// ═══════════════════════════════════════════════════════════════════
//  Public macros
// ═══════════════════════════════════════════════════════════════════

/// @def JSON5_DEFINE(...)
/// @brief Intrusive serialization macro — place inside a class/struct body.
///
/// Generates `_json5_to_()` and `_json5_from_()` member methods.
/// The primary @ref converter\<T\> template detects these methods
/// automatically, so `to_value()` and `from_value<T>()` work
/// out of the box.
///
/// Missing keys during deserialization are **silently skipped**; the
/// field retains its default value.
///
/// **No limit** on the number of fields (uses variadic templates internally).
///
/// @param ... Comma-separated list of field names.
#define JSON5_DEFINE(...)                                                       \
    void _json5_to_(::json5::value& _json5_j_) const {                        \
        using _json5_self_type_ = std::decay_t<decltype(*this)>;               \
        _json5_j_ = ::json5::object_t{};                                       \
        ::json5::detail::to_fields(_json5_j_, *this,                           \
            JSON5_DETAIL_FOR_EACH1(JSON5_DETAIL_MEMBER_PAIR, __VA_ARGS__));    \
    }                                                                          \
    void _json5_from_(const ::json5::value& _json5_j_) {                       \
        using _json5_self_type_ = std::decay_t<decltype(*this)>;               \
        ::json5::detail::from_fields(_json5_j_, *this,                         \
            JSON5_DETAIL_FOR_EACH1(JSON5_DETAIL_MEMBER_PAIR, __VA_ARGS__));    \
    }

/// @def JSON5_FIELDS(Type, ...)
/// @brief Non-intrusive serialization macro — place outside a class at
/// namespace scope.
///
/// Generates a full `json5::converter<Type>` specialization. Fields
/// must be **publicly accessible**.
///
/// **No limit** on the number of fields (uses variadic templates internally).
///
/// @param Type  The struct/class type.
/// @param ...   Comma-separated list of field names.
#define JSON5_FIELDS(Type, ...)                                                \
    template<>                                                                 \
    struct json5::converter<Type> {                                            \
        static ::json5::value to_json5(const Type& _json5_self_) {             \
            ::json5::value _json5_j_ = ::json5::object_t{};                    \
            ::json5::detail::to_fields(_json5_j_, _json5_self_,                \
                JSON5_DETAIL_FOR_EACH(JSON5_DETAIL_FIELD_PAIR, Type, __VA_ARGS__)); \
            return _json5_j_;                                                  \
        }                                                                      \
        static Type from_json5(const ::json5::value& _json5_j_) {              \
            Type _json5_self_{};                                               \
            ::json5::detail::from_fields(_json5_j_, _json5_self_,              \
                JSON5_DETAIL_FOR_EACH(JSON5_DETAIL_FIELD_PAIR, Type, __VA_ARGS__)); \
            return _json5_self_;                                               \
        }                                                                      \
    };

/// @def JSON5_ENUM(EnumType, ...)
/// @brief Enum-to-string serialization macro — place at namespace scope.
///
/// Generates a `json5::converter<EnumType>` specialization.
///
/// - **to_json5** uses a `switch` statement (O(1) via compiler jump table).
/// - **from_json5** uses a compile-time sorted array with binary search (O(log n)).
/// - Throws `json5::type_error` if the string does not match any enumerator.
///
/// @code
/// enum class Color { Red, Green, Blue };
/// JSON5_ENUM(Color, Red, Green, Blue)
///
/// json5::to_value(Color::Red);   // "Red"
/// json5::from_value<Color>(json5::value("Blue"));  // Color::Blue
/// @endcode
///
/// **No limit** on the number of enumerators.
///
/// @param EnumType  The enum (or enum class) type.
/// @param ...       Comma-separated list of enumerator names (without the type prefix).
#define JSON5_ENUM(EnumType, ...)                                              \
    template<>                                                                 \
    struct json5::converter<EnumType> {                                        \
        static ::json5::value to_json5(EnumType _json5_e_) {                   \
            switch (_json5_e_) {                                               \
                JSON5_DETAIL_ENUM_TO_CASES(EnumType, __VA_ARGS__)              \
            }                                                                  \
            throw ::json5::type_error(                                         \
                "unknown " #EnumType " value: " +                              \
                std::to_string(static_cast<std::underlying_type_t<EnumType>>(_json5_e_))); \
        }                                                                      \
        static EnumType from_json5(const ::json5::value& _json5_j_) {          \
            static constexpr auto _json5_table_ =                              \
                ::json5::detail::make_enum_table<EnumType>(                    \
                    JSON5_DETAIL_FOR_EACH(JSON5_DETAIL_ENUM_PAIR, EnumType, __VA_ARGS__)); \
            const auto& _json5_s_ = _json5_j_.as_string();                     \
            auto* _json5_p_ = ::json5::detail::find_enum_by_name(             \
                _json5_table_, _json5_s_);                                     \
            if (_json5_p_) return _json5_p_->value;                            \
            throw ::json5::type_error(                                         \
                "unknown " #EnumType " string: \"" + _json5_s_ + "\"");        \
        }                                                                      \
    };

/// @cond INTERNAL

// Generate switch cases: case EnumType::Value: return value("Value");
// We use a FOR_EACH that expands with a semicolon-separated list
// But switch cases need a different separator, so we use a dedicated macro.

#define JSON5_DETAIL_ENUM_TO_CASE(EnumType, val)                               \
    case EnumType::val: return ::json5::value(#val);

// Use __VA_OPT__ recursion for switch cases (semicolon-separated, not comma)
#define JSON5_DETAIL_ENUM_TO_CASES(EnumType, ...)                              \
    __VA_OPT__(JSON5_DETAIL_EXPAND4(JSON5_DETAIL_ENUM_CASES_IMPL(EnumType, __VA_ARGS__)))

#define JSON5_DETAIL_ENUM_CASES_IMPL(EnumType, a, ...)                         \
    JSON5_DETAIL_ENUM_TO_CASE(EnumType, a)                                     \
    __VA_OPT__(JSON5_DETAIL_ENUM_CASES_AGAIN JSON5_DETAIL_PARENS (EnumType, __VA_ARGS__))

#define JSON5_DETAIL_ENUM_CASES_AGAIN() JSON5_DETAIL_ENUM_CASES_IMPL

/// @endcond
