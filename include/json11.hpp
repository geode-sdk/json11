/* json11
 *
 * json11 is a tiny JSON library for C++11, providing JSON parsing and serialization.
 *
 * The core object provided by the library is json11::Json. A Json object represents any JSON
 * value: null, bool, number (int or double), string (std::string), array (std::vector), or
 * object (std::map).
 *
 * Json objects act like values: they can be assigned, copied, moved, compared for equality or
 * order, etc. There are also helper methods Json::dump, to serialize a Json to a string, and
 * Json::parse (static) to parse a std::string as a Json object.
 *
 * Internally, the various types of Json object are represented by the JsonValue class
 * hierarchy.
 *
 * A note on numbers - JSON specifies the syntax of number formatting but not its semantics,
 * so some JSON implementations distinguish between integers and floating-point numbers, while
 * some don't. In json11, we choose the latter. Because some JSON implementations (namely
 * Javascript itself) treat all numbers as the same type, distinguishing the two leads
 * to JSON that will be *silently* changed by a round-trip through those implementations.
 * Dangerous! To avoid that risk, json11 stores all numbers as double internally, but also
 * provides integer helpers.
 *
 * Fortunately, double-precision IEEE754 ('double') can precisely store any integer in the
 * range +/-2^53, which includes every 'int' on most systems. (Timestamps often use int64
 * or long long to avoid the Y2038K problem; a double storing microseconds since some epoch
 * will be exact for +/- 275 years.)
 */

/* Copyright (c) 2013 Dropbox, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <initializer_list>
#include <stdexcept>

#ifdef _MSC_VER
    #if _MSC_VER <= 1800 // VS 2013
        #ifndef noexcept
            #define noexcept throw()
        #endif

        #ifndef snprintf
            #define snprintf _snprintf_s
        #endif
    #endif
#endif

namespace json11 {

enum JsonParse {
    STANDARD, COMMENTS
};

class JsonException : public std::runtime_error {
public:
    template <class T>
    JsonException(T&& value) : std::runtime_error(std::forward<T>(value)) {}
};

class JsonValue;

class Json final {
public:
    // Types
    enum Type {
        NUL, NUMBER, BOOL, STRING, ARRAY, OBJECT
    };

    using array = std::vector<Json>;

    class object final {
        using value_type = std::pair<std::string, Json>;
        using iterator = typename std::vector<value_type>::iterator;
        using const_iterator = typename std::vector<value_type>::const_iterator;
        std::vector<value_type> m_data;
    public:
        object() = default;
        object(const object&);
        object(object&&);
        template <class It>
        object(It first, It last) : m_data(first, last) {}
        object(std::initializer_list<value_type> init);

        size_t size() const { return m_data.size(); }
        bool empty() const { return m_data.empty(); }

        Json& operator[](const std::string& key);
        iterator begin();
        iterator end();
        const_iterator begin() const;
        const_iterator cbegin() const;
        const_iterator end() const;
        const_iterator cend() const;

        iterator find(const std::string& key);
        const_iterator find(const std::string& key) const;

        std::pair<iterator, bool> insert(const value_type& value);
        size_t count(const std::string& key) const;

        bool operator==(const object& other) const;
        bool operator<(const object& other) const;
    };

    // Constructors for the various types of JSON value.
    Json() noexcept;                // NUL
    Json(std::nullptr_t) noexcept;  // NUL
    Json(double value);             // NUMBER
    Json(int value);                // NUMBER
    Json(bool value);               // BOOL
    Json(const std::string &value); // STRING
    Json(std::string &&value);      // STRING
    Json(const char * value);       // STRING
    Json(const array &values);      // ARRAY
    Json(array &&values);           // ARRAY
    Json(const object &values);     // OBJECT
    Json(object &&values);          // OBJECT

    template <typename T> requires std::is_integral_v<T>
    Json(const T& value) : Json(static_cast<double>(value)) {}

    template <class T>
    requires requires(const T& value) { to_json(value); }
    Json(const T& value) : Json(to_json(value)) {}

    // Implicit constructor: anything with a to_json() function.
    template <class T, class = decltype(&T::to_json)>
    Json(const T & t) : Json(t.to_json()) {}

    // Implicit constructor: map-like objects (std::map, std::unordered_map, etc)
    template <class M, typename std::enable_if<
        std::is_constructible<std::string, decltype(std::declval<M>().begin()->first)>::value
        && std::is_constructible<Json, decltype(std::declval<M>().begin()->second)>::value,
            int>::type = 0>
    Json(const M & m) : Json(object(m.begin(), m.end())) {}

    // Implicit constructor: vector-like objects (std::list, std::vector, std::set, etc)
    template <class V, typename std::enable_if<
        std::is_constructible<Json, decltype(*std::declval<V>().begin())>::value,
            int>::type = 0>
    Json(const V & v) : Json(array(v.begin(), v.end())) {}

    // This prevents Json(some_pointer) from accidentally producing a bool. Use
    // Json(bool(some_pointer)) if that behavior is desired.
    Json(void *) = delete;

    // Accessors
    Type type() const;

    bool is_null()   const { return type() == NUL; }
    bool is_number() const { return type() == NUMBER; }
    bool is_bool()   const { return type() == BOOL; }
    bool is_string() const { return type() == STRING; }
    bool is_array()  const { return type() == ARRAY; }
    bool is_object() const { return type() == OBJECT; }

    // Return the enclosed value if this is a number, 0 otherwise. Note that json11 does not
    // distinguish between integer and non-integer numbers - number_value() and int_value()
    // can both be applied to a NUMBER-typed object.
    double number_value() const;
    int int_value() const;

    // Return the enclosed value if this is a boolean, false otherwise.
    bool bool_value() const;
    // Return the enclosed string if this is a string, "" otherwise.
    const std::string &string_value() const;
    // Return the enclosed std::vector if this is an array, or an empty vector otherwise.
    const array &array_items() const;
    array& array_items();
    // Return the enclosed std::map if this is an object, or an empty map otherwise.
    const object &object_items() const;
    object& object_items();

    // Return a reference to arr[i] if this is an array, Json() otherwise.
    const Json& operator[](size_t i) const;
    Json& operator[](size_t i);
    // Return a reference to obj[key] if this is an object, Json() otherwise.
    const Json& operator[](const std::string &key) const;
    Json& operator[](const std::string& key);

    template <class T>
    decltype(auto) as() const {
        if constexpr (std::is_same_v<T, bool>) {
            return this->bool_value();
        } else if constexpr (std::is_integral_v<T>) {
            return this->int_value();
        } else if constexpr (std::is_floating_point_v<T>) {
            return this->number_value();
        } else if constexpr (std::is_constructible_v<std::string, T>) {
            return this->string_value();
        } else if constexpr (std::is_same_v<T, array>) {
            return this->array_items();
        } else if constexpr (std::is_same_v<T, object>) {
            return this->object_items();
        } else if constexpr (std::is_default_constructible_v<T> && requires(const Json& json, T& value) { from_json(json, value); }) {
            T value;
            from_json(*this, value);
            return value;
        }
    }

    template <class T>
    decltype(auto) as() {
        if constexpr (std::is_same_v<T, array>) {
            return this->array_items();
        } else if constexpr (std::is_same_v<T, object>) {
            return this->object_items();
        } else {
            return static_cast<const Json*>(this)->as<T>();
        }
    }

    template <class T>
    bool is() const {
        switch (type()) {
            case Json::Type::ARRAY: return std::is_same_v<T, array>;
            case Json::Type::OBJECT: return std::is_same_v<T, object>;
            case Json::Type::STRING: return std::is_constructible_v<std::string, T>;
            case Json::Type::NUMBER: return std::is_integral_v<T> || std::is_floating_point_v<T>;
            case Json::Type::BOOL: return std::is_same_v<T, bool>;
            case Json::Type::NUL: return false;
        }
    }

    template <class T, class Key>
    decltype(auto) get(Key&& key_or_index) const {
        const auto value = this->operator[](std::forward<Key>(key_or_index));
        return value.template as<T>();
    }

    template <class T, class Key>
    decltype(auto) get(Key&& key_or_index) {
        const auto value = this->operator[](std::forward<Key>(key_or_index));
        return value.template as<T>();
    }

    // Serialize.
    void dump(std::string &out) const;
    std::string dump() const {
        std::string out;
        dump(out);
        return out;
    }

    // Parse. If parse fails, return Json() and assign an error message to err.
    static Json parse(const std::string & in,
                      std::string & err,
                      JsonParse strategy = JsonParse::STANDARD);

    // Parse. If parse fails, throw an exception
    static Json try_parse(const std::string & in, JsonParse strategy = JsonParse::STANDARD);

    // Parse multiple objects, concatenated or separated by whitespace
    static std::vector<Json> parse_multi(
        const std::string & in,
        std::string::size_type & parser_stop_pos,
        std::string & err,
        JsonParse strategy = JsonParse::STANDARD);

    static inline std::vector<Json> parse_multi(
        const std::string & in,
        std::string & err,
        JsonParse strategy = JsonParse::STANDARD) {
        std::string::size_type parser_stop_pos;
        return parse_multi(in, parser_stop_pos, err, strategy);
    }

    bool operator== (const Json &rhs) const;
    bool operator<  (const Json &rhs) const;
    bool operator!= (const Json &rhs) const { return !(*this == rhs); }
    bool operator<= (const Json &rhs) const { return !(rhs < *this); }
    bool operator>  (const Json &rhs) const { return  (rhs < *this); }
    bool operator>= (const Json &rhs) const { return !(*this < rhs); }

    /* has_shape(types, err)
     *
     * Return true if this is a JSON object and, for each item in types, has a field of
     * the given type. If not, return false and set err to a descriptive message.
     */
    typedef std::initializer_list<std::pair<std::string, Type>> shape;
    bool has_shape(const shape & types, std::string & err) const;

private:
    std::shared_ptr<JsonValue> m_ptr;
};

// Internal class hierarchy - JsonValue objects are not exposed to users of this API.
class JsonValue {
protected:
    friend class Json;
    friend class JsonInt;
    friend class JsonDouble;
    virtual Json::Type type() const = 0;
    virtual bool equals(const JsonValue * other) const = 0;
    virtual bool less(const JsonValue * other) const = 0;
    virtual void dump(std::string &out) const = 0;
    virtual double number_value() const;
    virtual int int_value() const;
    virtual bool bool_value() const;
    virtual const std::string &string_value() const;
    virtual const Json::array &array_items() const;
    virtual Json::array &array_items();
    virtual const Json &operator[](size_t i) const;
    virtual Json &operator[](size_t i);
    virtual const Json::object &object_items() const;
    virtual Json::object &object_items();
    virtual const Json &operator[](const std::string &key) const;
    virtual Json &operator[](const std::string &key);
    virtual ~JsonValue() {}
};

} // namespace json11
