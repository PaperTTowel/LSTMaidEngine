#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace lve::io {

  class JsonParser;

  class JsonValue {
  public:
    enum class Type {
      Null,
      Bool,
      Number,
      String,
      Array,
      Object
    };

    using Array = std::vector<JsonValue>;
    using Object = std::unordered_map<std::string, JsonValue>;

    JsonValue() = default;

    Type type() const { return valueType; }
    bool isNull() const { return valueType == Type::Null; }
    bool isBool() const { return valueType == Type::Bool; }
    bool isNumber() const { return valueType == Type::Number; }
    bool isString() const { return valueType == Type::String; }
    bool isArray() const { return valueType == Type::Array; }
    bool isObject() const { return valueType == Type::Object; }

    bool asBool(bool defaultValue = false) const;
    double asNumber(double defaultValue = 0.0) const;
    int asInt(int defaultValue = 0) const;
    const std::string &asString() const;
    std::string asString(const std::string &defaultValue) const;
    const Array *asArray() const;
    const Object *asObject() const;
    const JsonValue *find(const std::string &key) const;

  private:
    Type valueType{Type::Null};
    bool boolValue{false};
    double numberValue{0.0};
    std::string stringValue{};
    Array arrayValue{};
    Object objectValue{};

    friend class JsonParser;
  };

  bool parseJson(const std::string &content, JsonValue &outValue, std::string *outError = nullptr);
  std::string escapeJsonString(const std::string &value);

} // namespace lve::io
