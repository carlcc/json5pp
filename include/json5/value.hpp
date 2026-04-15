/// @file value.hpp
/// @brief The central DOM type @ref json5::value and its `get<T>()` specializations.

#pragma once

#include "fwd.hpp"
#include "error.hpp"

#include <variant>
#include <initializer_list>
#include <algorithm>
#include <cmath>
#include <cassert>

namespace json5 {

/// @brief The central DOM type representing any JSON5 value.
///
/// Internally stores a `std::variant` over seven alternatives that mirror
/// the JSON5 data model:
///
/// | Index | Alternative       | @ref value_type |
/// |-------|-------------------|-----------------|
/// | 0     | `std::nullptr_t`  | `null`          |
/// | 1     | `bool`            | `boolean`       |
/// | 2     | `int64_t`         | `integer`       |
/// | 3     | `double`          | `floating`      |
/// | 4     | `string_t`        | `string`        |
/// | 5     | `array_t`         | `array`         |
/// | 6     | `object_t`        | `object`        |
///
/// ## Quick examples
///
/// @code
/// // Construction
/// json5::value v1;                   // null
/// json5::value v2 = 42;             // integer
/// json5::value v3 = "hello";        // string
///
/// // DOM manipulation
/// json5::value doc;
/// doc["name"] = "json5pp";          // null → object, inserts key
/// doc["version"] = 1;
///
/// // Type-safe access
/// doc["name"].as_string();          // "json5pp"
/// doc["version"].as_integer();      // 1
/// @endcode
class value {
public:
    /// Alias for @ref value_type.
    using type = value_type;

    // ── Construction ───────────────────────────────────────────────

    /// @name Constructors
    /// @{

    /// @brief Construct a null value (default).
    value() noexcept : storage_(nullptr) {}

    /// @brief Construct a null value from `nullptr`.
    value(std::nullptr_t) noexcept : storage_(nullptr) {}

    /// @brief Construct a boolean value.
    value(bool b) noexcept : storage_(b) {}

    /// @brief Construct an integer value from `int` (promoted to `int64_t`).
    value(int v) noexcept : storage_(static_cast<int64_t>(v)) {}

    /// @brief Construct an integer value.
    value(int64_t v) noexcept : storage_(v) {}

    /// @brief Construct an integer value from `uint64_t` (cast to `int64_t`).
    value(uint64_t v) noexcept : storage_(static_cast<int64_t>(v)) {}

    /// @brief Construct a floating-point value.
    value(double v) noexcept : storage_(v) {}

    /// @brief Construct a string value from a C string.
    value(const char* s) : storage_(string_t(s)) {}

    /// @brief Construct a string value from a string_view.
    value(std::string_view s) : storage_(string_t(s)) {}

    /// @brief Construct a string value (move).
    value(string_t s) : storage_(std::move(s)) {}

    /// @brief Construct an array value (move).
    value(array_t a) : storage_(std::move(a)) {}

    /// @brief Construct an object value (move).
    value(object_t o) : storage_(std::move(o)) {}

    /// @}

    // Copy/move — defaulted
    value(const value&) = default;
    value(value&&) noexcept = default;
    value& operator=(const value&) = default;
    value& operator=(value&&) noexcept = default;

    /// @name Assignment Operators
    /// @brief Assignment operators exist for every constructor parameter type.
    /// @{
    value& operator=(std::nullptr_t) { storage_ = nullptr; return *this; }
    value& operator=(bool b) { storage_ = b; return *this; }
    value& operator=(int v) { storage_ = static_cast<int64_t>(v); return *this; }
    value& operator=(int64_t v) { storage_ = v; return *this; }
    value& operator=(double v) { storage_ = v; return *this; }
    value& operator=(const char* s) { storage_ = string_t(s); return *this; }
    value& operator=(std::string_view s) { storage_ = string_t(s); return *this; }
    value& operator=(string_t s) { storage_ = std::move(s); return *this; }
    value& operator=(array_t a) { storage_ = std::move(a); return *this; }
    value& operator=(object_t o) { storage_ = std::move(o); return *this; }
    /// @}

    // ── Type queries ──────────────────────────────────────────────

    /// @name Type Queries
    /// @{

    /// @brief Return the @ref value_type discriminator for the stored value.
    [[nodiscard]] type get_type() const noexcept {
        return static_cast<type>(storage_.index());
    }

    /// @brief Test whether the stored value is null.
    [[nodiscard]] bool is_null()     const noexcept { return std::holds_alternative<std::nullptr_t>(storage_); }

    /// @brief Test whether the stored value is a boolean.
    [[nodiscard]] bool is_bool()     const noexcept { return std::holds_alternative<bool>(storage_); }

    /// @brief Test whether the stored value is an integer.
    [[nodiscard]] bool is_integer()  const noexcept { return std::holds_alternative<int64_t>(storage_); }

    /// @brief Test whether the stored value is a floating-point number.
    [[nodiscard]] bool is_floating() const noexcept { return std::holds_alternative<double>(storage_); }

    /// @brief Test whether the stored value is a number (integer **or** floating).
    [[nodiscard]] bool is_number()   const noexcept { return is_integer() || is_floating(); }

    /// @brief Test whether the stored value is a string.
    [[nodiscard]] bool is_string()   const noexcept { return std::holds_alternative<string_t>(storage_); }

    /// @brief Test whether the stored value is an array.
    [[nodiscard]] bool is_array()    const noexcept { return std::holds_alternative<array_t>(storage_); }

    /// @brief Test whether the stored value is an object.
    [[nodiscard]] bool is_object()   const noexcept { return std::holds_alternative<object_t>(storage_); }

    /// @}

    // ── Value access (throwing) ───────────────────────────────────

    /// @name Throwing Accessors
    /// Each accessor throws @ref type_error if the stored type does not match.
    /// @{

    /// @brief Return the boolean value.
    /// @throws type_error if the stored type is not `boolean`.
    [[nodiscard]] bool as_bool() const {
        if (auto* p = std::get_if<bool>(&storage_)) return *p;
        throw type_error("expected boolean, got " + std::string(type_name(get_type())));
    }

    /// @brief Return the integer value.
    /// @throws type_error if the stored type is not `integer`.
    [[nodiscard]] int64_t as_integer() const {
        if (auto* p = std::get_if<int64_t>(&storage_)) return *p;
        throw type_error("expected integer, got " + std::string(type_name(get_type())));
    }

    /// @brief Return the value as `double`.
    ///
    /// Unlike other accessors this performs an **implicit widening**: if the
    /// stored type is `integer`, the value is converted to `double`.
    ///
    /// @throws type_error if the stored type is neither `floating` nor `integer`.
    [[nodiscard]] double as_double() const {
        if (auto* p = std::get_if<double>(&storage_)) return *p;
        if (auto* p = std::get_if<int64_t>(&storage_)) return static_cast<double>(*p);
        throw type_error("expected number, got " + std::string(type_name(get_type())));
    }

    /// @brief Return a const reference to the string value.
    /// @throws type_error if the stored type is not `string`.
    [[nodiscard]] const string_t& as_string() const {
        if (auto* p = std::get_if<string_t>(&storage_)) return *p;
        throw type_error("expected string, got " + std::string(type_name(get_type())));
    }

    /// @brief Return a mutable reference to the string value.
    /// @throws type_error if the stored type is not `string`.
    [[nodiscard]] string_t& as_string() {
        if (auto* p = std::get_if<string_t>(&storage_)) return *p;
        throw type_error("expected string, got " + std::string(type_name(get_type())));
    }

    /// @brief Return a const reference to the array.
    /// @throws type_error if the stored type is not `array`.
    [[nodiscard]] const array_t& as_array() const {
        if (auto* p = std::get_if<array_t>(&storage_)) return *p;
        throw type_error("expected array, got " + std::string(type_name(get_type())));
    }

    /// @brief Return a mutable reference to the array.
    /// @throws type_error if the stored type is not `array`.
    [[nodiscard]] array_t& as_array() {
        if (auto* p = std::get_if<array_t>(&storage_)) return *p;
        throw type_error("expected array, got " + std::string(type_name(get_type())));
    }

    /// @brief Return a const reference to the object.
    /// @throws type_error if the stored type is not `object`.
    [[nodiscard]] const object_t& as_object() const {
        if (auto* p = std::get_if<object_t>(&storage_)) return *p;
        throw type_error("expected object, got " + std::string(type_name(get_type())));
    }

    /// @brief Return a mutable reference to the object.
    /// @throws type_error if the stored type is not `object`.
    [[nodiscard]] object_t& as_object() {
        if (auto* p = std::get_if<object_t>(&storage_)) return *p;
        throw type_error("expected object, got " + std::string(type_name(get_type())));
    }

    /// @}

    // ── Pointer access (non-throwing) ─────────────────────────────

    /// @name Non-throwing Pointer Accessors
    /// Return a pointer to the stored value, or `nullptr` if the type
    /// does not match. These never throw.
    /// @{

    /// @brief Return a pointer to the boolean, or `nullptr`.
    [[nodiscard]] const bool*     get_if_bool()     const noexcept { return std::get_if<bool>(&storage_); }

    /// @brief Return a pointer to the integer, or `nullptr`.
    [[nodiscard]] const int64_t*  get_if_integer()  const noexcept { return std::get_if<int64_t>(&storage_); }

    /// @brief Return a pointer to the double, or `nullptr`.
    [[nodiscard]] const double*   get_if_double()   const noexcept { return std::get_if<double>(&storage_); }

    /// @brief Return a pointer to the string, or `nullptr`.
    [[nodiscard]] const string_t* get_if_string()   const noexcept { return std::get_if<string_t>(&storage_); }

    /// @brief Return a pointer to the array, or `nullptr`.
    [[nodiscard]] const array_t*  get_if_array()    const noexcept { return std::get_if<array_t>(&storage_); }

    /// @brief Return a pointer to the object, or `nullptr`.
    [[nodiscard]] const object_t* get_if_object()   const noexcept { return std::get_if<object_t>(&storage_); }

    /// @}

    // ── Typed get<T> ──────────────────────────────────────────────

    /// @brief Extract the value as an explicit C++ type.
    ///
    /// Explicit specializations exist for: `bool`, `int`, `int64_t`,
    /// `uint64_t`, `float`, `double`, `string_t`, `std::string_view`.
    ///
    /// @code
    /// json5::value v = 42;
    /// int x = v.get<int>();           // 42
    /// double d = v.get<double>();     // 42.0
    /// @endcode
    ///
    /// @tparam T  The target type.
    /// @throws type_error on type mismatch.
    template<typename T>
    [[nodiscard]] T get() const;

    // ── Array subscript ───────────────────────────────────────────

    /// @name Subscript Operators
    /// @{

    /// @brief Const array element access by index.
    /// @param index  Zero-based element index.
    /// @throws type_error if not an array.
    /// @throws std::out_of_range if index >= size().
    [[nodiscard]] const value& operator[](std::size_t index) const {
        return as_array().at(index);
    }

    /// @brief Mutable array element access by index.
    ///
    /// If `index >= size()` the array is **resized** to `index + 1`,
    /// with new elements default-initialized to null.
    ///
    /// @param index  Zero-based element index.
    /// @throws type_error if not an array.
    value& operator[](std::size_t index) {
        if (!is_array()) {
            throw type_error("expected array for index access, got " + std::string(type_name(get_type())));
        }
        auto& arr = std::get<array_t>(storage_);
        if (index >= arr.size()) {
            arr.resize(index + 1);
        }
        return arr[index];
    }

    // ── Object subscript ──────────────────────────────────────────

    /// @brief Const object element access by key.
    /// @param key  The string key to look up.
    /// @throws type_error if not an object, or if @p key is not found.
    [[nodiscard]] const value& operator[](std::string_view key) const {
        const auto& obj = as_object();
        for (const auto& [k, v] : obj) {
            if (k == key) return v;
        }
        throw type_error("key not found: " + std::string(key));
    }

    /// @brief Mutable object element access by key.
    ///
    /// - If the value is `null`, it is **converted** to an empty object.
    /// - If @p key does not exist, a new null entry is inserted.
    ///
    /// @param key  The string key to look up or create.
    /// @throws type_error if the value is neither null nor an object.
    value& operator[](std::string_view key) {
        if (is_null()) {
            storage_ = object_t{};
        }
        if (!is_object()) {
            throw type_error("expected object for key access, got " + std::string(type_name(get_type())));
        }
        auto& obj = std::get<object_t>(storage_);
        for (auto& [k, v] : obj) {
            if (k == key) return v;
        }
        obj.emplace_back(string_t(key), value{});
        return obj.back().second;
    }

    /// @}

    // ── Size / empty / contains ───────────────────────────────────

    /// @name Container Queries
    /// @{

    /// @brief Return the number of elements (array), key-value pairs (object),
    /// or characters (string).
    /// @throws type_error if the value is not an array, object, or string.
    [[nodiscard]] std::size_t size() const {
        if (auto* a = get_if_array()) return a->size();
        if (auto* o = get_if_object()) return o->size();
        if (is_string()) return as_string().size();
        throw type_error("size() requires array, object, or string");
    }

    /// @brief Test whether the value is empty.
    ///
    /// Returns `true` for null, empty arrays, empty objects, and empty strings.
    /// Returns `false` for all other values (numbers, booleans, non-empty
    /// containers).
    [[nodiscard]] bool empty() const {
        if (is_null()) return true;
        if (auto* a = get_if_array()) return a->empty();
        if (auto* o = get_if_object()) return o->empty();
        if (is_string()) return as_string().empty();
        return false;
    }

    /// @brief Test whether the object contains a given key.
    ///
    /// Always returns `false` for non-object values (does **not** throw).
    ///
    /// @param key  The key to search for.
    [[nodiscard]] bool contains(std::string_view key) const {
        if (auto* o = get_if_object()) {
            for (const auto& [k, v] : *o) {
                if (k == key) return true;
            }
        }
        return false;
    }

    /// @}

    // ── Array mutators ────────────────────────────────────────────

    /// @name Array Mutators
    /// Both methods auto-convert a `null` value into an empty array
    /// before inserting.
    /// @{

    /// @brief Append a value to the array.
    /// @param v  The value to append.
    /// @throws type_error if the value is not null or array.
    void push_back(value v) {
        if (is_null()) storage_ = array_t{};
        as_array().push_back(std::move(v));
    }

    /// @brief Construct a value in-place at the end of the array.
    /// @tparam Args  Constructor argument types.
    /// @param args   Arguments forwarded to the @ref value constructor.
    /// @return Reference to the newly constructed element.
    /// @throws type_error if the value is not null or array.
    template<typename... Args>
    value& emplace_back(Args&&... args) {
        if (is_null()) storage_ = array_t{};
        return as_array().emplace_back(std::forward<Args>(args)...);
    }

    /// @}

    // ── Object mutators ───────────────────────────────────────────

    /// @name Object Mutators
    /// @{

    /// @brief Insert or update a key-value pair.
    ///
    /// Auto-converts a `null` value into an empty object. If @p key
    /// already exists, its value is **overwritten**.
    ///
    /// @param key  The key.
    /// @param v    The value.
    /// @throws type_error if the value is not null or object.
    void insert(std::string_view key, value v) {
        if (is_null()) storage_ = object_t{};
        auto& obj = as_object();
        for (auto& [k, val] : obj) {
            if (k == key) { val = std::move(v); return; }
        }
        obj.emplace_back(string_t(key), std::move(v));
    }

    /// @brief Remove a key-value pair by key.
    /// @param key  The key to remove.
    /// @return `true` if the key was found and removed, `false` otherwise.
    bool erase(std::string_view key) {
        if (!is_object()) return false;
        auto& obj = std::get<object_t>(storage_);
        auto it = std::find_if(obj.begin(), obj.end(),
            [&](const auto& p) { return p.first == key; });
        if (it != obj.end()) { obj.erase(it); return true; }
        return false;
    }

    /// @}

    // ── Comparison ────────────────────────────────────────────────

    /// @name Comparison Operators
    /// @{

    /// @brief Equality comparison.
    ///
    /// - Two values of the same type compare by content.
    /// - An `integer` and a `floating` compare by converting both to `double`.
    /// - Values of different types are never equal.
    friend bool operator==(const value& lhs, const value& rhs) {
        if (lhs.get_type() != rhs.get_type()) {
            // Allow integer == double comparison
            if (lhs.is_number() && rhs.is_number()) {
                return lhs.as_double() == rhs.as_double();
            }
            return false;
        }
        return lhs.storage_ == rhs.storage_;
    }

    /// @brief Inequality comparison.
    friend bool operator!=(const value& lhs, const value& rhs) {
        return !(lhs == rhs);
    }

    /// @}

    // ── Static factories ──────────────────────────────────────────

    /// @name Static Factory Methods
    /// @{

    /// @brief Create an array value from an initializer list.
    ///
    /// @code
    /// auto arr = json5::value::array({1, 2, "three"});
    /// @endcode
    ///
    /// @param init  Brace-enclosed list of values.
    [[nodiscard]] static value array(std::initializer_list<value> init = {}) {
        return value(array_t(init));
    }

    /// @brief Create an object value from an initializer list of key-value pairs.
    ///
    /// @code
    /// auto obj = json5::value::object({{"key", 42}});
    /// @endcode
    ///
    /// @param init  Brace-enclosed list of `{key, value}` pairs.
    [[nodiscard]] static value object(std::initializer_list<std::pair<string_t, value>> init = {}) {
        return value(object_t(init.begin(), init.end()));
    }

    /// @}

private:
    std::variant<
        std::nullptr_t,  // 0: null
        bool,            // 1: boolean
        int64_t,         // 2: integer
        double,          // 3: floating
        string_t,        // 4: string
        array_t,         // 5: array
        object_t         // 6: object
    > storage_;
};

// ── get<T> specializations ──────────────────────────────────────

/// @cond INTERNAL
template<> inline bool       value::get<bool>()       const { return as_bool(); }
template<> inline int        value::get<int>()        const { return static_cast<int>(as_integer()); }
template<> inline int64_t    value::get<int64_t>()    const { return as_integer(); }
template<> inline uint64_t   value::get<uint64_t>()   const { return static_cast<uint64_t>(as_integer()); }
template<> inline float      value::get<float>()      const { return static_cast<float>(as_double()); }
template<> inline double     value::get<double>()     const { return as_double(); }
template<> inline string_t   value::get<string_t>()   const { return as_string(); }
template<> inline std::string_view value::get<std::string_view>() const { return as_string(); }
/// @endcond

} // namespace json5
