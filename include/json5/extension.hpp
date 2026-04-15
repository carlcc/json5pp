/// @file extension.hpp
/// @brief Extension APIs for customizing JSON5 parsing behavior.
///
/// Two extension mechanisms are provided:
///
/// - @ref make_keyword_extension() — register custom identifiers that map
///   to fixed values (e.g. `undefined → null`, `PI → 3.14…`).
/// - @ref parse_with_transform() — parse, then recursively apply a
///   transformation to every node in the resulting DOM tree.
///
/// For fully custom syntax (e.g. `Date(2024, 1, 15)`), set
/// @ref parse_options::custom_value_parser directly.

#pragma once

#include "fwd.hpp"
#include "value.hpp"
#include "parser.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace json5 {

/// @brief Create a @ref parse_options with a keyword extension.
///
/// Returns a `parse_options` whose @ref parse_options::custom_value_parser
/// maps the given identifiers to values. When the parser encounters an
/// unknown identifier that matches a keyword in the list, it returns the
/// associated value instead of raising an error.
///
/// @param keywords  List of `{identifier, value}` pairs.
/// @return A `parse_options` struct ready to pass to @ref parse().
///
/// @code
/// auto opts = json5::make_keyword_extension({
///     {"undefined", json5::value(nullptr)},
///     {"PI",        json5::value(3.14159265358979)},
///     {"VERSION",   json5::value(2)},
/// });
/// auto v = json5::parse("{ val: undefined, pi: PI }", opts);
/// // v["val"] is null, v["pi"] is 3.14159...
/// @endcode
inline parse_options make_keyword_extension(
    std::initializer_list<std::pair<std::string_view, value>> keywords)
{
    // Copy keyword mappings into a vector for the lambda capture
    std::vector<std::pair<std::string, value>> kw_vec;
    kw_vec.reserve(keywords.size());
    for (const auto& [k, v] : keywords) {
        kw_vec.emplace_back(std::string(k), v);
    }

    parse_options opts;
    opts.custom_value_parser =
        [kw = std::move(kw_vec)](std::string_view token, std::string_view /*remaining*/)
            -> std::optional<value>
        {
            for (const auto& [k, v] : kw) {
                if (k == token) return v;
            }
            return std::nullopt;
        };

    return opts;
}

/// @brief Type alias for a function that transforms a @ref value in-place.
///
/// Used with @ref parse_with_transform(). The function receives a mutable
/// reference and may modify the value arbitrarily.
using value_transformer = std::function<void(value&)>;

/// @brief Parse JSON5 input, then recursively apply a transformation to
/// every value in the DOM tree (depth-first, bottom-up).
///
/// Child nodes are transformed before their parents, so the transformer
/// always sees already-transformed children.
///
/// @param input      JSON5 source text.
/// @param transform  A function applied to each value in the tree.
/// @param opts       Parsing options.
/// @return The parsed and transformed DOM tree.
/// @throws parse_error on syntax errors (before any transformation).
///
/// @code
/// // Clamp all integers to [0, 100]
/// auto result = json5::parse_with_transform(input, [](json5::value& v) {
///     if (v.is_integer()) {
///         auto n = v.as_integer();
///         if (n < 0)   v = int64_t(0);
///         if (n > 100) v = int64_t(100);
///     }
/// });
/// @endcode
[[nodiscard]] inline value parse_with_transform(
    std::string_view input,
    const value_transformer& transform,
    const parse_options& opts = {})
{
    value result = parse(input, opts);

    // Apply transformer recursively
    std::function<void(value&)> apply = [&](value& v) {
        if (v.is_array()) {
            for (auto& elem : v.as_array()) {
                apply(elem);
            }
        } else if (v.is_object()) {
            for (auto& [k, val] : v.as_object()) {
                apply(val);
            }
        }
        transform(v);
    };

    apply(result);
    return result;
}

} // namespace json5
