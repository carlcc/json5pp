/// @file macros.hpp
/// @brief Declarative macros for automatic struct serialization.
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
/// - Supports up to **16 fields**.
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
/// - Supports up to **16 fields**.
///
/// ## When to use which
///
/// | Scenario | Macro |
/// |---|---|
/// | You own the class and can modify it | `JSON5_DEFINE` |
/// | You cannot modify the class (third-party, legacy) | `JSON5_FIELDS` |
/// | You need custom logic (not just field mapping) | `converter<T>` specialization |

#pragma once

#include "fwd.hpp"
#include "value.hpp"
#include "converter.hpp"

/// @cond INTERNAL

#define JSON5_DETAIL_TO_FIELD(j, self, field)    \
    j.insert(#field, ::json5::to_value(self.field))

#define JSON5_DETAIL_FROM_FIELD(j, self, field)                        \
    do {                                                               \
        if (j.contains(#field)) {                                      \
            self.field = ::json5::from_value<                          \
                std::decay_t<decltype(self.field)>>(j[#field]);        \
        }                                                              \
    } while (0)

// Helper macros for variadic expansion
#define JSON5_DETAIL_EXPAND(x) x
#define JSON5_DETAIL_FOREACH_1(macro, j, self, a)        macro(j, self, a);
#define JSON5_DETAIL_FOREACH_2(macro, j, self, a, ...)   macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_1(macro, j, self, __VA_ARGS__))
#define JSON5_DETAIL_FOREACH_3(macro, j, self, a, ...)   macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_2(macro, j, self, __VA_ARGS__))
#define JSON5_DETAIL_FOREACH_4(macro, j, self, a, ...)   macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_3(macro, j, self, __VA_ARGS__))
#define JSON5_DETAIL_FOREACH_5(macro, j, self, a, ...)   macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_4(macro, j, self, __VA_ARGS__))
#define JSON5_DETAIL_FOREACH_6(macro, j, self, a, ...)   macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_5(macro, j, self, __VA_ARGS__))
#define JSON5_DETAIL_FOREACH_7(macro, j, self, a, ...)   macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_6(macro, j, self, __VA_ARGS__))
#define JSON5_DETAIL_FOREACH_8(macro, j, self, a, ...)   macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_7(macro, j, self, __VA_ARGS__))
#define JSON5_DETAIL_FOREACH_9(macro, j, self, a, ...)   macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_8(macro, j, self, __VA_ARGS__))
#define JSON5_DETAIL_FOREACH_10(macro, j, self, a, ...)  macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_9(macro, j, self, __VA_ARGS__))
#define JSON5_DETAIL_FOREACH_11(macro, j, self, a, ...)  macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_10(macro, j, self, __VA_ARGS__))
#define JSON5_DETAIL_FOREACH_12(macro, j, self, a, ...)  macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_11(macro, j, self, __VA_ARGS__))
#define JSON5_DETAIL_FOREACH_13(macro, j, self, a, ...)  macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_12(macro, j, self, __VA_ARGS__))
#define JSON5_DETAIL_FOREACH_14(macro, j, self, a, ...)  macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_13(macro, j, self, __VA_ARGS__))
#define JSON5_DETAIL_FOREACH_15(macro, j, self, a, ...)  macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_14(macro, j, self, __VA_ARGS__))
#define JSON5_DETAIL_FOREACH_16(macro, j, self, a, ...)  macro(j, self, a); JSON5_DETAIL_EXPAND(JSON5_DETAIL_FOREACH_15(macro, j, self, __VA_ARGS__))

#define JSON5_DETAIL_ARG_COUNT_IMPL(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N
#define JSON5_DETAIL_ARG_COUNT(...) JSON5_DETAIL_EXPAND(JSON5_DETAIL_ARG_COUNT_IMPL(__VA_ARGS__,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1))

#define JSON5_DETAIL_CONCAT_(a, b) a##b
#define JSON5_DETAIL_CONCAT(a, b) JSON5_DETAIL_CONCAT_(a, b)

#define JSON5_DETAIL_FOREACH(macro, j, self, ...) \
    JSON5_DETAIL_EXPAND(JSON5_DETAIL_CONCAT(JSON5_DETAIL_FOREACH_, JSON5_DETAIL_ARG_COUNT(__VA_ARGS__))(macro, j, self, __VA_ARGS__))

/// @endcond

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
/// @param ... Comma-separated list of field names (up to 16).
#define JSON5_DEFINE(...)                                                      \
    void _json5_to_(::json5::value& _json5_j_) const {                        \
        _json5_j_ = ::json5::object_t{};                                       \
        JSON5_DETAIL_FOREACH(JSON5_DETAIL_TO_FIELD, _json5_j_, (*this), __VA_ARGS__) \
    }                                                                          \
    void _json5_from_(const ::json5::value& _json5_j_) {                       \
        JSON5_DETAIL_FOREACH(JSON5_DETAIL_FROM_FIELD, _json5_j_, (*this), __VA_ARGS__) \
    }

/// @def JSON5_FIELDS(Type, ...)
/// @brief Non-intrusive serialization macro — place outside a class at
/// namespace scope.
///
/// Generates a full `json5::converter<Type>` specialization. Fields
/// must be **publicly accessible**.
///
/// @param Type  The struct/class type.
/// @param ...   Comma-separated list of field names (up to 16).
#define JSON5_FIELDS(Type, ...)                                                \
    template<>                                                                 \
    struct json5::converter<Type> {                                            \
        static ::json5::value to_json5(const Type& _json5_self_) {             \
            ::json5::value _json5_j_ = ::json5::object_t{};                    \
            JSON5_DETAIL_FOREACH(JSON5_DETAIL_TO_FIELD, _json5_j_, _json5_self_, __VA_ARGS__) \
            return _json5_j_;                                                  \
        }                                                                      \
        static Type from_json5(const ::json5::value& _json5_j_) {              \
            Type _json5_self_{};                                               \
            JSON5_DETAIL_FOREACH(JSON5_DETAIL_FROM_FIELD, _json5_j_, _json5_self_, __VA_ARGS__) \
            return _json5_self_;                                               \
        }                                                                      \
    };
