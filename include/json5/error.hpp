/// @file error.hpp
/// @brief Exception types thrown by the json5pp library.
///
/// Two exception classes are provided:
///
/// - @ref json5::parse_error — thrown by @ref parse() when the input is
///   not valid JSON5, carrying line/column information.
/// - @ref json5::type_error — thrown by @ref value accessor methods
///   (`as_bool()`, `as_string()`, `operator[]`, etc.) when the stored
///   type does not match the requested type.

#pragma once

#include <stdexcept>
#include <string>
#include <cstddef>

namespace json5 {

/// @brief Exception thrown when JSON5 parsing fails.
///
/// Carries the line and column where the error was detected so that
/// callers can produce IDE-friendly diagnostics.
///
/// The formatted `what()` string looks like:
/// @code
/// json5 parse error at line 3, column 5: unexpected token
/// @endcode
///
/// @see parse(), try_parse()
class parse_error : public std::runtime_error {
public:
    /// @brief Construct a parse_error with location information.
    ///
    /// @param message  A human-readable description of the error.
    /// @param line     1-based line number where the error was detected.
    /// @param column   1-based column number where the error was detected.
    parse_error(const std::string& message, std::size_t line, std::size_t column)
        : std::runtime_error(format_message(message, line, column))
        , line_(line)
        , column_(column)
    {}

    /// @brief Return the 1-based line number of the error.
    [[nodiscard]] std::size_t line() const noexcept { return line_; }

    /// @brief Return the 1-based column number of the error.
    [[nodiscard]] std::size_t column() const noexcept { return column_; }

private:
    static std::string format_message(const std::string& msg, std::size_t line, std::size_t col) {
        return "json5 parse error at line " + std::to_string(line) +
               ", column " + std::to_string(col) + ": " + msg;
    }

    std::size_t line_;
    std::size_t column_;
};

/// @brief Exception thrown on type mismatch when accessing @ref value.
///
/// For example, calling `value::as_string()` on an integer value throws:
/// @code
/// json5 type error: expected string, got integer
/// @endcode
///
/// @see value
class type_error : public std::runtime_error {
public:
    /// @brief Construct a type_error.
    /// @param message  Description including the expected and actual types.
    explicit type_error(const std::string& message)
        : std::runtime_error("json5 type error: " + message)
    {}
};

} // namespace json5
