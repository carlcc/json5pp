/// @file parser.hpp
/// @brief JSON5 parsing API — @ref json5::parse(), @ref json5::try_parse(),
/// and the @ref json5::parse_options configuration struct.

#pragma once

#include "fwd.hpp"
#include "value.hpp"

#include <functional>
#include <optional>

namespace json5 {

// ── Parse options ─────────────────────────────────────────────────

namespace detail {
    class lexer;
}

/// @brief Options that control how the parser handles JSON5 extensions.
///
/// All JSON5 features are **enabled by default** (equivalent to `json5()`).
/// Three pre-built presets are provided via static factory methods:
///
/// | Preset          | comments | trailing comma | unquoted keys | single quotes | hex numbers | Inf/NaN | multiline strings |
/// |-----------------|----------|----------------|---------------|---------------|-------------|---------|-------------------|
/// | `json5()`       | ✓        | ✓              | ✓             | ✓             | ✓           | ✓       | ✓                 |
/// | `strict_json()` | ✗        | ✗              | ✗             | ✗             | ✗           | ✗       | ✗                 |
/// | `permissive()`  | ✓        | ✓              | ✓             | ✓             | ✓           | ✓       | ✓                 |
///
/// `json5()` and `permissive()` currently allow the same features. The
/// distinction exists so that future extensions (e.g. bareword values,
/// relaxed number formats) can be added to `permissive()` without
/// changing the strict JSON5 spec behaviour of `json5()`.
///
/// ### Strict JSON mode
///
/// @code
/// auto v = json5::parse(input, json5::parse_options::strict_json());
/// @endcode
///
/// ### Custom value hook
///
/// The @ref custom_value_parser callback is invoked whenever the parser
/// encounters an unknown identifier token. Return a @ref value to consume
/// it, or `std::nullopt` to fall back to the default error path.
///
/// @see make_keyword_extension()
struct parse_options {
    bool allow_comments        = true;  ///< Allow `//` and `/* */` comments.
    bool allow_trailing_comma  = true;  ///< Allow trailing `,` in arrays/objects.
    bool allow_unquoted_keys   = true;  ///< Allow identifier-style object keys without quotes.
    bool allow_single_quotes   = true;  ///< Allow `'string'` in addition to `"string"`.
    bool allow_hex_numbers     = true;  ///< Allow `0xFF` hex integer literals.
    bool allow_infinity_nan    = true;  ///< Allow `Infinity`, `-Infinity`, `NaN`.
    bool allow_multiline_strings = true; ///< Allow newlines inside string literals.

    /// @brief Custom value parser hook.
    ///
    /// Called when the parser encounters an unknown identifier.
    ///
    /// @param token      The unrecognized identifier text.
    /// @param remaining  The rest of the unparsed input (for look-ahead).
    /// @return A @ref value to consume the token, or `std::nullopt` to
    ///         let the parser raise a @ref parse_error.
    std::function<std::optional<value>(std::string_view token, std::string_view remaining)> custom_value_parser = nullptr;

    /// @brief Return default JSON5 options (all JSON5 features enabled).
    [[nodiscard]] static parse_options json5() {
        return {};
    }

    /// @brief Return strict JSON options (all JSON5 extensions disabled).
    [[nodiscard]] static parse_options strict_json() {
        parse_options opts;
        opts.allow_comments          = false;
        opts.allow_trailing_comma    = false;
        opts.allow_unquoted_keys     = false;
        opts.allow_single_quotes     = false;
        opts.allow_hex_numbers       = false;
        opts.allow_infinity_nan      = false;
        opts.allow_multiline_strings = false;
        return opts;
    }

    /// @brief Return permissive options (all features enabled; future-proof
    /// for extensions beyond the JSON5 spec).
    [[nodiscard]] static parse_options permissive() {
        return {};
    }
};

// ── Public parsing API ────────────────────────────────────────────

/// @brief Parse a JSON5 string into a @ref value.
///
/// @param input  The JSON5 source text.
/// @param opts   Parsing options (all JSON5 features enabled by default).
/// @return The parsed DOM tree.
/// @throws parse_error on any syntax error.
///
/// @code
/// auto v = json5::parse(R"({ key: 'value', nums: [1, 2, 3,] })");
/// @endcode
[[nodiscard]] value parse(std::string_view input, const parse_options& opts = {});

/// @brief Non-throwing alternative to @ref parse().
///
/// Returns `std::nullopt` on failure instead of throwing.
///
/// @param input      The JSON5 source text.
/// @param error_out  If non-null and parsing fails, receives the error message.
/// @param opts       Parsing options.
/// @return The parsed @ref value, or `std::nullopt` on failure.
///
/// @code
/// std::string err;
/// auto result = json5::try_parse("{bad json", &err);
/// if (!result) {
///     std::cerr << "Parse failed: " << err << "\n";
/// }
/// @endcode
[[nodiscard]] std::optional<value> try_parse(
    std::string_view input,
    std::string* error_out = nullptr,
    const parse_options& opts = {}
);

} // namespace json5
