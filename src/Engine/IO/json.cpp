#include "Engine/IO/json.hpp"

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace lve::io {

  namespace {
    const std::string kEmptyString{};

    void appendUtf8(std::string &out, unsigned int codepoint) {
      if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
      } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
      } else if (codepoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
      } else {
        out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
      }
    }

    int hexValue(char ch) {
      if (ch >= '0' && ch <= '9') return ch - '0';
      if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
      if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
      return -1;
    }
  } // namespace

  class JsonParser {
    public:
      explicit JsonParser(const std::string &src) : source{src} {}

      bool parse(JsonValue &outValue) {
        skipWhitespace();
        if (!parseValue(outValue)) {
          return false;
        }
        skipWhitespace();
        if (position != source.size()) {
          setError("Unexpected trailing characters");
          return false;
        }
        return true;
      }

      const std::string &getError() const { return error; }

    private:
      bool parseValue(JsonValue &outValue) {
        skipWhitespace();
        if (position >= source.size()) {
          setError("Unexpected end of JSON");
          return false;
        }

        const char ch = source[position];
        if (ch == '{') return parseObject(outValue);
        if (ch == '[') return parseArray(outValue);
        if (ch == '"') return parseStringValue(outValue);
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) return parseNumber(outValue);
        if (matchLiteral("true")) {
          outValue.valueType = JsonValue::Type::Bool;
          outValue.boolValue = true;
          return true;
        }
        if (matchLiteral("false")) {
          outValue.valueType = JsonValue::Type::Bool;
          outValue.boolValue = false;
          return true;
        }
        if (matchLiteral("null")) {
          outValue = JsonValue{};
          return true;
        }

        setError("Unexpected token");
        return false;
      }

      bool parseObject(JsonValue &outValue) {
        ++position;
        outValue.valueType = JsonValue::Type::Object;
        outValue.objectValue.clear();

        skipWhitespace();
        if (consume('}')) {
          return true;
        }

        while (position < source.size()) {
          std::string key;
          if (!parseString(key)) {
            return false;
          }
          skipWhitespace();
          if (!consume(':')) {
            setError("Expected ':' after object key");
            return false;
          }

          JsonValue value;
          if (!parseValue(value)) {
            return false;
          }
          outValue.objectValue[std::move(key)] = std::move(value);

          skipWhitespace();
          if (consume('}')) {
            return true;
          }
          if (!consume(',')) {
            setError("Expected ',' or '}' in object");
            return false;
          }
          skipWhitespace();
        }

        setError("Unterminated object");
        return false;
      }

      bool parseArray(JsonValue &outValue) {
        ++position;
        outValue.valueType = JsonValue::Type::Array;
        outValue.arrayValue.clear();

        skipWhitespace();
        if (consume(']')) {
          return true;
        }

        while (position < source.size()) {
          JsonValue value;
          if (!parseValue(value)) {
            return false;
          }
          outValue.arrayValue.push_back(std::move(value));

          skipWhitespace();
          if (consume(']')) {
            return true;
          }
          if (!consume(',')) {
            setError("Expected ',' or ']' in array");
            return false;
          }
          skipWhitespace();
        }

        setError("Unterminated array");
        return false;
      }

      bool parseStringValue(JsonValue &outValue) {
        std::string value;
        if (!parseString(value)) {
          return false;
        }
        outValue.valueType = JsonValue::Type::String;
        outValue.stringValue = std::move(value);
        return true;
      }

      bool parseString(std::string &outString) {
        if (!consume('"')) {
          setError("Expected string");
          return false;
        }

        outString.clear();
        while (position < source.size()) {
          char ch = source[position++];
          if (ch == '"') {
            return true;
          }
          if (static_cast<unsigned char>(ch) < 0x20) {
            setError("Control character in string");
            return false;
          }
          if (ch != '\\') {
            outString.push_back(ch);
            continue;
          }

          if (position >= source.size()) {
            setError("Unterminated escape sequence");
            return false;
          }
          const char escaped = source[position++];
          switch (escaped) {
            case '"': outString.push_back('"'); break;
            case '\\': outString.push_back('\\'); break;
            case '/': outString.push_back('/'); break;
            case 'b': outString.push_back('\b'); break;
            case 'f': outString.push_back('\f'); break;
            case 'n': outString.push_back('\n'); break;
            case 'r': outString.push_back('\r'); break;
            case 't': outString.push_back('\t'); break;
            case 'u': {
              if (position + 4 > source.size()) {
                setError("Incomplete unicode escape");
                return false;
              }
              unsigned int codepoint = 0;
              for (int i = 0; i < 4; ++i) {
                const int value = hexValue(source[position++]);
                if (value < 0) {
                  setError("Invalid unicode escape");
                  return false;
                }
                codepoint = (codepoint << 4) | static_cast<unsigned int>(value);
              }
              appendUtf8(outString, codepoint);
              break;
            }
            default:
              setError("Invalid escape sequence");
              return false;
          }
        }

        setError("Unterminated string");
        return false;
      }

      bool parseNumber(JsonValue &outValue) {
        const std::size_t start = position;
        if (source[position] == '-') {
          ++position;
        }
        if (position >= source.size()) {
          setError("Invalid number");
          return false;
        }
        if (source[position] == '0') {
          ++position;
        } else if (std::isdigit(static_cast<unsigned char>(source[position]))) {
          while (position < source.size() && std::isdigit(static_cast<unsigned char>(source[position]))) {
            ++position;
          }
        } else {
          setError("Invalid number");
          return false;
        }

        if (position < source.size() && source[position] == '.') {
          ++position;
          if (position >= source.size() || !std::isdigit(static_cast<unsigned char>(source[position]))) {
            setError("Invalid fractional number");
            return false;
          }
          while (position < source.size() && std::isdigit(static_cast<unsigned char>(source[position]))) {
            ++position;
          }
        }

        if (position < source.size() && (source[position] == 'e' || source[position] == 'E')) {
          ++position;
          if (position < source.size() && (source[position] == '+' || source[position] == '-')) {
            ++position;
          }
          if (position >= source.size() || !std::isdigit(static_cast<unsigned char>(source[position]))) {
            setError("Invalid number exponent");
            return false;
          }
          while (position < source.size() && std::isdigit(static_cast<unsigned char>(source[position]))) {
            ++position;
          }
        }

        char *end = nullptr;
        const std::string numberText = source.substr(start, position - start);
        const double number = std::strtod(numberText.c_str(), &end);
        if (!end || *end != '\0') {
          setError("Invalid number");
          return false;
        }

        outValue.valueType = JsonValue::Type::Number;
        outValue.numberValue = number;
        return true;
      }

      bool matchLiteral(const char *literal) {
        const std::size_t start = position;
        for (const char *it = literal; *it; ++it) {
          if (position >= source.size() || source[position] != *it) {
            position = start;
            return false;
          }
          ++position;
        }
        return true;
      }

      bool consume(char expected) {
        if (position < source.size() && source[position] == expected) {
          ++position;
          return true;
        }
        return false;
      }

      void skipWhitespace() {
        while (position < source.size() && std::isspace(static_cast<unsigned char>(source[position]))) {
          ++position;
        }
      }

      void setError(const std::string &message) {
        if (!error.empty()) {
          return;
        }
        std::ostringstream ss;
        ss << message << " at byte " << position;
        error = ss.str();
      }

      const std::string &source;
      std::size_t position{0};
      std::string error{};
    };

  bool JsonValue::asBool(bool defaultValue) const {
    return valueType == Type::Bool ? boolValue : defaultValue;
  }

  double JsonValue::asNumber(double defaultValue) const {
    return valueType == Type::Number ? numberValue : defaultValue;
  }

  int JsonValue::asInt(int defaultValue) const {
    return valueType == Type::Number ? static_cast<int>(numberValue) : defaultValue;
  }

  const std::string &JsonValue::asString() const {
    return valueType == Type::String ? stringValue : kEmptyString;
  }

  std::string JsonValue::asString(const std::string &defaultValue) const {
    return valueType == Type::String ? stringValue : defaultValue;
  }

  const JsonValue::Array *JsonValue::asArray() const {
    return valueType == Type::Array ? &arrayValue : nullptr;
  }

  const JsonValue::Object *JsonValue::asObject() const {
    return valueType == Type::Object ? &objectValue : nullptr;
  }

  const JsonValue *JsonValue::find(const std::string &key) const {
    if (valueType != Type::Object) {
      return nullptr;
    }
    auto it = objectValue.find(key);
    return it == objectValue.end() ? nullptr : &it->second;
  }

  bool parseJson(const std::string &content, JsonValue &outValue, std::string *outError) {
    JsonParser parser{content};
    if (!parser.parse(outValue)) {
      if (outError) {
        *outError = parser.getError();
      }
      return false;
    }
    return true;
  }

  std::string escapeJsonString(const std::string &value) {
    std::string out;
    out.reserve(value.size());
    for (char ch : value) {
      switch (ch) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
          if (static_cast<unsigned char>(ch) < 0x20) {
            const char *digits = "0123456789abcdef";
            out += "\\u00";
            out.push_back(digits[(static_cast<unsigned char>(ch) >> 4) & 0x0F]);
            out.push_back(digits[static_cast<unsigned char>(ch) & 0x0F]);
          } else {
            out.push_back(ch);
          }
          break;
      }
    }
    return out;
  }

} // namespace lve::io
