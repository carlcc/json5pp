/// @file writer.hpp
/// @brief JSON5 serialization API — @ref json5::stringify() and
/// the @ref json5::write_options configuration struct.

#pragma once

#include "fwd.hpp"
#include "value.hpp"

#include <string>

namespace json5 {

// ── Write options ─────────────────────────────────────────────────

/// @brief Options that control how @ref stringify() formats its output.
///
/// Four pre-built presets are provided via static factory methods:
///
/// | Preset            | `indent` | `newline` | `colon_space` | `single_quotes` | `unquoted_keys` | `trailing_comma` |
/// |-------------------|----------|-----------|---------------|------------------|------------------|-------------------|
/// | `compact()`       | `""`     | `""`      | `""`          | `false`          | `false`          | `false`           |
/// | `pretty()`        | `"  "`   | `"\n"`    | `" "`         | `false`          | `false`          | `false`           |
/// | `compact_json5()` | `""`     | `""`      | `""`          | `true`           | `true`           | `false`           |
/// | `pretty_json5()`  | `"  "`   | `"\n"`    | `" "`         | `true`           | `true`           | `true`            |
///
/// You can also construct a fully custom `write_options`:
///
/// @code
/// json5::write_options custom;
/// custom.indent      = "\t";
/// custom.newline     = "\n";
/// custom.colon_space = " ";
/// json5::stringify(v, custom);
/// @endcode
///
/// ### Special value handling
///
/// | Value         | Output       |
/// |---------------|-------------|
/// | `Infinity`    | `Infinity`  |
/// | `-Infinity`   | `-Infinity` |
/// | `NaN`         | `NaN`       |
/// | whole `double`| appends `.0` (e.g. `3.0` not `3`) |
struct write_options {
    std::string indent       = "";  ///< Indentation string per nesting level (empty = compact).
    std::string newline      = "";  ///< Line break string (`"\n"` for pretty, `""` for compact).
    std::string colon_space  = "";  ///< Space inserted after `:` in objects.
    std::string comma_space  = "";  ///< Space inserted after `,`.
    bool single_quotes       = false;  ///< Use `'` instead of `"` for strings.
    bool unquoted_keys       = false;  ///< Omit quotes on valid-identifier object keys.
    bool trailing_comma      = false;  ///< Append a trailing `,` after the last element.

    /// @brief Return compact / minified options (the default).
    [[nodiscard]] static write_options compact() {
        return {};
    }

    /// @brief Return pretty-printed JSON options.
    /// @param indent_str  The indentation string per level (default: two spaces).
    [[nodiscard]] static write_options pretty(std::string indent_str = "  ") {
        write_options opts;
        opts.indent = std::move(indent_str);
        opts.newline = "\n";
        opts.colon_space = " ";
        return opts;
    }

    /// @brief Return compact JSON5-style options (unquoted keys, single quotes, minified).
    [[nodiscard]] static write_options compact_json5() {
        write_options opts;
        opts.single_quotes = true;
        opts.unquoted_keys = true;
        return opts;
    }

    /// @brief Return pretty-printed JSON5-style options (unquoted keys, single quotes,
    /// trailing commas).
    /// @param indent_str  The indentation string per level (default: two spaces).
    [[nodiscard]] static write_options pretty_json5(std::string indent_str = "  ") {
        write_options opts;
        opts.indent = std::move(indent_str);
        opts.newline = "\n";
        opts.colon_space = " ";
        opts.single_quotes = true;
        opts.unquoted_keys = true;
        opts.trailing_comma = true;
        return opts;
    }
};

// ── Public serialization API ──────────────────────────────────────

/// @brief Serialize a @ref value to a string.
///
/// By default produces compact (minified) output. Use one of the
/// @ref write_options presets for human-readable formatting.
///
/// @param v     The value to serialize.
/// @param opts  Formatting options.
/// @return The serialized string.
///
/// @code
/// json5::stringify(v);                                        // compact
/// json5::stringify(v, json5::write_options::pretty());        // pretty JSON
/// json5::stringify(v, json5::write_options::compact_json5()); // compact JSON5
/// json5::stringify(v, json5::write_options::pretty_json5());  // pretty JSON5
/// @endcode
[[nodiscard]] std::string stringify(const value& v, const write_options& opts = {});

} // namespace json5
