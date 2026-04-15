//
// json5.cpp — Core compilation unit for json5pp
// Contains: Lexer, Parser, Writer implementations
//

#include "json5/parser.hpp"
#include "json5/writer.hpp"
#include "json5/error.hpp"

#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>

namespace json5 {
namespace detail {

// ═══════════════════════════════════════════════════════════════════
//  Token types
// ═══════════════════════════════════════════════════════════════════

enum class token_type {
    // Structural
    lbrace,         // {
    rbrace,         // }
    lbracket,       // [
    rbracket,       // ]
    colon,          // :
    comma,          // ,

    // Literals
    string_literal, // "..." or '...'
    number_literal, // 123, 0xFF, 1.5e3, Infinity, NaN
    true_literal,   // true
    false_literal,  // false
    null_literal,   // null

    // JSON5 extras
    identifier,     // unquoted key

    // Special
    eof,
    error
};

// ═══════════════════════════════════════════════════════════════════
//  Lexer — hand-written state machine
// ═══════════════════════════════════════════════════════════════════

class lexer {
public:
    lexer(std::string_view input, const parse_options& opts)
        : begin_(input.data())
        , end_(input.data() + input.size())
        , cur_(input.data())
        , opts_(opts)
        , line_(1)
        , col_(1)
    {}

    struct token {
        token_type type = token_type::eof;
        std::string_view text;        // raw text of the token
        std::string string_value;     // decoded string (for string_literal)
        double number_value = 0.0;    // numeric value
        int64_t integer_value = 0;    // integer value
        bool is_integer = false;      // true if the number is an integer
        std::size_t line = 0;
        std::size_t col = 0;
    };

    token next_token() {
        if (!skip_whitespace_and_comments()) {
            token_start_line_ = line_;
            token_start_col_ = col_;
            return make_error("unterminated block comment");
        }

        if (cur_ >= end_) {
            return make_token(token_type::eof, {});
        }

        token_start_line_ = line_;
        token_start_col_ = col_;

        char c = *cur_;

        // Single-character structural tokens
        switch (c) {
            case '{': advance(); return make_token(token_type::lbrace, "{");
            case '}': advance(); return make_token(token_type::rbrace, "}");
            case '[': advance(); return make_token(token_type::lbracket, "[");
            case ']': advance(); return make_token(token_type::rbracket, "]");
            case ':': advance(); return make_token(token_type::colon, ":");
            case ',': advance(); return make_token(token_type::comma, ",");
            default: break;
        }

        // String literal
        if (c == '"' || (c == '\'' && opts_.allow_single_quotes)) {
            return lex_string(c);
        }

        // Number literal (including +/- prefix)
        if (c == '-' || c == '+' || (c >= '0' && c <= '9') || c == '.') {
            return lex_number();
        }

        // Identifier or keyword (true/false/null/Infinity/NaN)
        if (is_id_start(c)) {
            return lex_identifier();
        }

        // Error
        advance();
        return make_error("unexpected character: '" + std::string(1, c) + "'");
    }

    [[nodiscard]] std::size_t line() const noexcept { return line_; }
    [[nodiscard]] std::size_t col()  const noexcept { return col_; }

    [[nodiscard]] std::string_view remaining() const noexcept {
        return {cur_, static_cast<std::size_t>(end_ - cur_)};
    }

private:
    const char* begin_;
    const char* end_;
    const char* cur_;
    const parse_options& opts_;
    std::size_t line_;
    std::size_t col_;
    std::size_t token_start_line_ = 1;
    std::size_t token_start_col_ = 1;

    void advance() {
        if (cur_ < end_) {
            if (*cur_ == '\n') {
                ++line_;
                col_ = 1;
            } else if (*cur_ == '\r') {
                // CR: treat as newline (classic Mac OS style)
                // If followed by LF, the LF will be consumed by the next
                // advance() call which will NOT double-increment line_
                // because we set a flag here.
                ++line_;
                col_ = 1;
                ++cur_;
                // If \r\n, skip the \n so we don't double-count lines
                if (cur_ < end_ && *cur_ == '\n') {
                    ++cur_;
                }
                return;
            } else {
                ++col_;
            }
            ++cur_;
        }
    }

    char peek() const {
        return cur_ < end_ ? *cur_ : '\0';
    }

    char peek_next() const {
        return (cur_ + 1 < end_) ? *(cur_ + 1) : '\0';
    }

    token make_token(token_type type, std::string_view text) {
        token t;
        t.type = type;
        t.text = text;
        t.line = token_start_line_;
        t.col  = token_start_col_;
        return t;
    }

    token make_error(const std::string& msg) {
        token t;
        t.type = token_type::error;
        t.string_value = msg;
        t.line = token_start_line_;
        t.col  = token_start_col_;
        return t;
    }

    // ── Whitespace & comments ─────────────────────────────────────

    static bool is_json5_whitespace(char c) {
        // JSON5 whitespace: Tab, VT, FF, SP, NBSP, BOM, and line terminators
        // (CR and LF are handled by advance() as line terminators)
        switch (c) {
            case ' ':    // Space
            case '\t':   // Tab
            case '\r':   // CR  (line terminator)
            case '\n':   // LF  (line terminator)
            case '\v':   // Vertical tab (0x0B)
            case '\f':   // Form feed (0x0C)
                return true;
            default:
                // Check for NBSP (0xC2 0xA0 in UTF-8) and BOM (0xEF 0xBB 0xBF)
                // would require multi-byte lookahead; covered by unsigned check
                return false;
        }
    }

    // Returns false if an unterminated block comment is found.
    bool skip_whitespace_and_comments() {
        while (cur_ < end_) {
            char c = *cur_;

            // Whitespace
            if (is_json5_whitespace(c)) {
                advance();
                continue;
            }

            // Comments
            if (c == '/' && opts_.allow_comments) {
                if (peek_next() == '/') {
                    skip_line_comment();
                    continue;
                }
                if (peek_next() == '*') {
                    if (!skip_block_comment()) {
                        return false; // unterminated block comment
                    }
                    continue;
                }
            }

            break;
        }
        return true;
    }

    void skip_line_comment() {
        advance(); // /
        advance(); // /
        while (cur_ < end_ && *cur_ != '\n' && *cur_ != '\r') {
            advance();
        }
        // Don't consume the line terminator — let skip_whitespace handle it
    }

    bool skip_block_comment() {
        advance(); // /
        advance(); // *
        while (cur_ < end_) {
            if (*cur_ == '*' && peek_next() == '/') {
                advance(); // *
                advance(); // /
                return true;
            }
            advance();
        }
        // Unterminated block comment — report error
        return false;
    }

    // ── String lexing ─────────────────────────────────────────────

    token lex_string(char quote) {
        const char* start = cur_;
        advance(); // opening quote

        const char* str_start = cur_; // start of string content (after quote)
        std::string decoded;
        bool has_escape = false;

        while (cur_ < end_ && *cur_ != quote) {
            if (*cur_ == '\\') {
                if (!has_escape) {
                    // First escape: copy all previous plain chars into decoded
                    decoded.append(str_start, cur_);
                    has_escape = true;
                }
                advance(); // backslash
                if (cur_ >= end_) break;

                char esc = *cur_;
                switch (esc) {
                    case '"':  decoded += '"';  break;
                    case '\'': decoded += '\''; break;
                    case '\\': decoded += '\\'; break;
                    case '/':  decoded += '/';  break;
                    case 'b':  decoded += '\b'; break;
                    case 'f':  decoded += '\f'; break;
                    case 'n':  decoded += '\n'; break;
                    case 'r':  decoded += '\r'; break;
                    case 't':  decoded += '\t'; break;
                    case '0':  decoded += '\0'; break;
                    case 'u': {
                        advance();
                        auto cp = parse_unicode_escape();
                        if (cp < 0) return make_error("invalid unicode escape");
                        encode_utf8(decoded, static_cast<uint32_t>(cp));
                        continue; // skip the advance() at the end
                    }
                    case '\n':
                        // Line continuation (JSON5): \<LF>
                        if (!opts_.allow_multiline_strings) {
                            return make_error("multiline strings are not allowed");
                        }
                        break;
                    case '\r':
                        // Line continuation: \<CR> or \<CRLF>
                        // advance() at the end of the switch will consume \r
                        // (and the optional \n after it, since advance() handles CRLF)
                        if (!opts_.allow_multiline_strings) {
                            return make_error("multiline strings are not allowed");
                        }
                        break;
                    default:
                        decoded += esc;
                        break;
                }
                advance();
            } else if (*cur_ == '\n' || *cur_ == '\r') {
                // Unescaped line terminators are never allowed in JSON5 strings.
                // Multi-line strings in JSON5 use line continuations (\<newline>),
                // not raw newlines.
                return make_error("unterminated string (newline in string)");
            } else {
                if (has_escape) {
                    decoded += *cur_;
                }
                advance();
            }
        }

        if (cur_ >= end_) {
            return make_error("unterminated string");
        }

        advance(); // closing quote

        std::string_view raw(start, static_cast<std::size_t>(cur_ - start));

        token tok = make_token(token_type::string_literal, raw);
        if (has_escape) {
            tok.string_value = std::move(decoded);
        } else {
            // No escape: direct substring (skip quotes)
            tok.string_value = std::string(start + 1, cur_ - 1);
        }
        return tok;
    }

    int32_t parse_unicode_escape() {
        // Expect 4 hex digits
        if (cur_ + 4 > end_) return -1;
        uint32_t cp = 0;
        for (int i = 0; i < 4; ++i) {
            char c = *cur_;
            cp <<= 4;
            if (c >= '0' && c <= '9') cp |= (c - '0');
            else if (c >= 'a' && c <= 'f') cp |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') cp |= (c - 'A' + 10);
            else return -1;
            advance();
        }

        // Handle surrogate pairs
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            // High surrogate, expect \uXXXX low surrogate
            if (cur_ + 6 <= end_ && *cur_ == '\\') {
                advance();
                if (*cur_ == 'u') {
                    advance();
                    uint32_t low = 0;
                    for (int i = 0; i < 4; ++i) {
                        char c = *cur_;
                        low <<= 4;
                        if (c >= '0' && c <= '9') low |= (c - '0');
                        else if (c >= 'a' && c <= 'f') low |= (c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F') low |= (c - 'A' + 10);
                        else return -1;
                        advance();
                    }
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else {
                        return -1;
                    }
                } else {
                    return -1;
                }
            } else {
                return -1;
            }
        }

        return static_cast<int32_t>(cp);
    }

    static void encode_utf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    // ── Number lexing ─────────────────────────────────────────────

    token lex_number() {
        const char* start = cur_;
        bool negative = false;

        // Sign
        if (*cur_ == '+' || *cur_ == '-') {
            negative = (*cur_ == '-');
            advance();
            // After sign, check for Infinity/NaN
            if (cur_ < end_ && is_id_start(*cur_)) {
                return lex_signed_keyword(start, negative);
            }
        }

        const char* num_start = cur_; // start of numeric content (after optional sign)

        if (cur_ >= end_) return make_error("unexpected end of number");

        // Hex: 0x or 0X
        if (*cur_ == '0' && (cur_ + 1 < end_) &&
            (*(cur_ + 1) == 'x' || *(cur_ + 1) == 'X') &&
            opts_.allow_hex_numbers) {
            advance(); // 0
            advance(); // x
            return lex_hex_number(start, negative);
        }

        // Reject octal/noctal: leading zero followed by digit (e.g. 010, 098)
        // JSON5 (like JSON) forbids leading zeros. Only "0" by itself,
        // "0.", "0e", "0x" are valid.
        if (*cur_ == '0' && (cur_ + 1 < end_) &&
            *(cur_ + 1) >= '0' && *(cur_ + 1) <= '9') {
            // Consume all digits to show the full invalid token
            advance(); // the leading 0
            while (cur_ < end_ && (*cur_ >= '0' && *cur_ <= '9')) {
                advance();
            }
            std::string_view text(start, static_cast<std::size_t>(cur_ - start));
            return make_error("leading zeros are not allowed in JSON5: " + std::string(text));
        }

        // Decimal number
        bool has_dot = false;
        bool has_exp = false;

        // Leading dot: .5
        if (*cur_ == '.') {
            has_dot = true;
            advance();
        }

        // Integer part
        while (cur_ < end_ && (*cur_ >= '0' && *cur_ <= '9')) {
            advance();
        }

        // Fraction
        if (cur_ < end_ && *cur_ == '.' && !has_dot) {
            has_dot = true;
            advance();
            while (cur_ < end_ && (*cur_ >= '0' && *cur_ <= '9')) {
                advance();
            }
        }

        // Exponent
        if (cur_ < end_ && (*cur_ == 'e' || *cur_ == 'E')) {
            has_exp = true;
            advance();
            if (cur_ < end_ && (*cur_ == '+' || *cur_ == '-')) advance();
            while (cur_ < end_ && (*cur_ >= '0' && *cur_ <= '9')) {
                advance();
            }
        }

        std::string_view text(start, static_cast<std::size_t>(cur_ - start));
        token tok = make_token(token_type::number_literal, text);

        if (!has_dot && !has_exp) {
            // Try integer — from_chars handles '-' but not '+'
            int64_t ival = 0;
            const char* parse_start = negative ? start : num_start;
            auto [ptr, ec] = std::from_chars(parse_start, cur_, ival);
            if (ec == std::errc{} && ptr == cur_) {
                tok.integer_value = ival;
                tok.number_value = static_cast<double>(ival);
                tok.is_integer = true;
                return tok;
            }
        }

        // Parse as double
        // std::from_chars for double may not handle leading +/./etc on all compilers
        // Use strtod as fallback
        std::string num_str(start, cur_);
        char* endp = nullptr;
        double dval = std::strtod(num_str.c_str(), &endp);
        if (endp == num_str.c_str() + num_str.size()) {
            tok.number_value = dval;
            tok.is_integer = false;
            return tok;
        }

        return make_error("invalid number: " + std::string(text));
    }

    token lex_hex_number(const char* start, bool negative) {
        const char* hex_start = cur_;
        while (cur_ < end_ && is_hex_digit(*cur_)) {
            advance();
        }
        if (cur_ == hex_start) {
            return make_error("expected hex digits after 0x");
        }

        std::string_view text(start, static_cast<std::size_t>(cur_ - start));

        // Parse hex value
        uint64_t val = 0;
        for (const char* p = hex_start; p < cur_; ++p) {
            val <<= 4;
            if (*p >= '0' && *p <= '9') val |= (*p - '0');
            else if (*p >= 'a' && *p <= 'f') val |= (*p - 'a' + 10);
            else if (*p >= 'A' && *p <= 'F') val |= (*p - 'A' + 10);
        }

        int64_t ival = negative ? -static_cast<int64_t>(val) : static_cast<int64_t>(val);

        token tok = make_token(token_type::number_literal, text);
        tok.integer_value = ival;
        tok.number_value = static_cast<double>(ival);
        tok.is_integer = true;
        return tok;
    }

    token lex_signed_keyword(const char* start, bool negative) {
        // After +/- we expect Infinity or NaN
        const char* id_start = cur_;
        while (cur_ < end_ && is_id_continue(*cur_)) {
            advance();
        }
        std::string_view id(id_start, static_cast<std::size_t>(cur_ - id_start));
        std::string_view text(start, static_cast<std::size_t>(cur_ - start));

        if (id == "Infinity" && opts_.allow_infinity_nan) {
            token tok = make_token(token_type::number_literal, text);
            tok.number_value = negative ? -std::numeric_limits<double>::infinity()
                                        : std::numeric_limits<double>::infinity();
            tok.is_integer = false;
            return tok;
        }
        if (id == "NaN" && opts_.allow_infinity_nan) {
            token tok = make_token(token_type::number_literal, text);
            tok.number_value = std::numeric_limits<double>::quiet_NaN();
            tok.is_integer = false;
            return tok;
        }

        return make_error("unexpected identifier after sign: " + std::string(id));
    }

    // ── Identifier/keyword lexing ─────────────────────────────────

    token lex_identifier() {
        const char* start = cur_;
        advance();
        while (cur_ < end_ && is_id_continue(*cur_)) {
            advance();
        }

        std::string_view text(start, static_cast<std::size_t>(cur_ - start));

        if (text == "true")  return make_token(token_type::true_literal, text);
        if (text == "false") return make_token(token_type::false_literal, text);
        if (text == "null")  return make_token(token_type::null_literal, text);

        if (text == "Infinity" && opts_.allow_infinity_nan) {
            token tok = make_token(token_type::number_literal, text);
            tok.number_value = std::numeric_limits<double>::infinity();
            tok.is_integer = false;
            return tok;
        }
        if (text == "NaN" && opts_.allow_infinity_nan) {
            token tok = make_token(token_type::number_literal, text);
            tok.number_value = std::numeric_limits<double>::quiet_NaN();
            tok.is_integer = false;
            return tok;
        }

        return make_token(token_type::identifier, text);
    }

    // ── Character classification ──────────────────────────────────

    static bool is_id_start(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               c == '_' || c == '$';
    }

    static bool is_id_continue(char c) {
        return is_id_start(c) || (c >= '0' && c <= '9');
    }

    static bool is_hex_digit(char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
    }
};

// ═══════════════════════════════════════════════════════════════════
//  Parser — recursive descent
// ═══════════════════════════════════════════════════════════════════

class parser {
public:
    parser(std::string_view input, const parse_options& opts)
        : lex_(input, opts)
        , opts_(opts)
    {
        advance(); // prime the lookahead
    }

    value parse() {
        value result = parse_value();
        if (current_.type != token_type::eof) {
            error("unexpected token after value");
        }
        return result;
    }

private:
    lexer lex_;
    lexer::token current_;
    const parse_options& opts_;

    void advance() {
        current_ = lex_.next_token();
        if (current_.type == token_type::error) {
            throw parse_error(current_.string_value, current_.line, current_.col);
        }
    }

    void expect(token_type type, const char* msg) {
        if (current_.type != type) {
            error(msg);
        }
        advance();
    }

    [[noreturn]] void error(const char* msg) {
        throw parse_error(msg, current_.line, current_.col);
    }

    [[noreturn]] void error(const std::string& msg) {
        throw parse_error(msg, current_.line, current_.col);
    }

    value parse_value() {
        switch (current_.type) {
            case token_type::null_literal:
                advance();
                return value(nullptr);

            case token_type::true_literal:
                advance();
                return value(true);

            case token_type::false_literal:
                advance();
                return value(false);

            case token_type::number_literal: {
                auto tok = current_;
                advance();
                if (tok.is_integer)
                    return value(tok.integer_value);
                return value(tok.number_value);
            }

            case token_type::string_literal: {
                std::string s = std::move(current_.string_value);
                advance();
                return value(std::move(s));
            }

            case token_type::lbrace:
                return parse_object();

            case token_type::lbracket:
                return parse_array();

            case token_type::identifier: {
                // Try custom value parser extension
                if (opts_.custom_value_parser) {
                    auto result = opts_.custom_value_parser(
                        current_.text, lex_.remaining());
                    if (result.has_value()) {
                        advance();
                        return std::move(*result);
                    }
                }
                error("unexpected identifier: " + std::string(current_.text));
            }

            default:
                error("unexpected token");
        }
    }

    value parse_object() {
        advance(); // consume {

        object_t obj;

        if (current_.type == token_type::rbrace) {
            advance();
            return value(std::move(obj));
        }

        while (true) {
            // Key: string or identifier (unquoted key)
            std::string key;
            if (current_.type == token_type::string_literal) {
                key = std::move(current_.string_value);
                advance();
            } else if (current_.type == token_type::identifier && opts_.allow_unquoted_keys) {
                key = std::string(current_.text);
                advance();
            } else if (current_.type == token_type::number_literal && opts_.allow_unquoted_keys) {
                // JSON5 allows numeric keys
                key = std::string(current_.text);
                advance();
            } else {
                error("expected object key (string or identifier)");
            }

            expect(token_type::colon, "expected ':' after object key");

            value val = parse_value();

            // Duplicate keys: last value wins (per JSON/JSON5 convention)
            bool replaced = false;
            for (auto& [k, v] : obj) {
                if (k == key) {
                    v = std::move(val);
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                obj.emplace_back(std::move(key), std::move(val));
            }

            if (current_.type == token_type::comma) {
                advance();
                // Trailing comma
                if (current_.type == token_type::rbrace && opts_.allow_trailing_comma) {
                    break;
                }
            } else {
                break;
            }
        }

        if (current_.type != token_type::rbrace) {
            error("expected '}' or ',' in object");
        }
        advance();
        return value(std::move(obj));
    }

    value parse_array() {
        advance(); // consume [

        array_t arr;

        if (current_.type == token_type::rbracket) {
            advance();
            return value(std::move(arr));
        }

        while (true) {
            arr.push_back(parse_value());

            if (current_.type == token_type::comma) {
                advance();
                // Trailing comma
                if (current_.type == token_type::rbracket && opts_.allow_trailing_comma) {
                    break;
                }
            } else {
                break;
            }
        }

        if (current_.type != token_type::rbracket) {
            error("expected ']' or ',' in array");
        }
        advance();
        return value(std::move(arr));
    }
};

// ═══════════════════════════════════════════════════════════════════
//  Writer — JSON5 serializer
// ═══════════════════════════════════════════════════════════════════

class writer {
public:
    explicit writer(const write_options& opts)
        : opts_(opts)
        , depth_(0)
        , pretty_(!opts.indent.empty())
    {}

    std::string write(const value& v) {
        buf_.clear();
        buf_.reserve(256);
        write_value(v);
        return std::move(buf_);
    }

private:
    const write_options& opts_;
    std::string buf_;
    int depth_;
    bool pretty_;

    void write_value(const value& v) {
        switch (v.get_type()) {
            case value_type::null:
                buf_ += "null";
                break;

            case value_type::boolean:
                buf_ += v.as_bool() ? "true" : "false";
                break;

            case value_type::integer:
                write_integer(v.as_integer());
                break;

            case value_type::floating:
                write_double(v.as_double());
                break;

            case value_type::string:
                write_string(v.as_string());
                break;

            case value_type::array:
                write_array(v.as_array());
                break;

            case value_type::object:
                write_object(v.as_object());
                break;
        }
    }

    void write_integer(int64_t v) {
        char buf[32];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
        buf_.append(buf, ptr);
    }

    void write_double(double v) {
        if (std::isinf(v)) {
            buf_ += (v < 0) ? "-Infinity" : "Infinity";
            return;
        }
        if (std::isnan(v)) {
            buf_ += "NaN";
            return;
        }

        char buf[64];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
        if (ec == std::errc{}) {
            std::string_view sv(buf, static_cast<std::size_t>(ptr - buf));
            // Ensure there is a decimal point or exponent to distinguish from integer
            if (sv.find('.') == std::string_view::npos &&
                sv.find('e') == std::string_view::npos &&
                sv.find('E') == std::string_view::npos) {
                buf_.append(sv);
                buf_ += ".0";
            } else {
                buf_.append(sv);
            }
        } else {
            // Fallback
            std::ostringstream oss;
            oss << v;
            buf_ += oss.str();
        }
    }

    void write_string(const std::string& s) {
        char quote = opts_.single_quotes ? '\'' : '"';
        buf_ += quote;

        for (char c : s) {
            switch (c) {
                case '\\': buf_ += "\\\\"; break;
                case '\b': buf_ += "\\b";  break;
                case '\f': buf_ += "\\f";  break;
                case '\n': buf_ += "\\n";  break;
                case '\r': buf_ += "\\r";  break;
                case '\t': buf_ += "\\t";  break;
                case '\0': buf_ += "\\0";  break;
                case '"':
                    if (quote == '"') buf_ += "\\\"";
                    else buf_ += '"';
                    break;
                case '\'':
                    if (quote == '\'') buf_ += "\\'";
                    else buf_ += '\'';
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        // Control character: \u00XX
                        char hex[7];
                        std::snprintf(hex, sizeof(hex), "\\u%04x",
                                     static_cast<unsigned char>(c));
                        buf_ += hex;
                    } else {
                        buf_ += c;
                    }
                    break;
            }
        }

        buf_ += quote;
    }

    void write_array(const array_t& arr) {
        if (arr.empty()) {
            buf_ += "[]";
            return;
        }

        buf_ += '[';
        ++depth_;

        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) {
                buf_ += ',';
                buf_ += opts_.comma_space;
            }
            if (pretty_) {
                buf_ += opts_.newline;
                write_indent();
            }
            write_value(arr[i]);
        }

        if (opts_.trailing_comma && !arr.empty()) {
            buf_ += ',';
        }

        --depth_;
        if (pretty_) {
            buf_ += opts_.newline;
            write_indent();
        }
        buf_ += ']';
    }

    void write_object(const object_t& obj) {
        if (obj.empty()) {
            buf_ += "{}";
            return;
        }

        buf_ += '{';
        ++depth_;

        for (std::size_t i = 0; i < obj.size(); ++i) {
            if (i > 0) {
                buf_ += ',';
                buf_ += opts_.comma_space;
            }
            if (pretty_) {
                buf_ += opts_.newline;
                write_indent();
            }

            // Key
            if (opts_.unquoted_keys && is_valid_identifier(obj[i].first)) {
                buf_ += obj[i].first;
            } else {
                write_string(obj[i].first);
            }

            buf_ += ':';
            buf_ += opts_.colon_space;

            // Value
            write_value(obj[i].second);
        }

        if (opts_.trailing_comma && !obj.empty()) {
            buf_ += ',';
        }

        --depth_;
        if (pretty_) {
            buf_ += opts_.newline;
            write_indent();
        }
        buf_ += '}';
    }

    void write_indent() {
        for (int i = 0; i < depth_; ++i) {
            buf_ += opts_.indent;
        }
    }

    static bool is_valid_identifier(const std::string& s) {
        if (s.empty()) return false;
        char c = s[0];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              c == '_' || c == '$'))
            return false;
        for (std::size_t i = 1; i < s.size(); ++i) {
            c = s[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '$'))
                return false;
        }
        // Avoid keywords
        if (s == "true" || s == "false" || s == "null" ||
            s == "Infinity" || s == "NaN")
            return false;
        return true;
    }
};

} // namespace detail

// ═══════════════════════════════════════════════════════════════════
//  Public API implementations
// ═══════════════════════════════════════════════════════════════════

value parse(std::string_view input, const parse_options& opts) {
    detail::parser p(input, opts);
    return p.parse();
}

std::optional<value> try_parse(
    std::string_view input,
    std::string* error_out,
    const parse_options& opts)
{
    try {
        return parse(input, opts);
    } catch (const parse_error& e) {
        if (error_out) *error_out = e.what();
        return std::nullopt;
    }
}

std::string stringify(const value& v, const write_options& opts) {
    detail::writer w(opts);
    return w.write(v);
}

} // namespace json5
