#ifndef LOM_LAUNCHER_JSON_H
#define LOM_LAUNCHER_JSON_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace lom::launcher {
class Json {
  public:
    enum class Type { Null, Bool, Integer, Real, String, Array, Object };

    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;

    /**
     * @brief Create a null value.
     */
    Json();

    /**
     * @brief Create a boolean value.
     * @param value Boolean.
     */
    Json(bool value);

    /**
     * @brief Create an integer value.
     * @param value Integer.
     */
    Json(std::int64_t value);

    /**
     * @brief Create an integer value from a plain int.
     * @param value Integer.
     */
    Json(int value);

    /**
     * @brief Create a real value.
     * @param value Finite number.
     */
    Json(double value);

    /**
     * @brief Create a string value.
     * @param value UTF-8 text.
     */
    Json(std::string value);

    /**
     * @brief Create a string value from a C string.
     * @param value UTF-8 text; must not be null.
     */
    Json(const char* value);

    /**
     * @brief Create an array value.
     * @param value Elements.
     */
    Json(Array value);

    /**
     * @brief Create an object value.
     * @param value Members.
     */
    Json(Object value);

    /**
     * @brief Parse UTF-8 JSON text.
     * @param text Complete document.
     * @param error Receives a short description on failure; may be null.
     * @return The parsed value, or nothing if the text is not valid JSON.
     */
    static std::optional<Json> parse(const std::string& text, std::string* error);

    /**
     * @brief Serialize the value.
     * @param pretty Whether to indent with two spaces and end with a newline.
     * @return JSON text.
     */
    std::string dump(bool pretty) const;

    /**
     * @brief Report the value kind.
     * @return Kind of this value.
     */
    Type type() const;

    /**
     * @brief Read a boolean.
     * @param fallback Returned when the value is not a boolean.
     * @return The boolean or fallback.
     */
    bool as_bool(bool fallback) const;

    /**
     * @brief Read an integer.
     * @param fallback Returned when the value is not an integer.
     * @return The integer or fallback.
     */
    std::int64_t as_int(std::int64_t fallback) const;

    /**
     * @brief Read a number as a double.
     * @param fallback Returned when the value is not a number.
     * @return The number or fallback.
     */
    double as_real(double fallback) const;

    /**
     * @brief Read a string.
     * @return The string, or empty when the value is not a string.
     */
    const std::string& as_string() const;

    /**
     * @brief Read array elements.
     * @return The elements, or an empty array when the value is not an array.
     */
    const Array& as_array() const;

    /**
     * @brief Read object members.
     * @return The members, or an empty object when the value is not an object.
     */
    const Object& as_object() const;

    /**
     * @brief Find an object member.
     * @param key Member name.
     * @return The member, or null when absent or when this is not an object.
     */
    const Json* find(const std::string& key) const;

    /**
     * @brief Access or create an object member, converting a null value into an object.
     * @param key Member name.
     * @return The member.
     */
    Json& operator[](const std::string& key);

    /**
     * @brief Append an array element, converting a null value into an array.
     * @param value Element to append.
     */
    void push_back(Json value);

  private:
    Type type_ = Type::Null;
    bool bool_ = false;
    std::int64_t integer_ = 0;
    double real_ = 0.0;
    std::string string_;
    Array array_;
    Object object_;
};

}

#endif
