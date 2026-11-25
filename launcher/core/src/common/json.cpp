#include "lom/launcher/common/json.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace lom::launcher {

namespace {
constexpr int max_depth = 64;

const std::string empty_string;
const Json::Array empty_array;
const Json::Object empty_object;

class Parser {
  public:
    /**
     * @brief Bind the parser to text.
     * @param text Document text; must outlive the parser.
     */
    explicit Parser(const std::string& text) : text_(text) {
    }

    /**
     * @brief Parse the whole document.
     * @param error Receives a description on failure; may be null.
     * @return The value, or nothing on failure.
     */
    std::optional<Json> document(std::string* error) {
        Json value;

        if (!parse_value(value, 0)) {
            report(error);
            return std::nullopt;
        }

        skip_space();

        if (position_ != text_.size()) {
            error_ = "unexpected text after the value";
            report(error);
            return std::nullopt;
        }

        return value;
    }

  private:
    /**
     * @brief Copy the current error and position to the caller.
     * @param error Destination; may be null.
     */
    void report(std::string* error) const {
        if (error != nullptr) {
            *error = error_ + " at byte " + std::to_string(position_);
        }
    }

    /**
     * @brief Skip JSON whitespace.
     */
    void skip_space() {
        while (position_ < text_.size() && (text_[position_] == ' ' || text_[position_] == '\t' || text_[position_] == '\n' || text_[position_] == '\r')) {
            position_++;
        }
    }

    /**
     * @brief Consume an exact literal.
     * @param literal Expected text.
     * @return True when it was present.
     */
    bool consume(const char* literal) {
        std::size_t i = 0;

        while (literal[i] != '\0') {
            if (position_ + i >= text_.size() || text_[position_ + i] != literal[i]) {
                return false;
            }

            i++;
        }

        position_ += i;
        return true;
    }

    /**
     * @brief Parse any value.
     * @param out Receives the value.
     * @param depth Current nesting depth.
     * @return True on success.
     */
    bool parse_value(Json& out, int depth) {
        if (depth > max_depth) {
            error_ = "nesting too deep";
            return false;
        }

        skip_space();

        if (position_ >= text_.size()) {
            error_ = "unexpected end of text";
            return false;
        }

        const char c = text_[position_];

        if (c == '{') {
            return parse_object(out, depth);
        }

        if (c == '[') {
            return parse_array(out, depth);
        }

        if (c == '"') {
            std::string value;

            if (!parse_string(value)) {
                return false;
            }

            out = Json(std::move(value));
            return true;
        }

        if (consume("true")) {
            out = Json(true);
            return true;
        }

        if (consume("false")) {
            out = Json(false);
            return true;
        }

        if (consume("null")) {
            out = Json();
            return true;
        }

        return parse_number(out);
    }

    /**
     * @brief Parse an object.
     * @param out Receives the object.
     * @param depth Current nesting depth.
     * @return True on success.
     */
    bool parse_object(Json& out, int depth) {
        position_++;
        Json::Object members;
        skip_space();

        if (position_ < text_.size() && text_[position_] == '}') {
            position_++;
            out = Json(std::move(members));
            return true;
        }

        while (true) {
            skip_space();
            std::string key;

            if (position_ >= text_.size() || text_[position_] != '"' || !parse_string(key)) {
                if (error_.empty()) {
                    error_ = "expected an object key";
                }

                return false;
            }

            skip_space();

            if (position_ >= text_.size() || text_[position_] != ':') {
                error_ = "expected ':'";
                return false;
            }

            position_++;
            Json value;

            if (!parse_value(value, depth + 1)) {
                return false;
            }

            if (members.count(key) != 0) {
                error_ = "duplicate object key";
                return false;
            }

            members.emplace(std::move(key), std::move(value));
            skip_space();

            if (position_ < text_.size() && text_[position_] == ',') {
                position_++;
                continue;
            }

            if (position_ < text_.size() && text_[position_] == '}') {
                position_++;
                out = Json(std::move(members));
                return true;
            }

            error_ = "expected ',' or '}'";
            return false;
        }
    }

    /**
     * @brief Parse an array.
     * @param out Receives the array.
     * @param depth Current nesting depth.
     * @return True on success.
     */
    bool parse_array(Json& out, int depth) {
        position_++;
        Json::Array elements;
        skip_space();

        if (position_ < text_.size() && text_[position_] == ']') {
            position_++;
            out = Json(std::move(elements));
            return true;
        }

        while (true) {
            Json value;

            if (!parse_value(value, depth + 1)) {
                return false;
            }

            elements.push_back(std::move(value));
            skip_space();

            if (position_ < text_.size() && text_[position_] == ',') {
                position_++;
                continue;
            }

            if (position_ < text_.size() && text_[position_] == ']') {
                position_++;
                out = Json(std::move(elements));
                return true;
            }

            error_ = "expected ',' or ']'";
            return false;
        }
    }

    /**
     * @brief Parse four hexadecimal digits of a \\u escape.
     * @param out Receives the code unit.
     * @return True on success.
     */
    bool parse_hex4(unsigned& out) {
        if (position_ + 4 > text_.size()) {
            return false;
        }

        out = 0;

        for (int i = 0; i < 4; i++) {
            const char c = text_[position_++];
            out <<= 4;

            if (c >= '0' && c <= '9') {
                out |= unsigned(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                out |= unsigned(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                out |= unsigned(c - 'A' + 10);
            } else {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Append a code point as UTF-8.
     * @param out Destination.
     * @param code Unicode scalar value.
     */
    static void append_utf8(std::string& out, unsigned code) {
        if (code < 0x80) {
            out.push_back(char(code));
        } else if (code < 0x800) {
            out.push_back(char(0xC0 | (code >> 6)));
            out.push_back(char(0x80 | (code & 0x3F)));
        } else if (code < 0x10000) {
            out.push_back(char(0xE0 | (code >> 12)));
            out.push_back(char(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(char(0x80 | (code & 0x3F)));
        } else {
            out.push_back(char(0xF0 | (code >> 18)));
            out.push_back(char(0x80 | ((code >> 12) & 0x3F)));
            out.push_back(char(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(char(0x80 | (code & 0x3F)));
        }
    }

    /**
     * @brief Parse a quoted string.
     * @param out Receives the decoded UTF-8 text.
     * @return True on success.
     */
    bool parse_string(std::string& out) {
        position_++;

        while (position_ < text_.size()) {
            const char c = text_[position_++];

            if (c == '"') {
                return true;
            }

            if (static_cast<unsigned char>(c) < 0x20) {
                error_ = "control character in string";
                return false;
            }

            if (c != '\\') {
                out.push_back(c);
                continue;
            }

            if (position_ >= text_.size()) {
                break;
            }

            const char escape = text_[position_++];

            switch (escape) {
            case '"':
            case '\\':
            case '/':
                out.push_back(escape);
                break;

            case 'b':
                out.push_back('\b');
                break;

            case 'f':
                out.push_back('\f');
                break;

            case 'n':
                out.push_back('\n');
                break;

            case 'r':
                out.push_back('\r');
                break;

            case 't':
                out.push_back('\t');
                break;

            case 'u': {
                unsigned code;

                if (!parse_hex4(code)) {
                    error_ = "invalid \\u escape";
                    return false;
                }

                if (code >= 0xD800 && code <= 0xDBFF) {
                    unsigned low;

                    if (!consume("\\u") || !parse_hex4(low) || low < 0xDC00 || low > 0xDFFF) {
                        error_ = "invalid surrogate pair";
                        return false;
                    }

                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                } else if (code >= 0xDC00 && code <= 0xDFFF) {
                    error_ = "unpaired surrogate";
                    return false;
                }

                append_utf8(out, code);
                break;
            }

            default:
                error_ = "invalid escape";
                return false;
            }
        }

        error_ = "unterminated string";
        return false;
    }

    /**
     * @brief Parse a number; integers without fraction or exponent stay exact.
     * @param out Receives the number.
     * @return True on success.
     */
    bool parse_number(Json& out) {
        const std::size_t start = position_;
        bool integral = true;

        if (position_ < text_.size() && text_[position_] == '-') {
            position_++;
        }

        const std::size_t digits_start = position_;

        while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
            position_++;
        }

        if (position_ == digits_start) {
            error_ = "invalid value";
            return false;
        }

        if (text_[digits_start] == '0' && position_ - digits_start > 1) {
            error_ = "leading zero";
            return false;
        }

        if (position_ < text_.size() && text_[position_] == '.') {
            integral = false;
            position_++;

            const std::size_t fraction_start = position_;

            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
                position_++;
            }

            if (position_ == fraction_start) {
                error_ = "invalid fraction";
                return false;
            }
        }

        if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
            integral = false;
            position_++;

            if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) {
                position_++;
            }

            const std::size_t exponent_start = position_;

            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
                position_++;
            }

            if (position_ == exponent_start) {
                error_ = "invalid exponent";
                return false;
            }
        }

        const std::string number = text_.substr(start, position_ - start);

        if (integral) {
            errno = 0;
            char* end = nullptr;
            const long long value = std::strtoll(number.c_str(), &end, 10);

            if (errno == 0 && end != nullptr && *end == '\0') {
                out = Json(std::int64_t(value));
                return true;
            }
        }

        out = Json(std::strtod(number.c_str(), nullptr));
        return true;
    }

    const std::string& text_;
    std::size_t position_ = 0;
    std::string error_;
};

/**
 * @brief Append a quoted, escaped JSON string.
 * @param out Destination.
 * @param value UTF-8 text.
 */
void dump_string(std::string& out, const std::string& value) {
    out.push_back('"');

    for (const char c : value) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;

        case '\\':
            out += "\\\\";
            break;

        case '\n':
            out += "\\n";
            break;

        case '\r':
            out += "\\r";
            break;

        case '\t':
            out += "\\t";
            break;

        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char escape[8];
                std::snprintf(escape, sizeof(escape), "\\u%04x", unsigned(static_cast<unsigned char>(c)));
                out += escape;
            } else {
                out.push_back(c);
            }
        }
    }

    out.push_back('"');
}

/**
 * @brief Append a value's serialization.
 * @param out Destination.
 * @param value Value to serialize.
 * @param pretty Whether to indent.
 * @param indent Current indentation in spaces.
 */
void dump_value(std::string& out, const Json& value, bool pretty, int indent) {
    const std::string newline = pretty ? "\n" : "";
    const std::string inner(pretty ? std::size_t(indent + 2) : 0, ' ');
    const std::string outer(pretty ? std::size_t(indent) : 0, ' ');

    switch (value.type()) {
    case Json::Type::Null:
        out += "null";
        break;

    case Json::Type::Bool:
        out += value.as_bool(false) ? "true" : "false";
        break;

    case Json::Type::Integer:
        out += std::to_string(value.as_int(0));
        break;

    case Json::Type::Real: {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.17g", value.as_real(0.0));
        out += buffer;
        break;
    }

    case Json::Type::String:
        dump_string(out, value.as_string());
        break;

    case Json::Type::Array: {
        const auto& elements = value.as_array();

        if (elements.empty()) {
            out += "[]";
            break;
        }

        out += "[" + newline;

        for (std::size_t i = 0; i < elements.size(); i++) {
            out += inner;
            dump_value(out, elements[i], pretty, indent + 2);
            out += (i + 1 < elements.size() ? "," : "") + newline;
        }

        out += outer + "]";
        break;
    }

    case Json::Type::Object: {
        const auto& members = value.as_object();

        if (members.empty()) {
            out += "{}";
            break;
        }

        out += "{" + newline;
        std::size_t i = 0;

        for (const auto& [key, member] : members) {
            out += inner;
            dump_string(out, key);
            out += pretty ? ": " : ":";
            dump_value(out, member, pretty, indent + 2);
            out += (++i < members.size() ? "," : "") + newline;
        }

        out += outer + "}";
        break;
    }
    }
}

}

Json::Json() = default;

Json::Json(bool value) : type_(Type::Bool), bool_(value) {
}

Json::Json(std::int64_t value) : type_(Type::Integer), integer_(value) {
}

Json::Json(int value) : type_(Type::Integer), integer_(value) {
}

Json::Json(double value) : type_(Type::Real), real_(value) {
}

Json::Json(std::string value) : type_(Type::String), string_(std::move(value)) {
}

Json::Json(const char* value) : type_(Type::String), string_(value) {
}

Json::Json(Array value) : type_(Type::Array), array_(std::move(value)) {
}

Json::Json(Object value) : type_(Type::Object), object_(std::move(value)) {
}

std::optional<Json> Json::parse(const std::string& text, std::string* error) {
    Parser parser(text);
    return parser.document(error);
}

std::string Json::dump(bool pretty) const {
    std::string out;
    dump_value(out, *this, pretty, 0);

    if (pretty) {
        out.push_back('\n');
    }

    return out;
}

Json::Type Json::type() const {
    return type_;
}

bool Json::as_bool(bool fallback) const {
    return type_ == Type::Bool ? bool_ : fallback;
}

std::int64_t Json::as_int(std::int64_t fallback) const {
    return type_ == Type::Integer ? integer_ : fallback;
}

double Json::as_real(double fallback) const {
    if (type_ == Type::Real) {
        return real_;
    }

    return type_ == Type::Integer ? double(integer_) : fallback;
}

const std::string& Json::as_string() const {
    return type_ == Type::String ? string_ : empty_string;
}

const Json::Array& Json::as_array() const {
    return type_ == Type::Array ? array_ : empty_array;
}

const Json::Object& Json::as_object() const {
    return type_ == Type::Object ? object_ : empty_object;
}

const Json* Json::find(const std::string& key) const {
    if (type_ != Type::Object) {
        return nullptr;
    }

    const auto found = object_.find(key);
    return found == object_.end() ? nullptr : &found->second;
}

Json& Json::operator[](const std::string& key) {
    if (type_ != Type::Object) {
        *this = Json(Object{});
    }

    return object_[key];
}

void Json::push_back(Json value) {
    if (type_ != Type::Array) {
        *this = Json(Array{});
    }

    array_.push_back(std::move(value));
}

}
