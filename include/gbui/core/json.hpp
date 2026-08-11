// A JSON reader, sized for what this library actually parses: theme files.
//
// It exists rather than a dependency because a theme is a handful of strings
// and numbers, and pulling a general-purpose parser into a UI library makes it
// harder to vendor than the 200 lines it replaces. It is a reader only —
// nothing here writes JSON.
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace gbui::json {

class Value;
using Object = std::map<std::string, Value, std::less<>>;
using Array = std::vector<Value>;

class Value {
public:
    using Storage = std::variant<std::monostate, bool, double, std::string, Array, Object>;

    Value() = default;
    Value(bool v) : storage_(v) {}
    Value(double v) : storage_(v) {}
    Value(std::string v) : storage_(std::move(v)) {}
    Value(Array v) : storage_(std::move(v)) {}
    Value(Object v) : storage_(std::move(v)) {}

    bool isNull() const { return std::holds_alternative<std::monostate>(storage_); }
    bool isObject() const { return std::holds_alternative<Object>(storage_); }
    bool isArray() const { return std::holds_alternative<Array>(storage_); }

    /** Typed accessors that answer nothing when the type does not match, so a
     *  malformed theme surfaces as a named missing field instead of a default. */
    std::optional<bool> asBool() const;
    std::optional<double> asNumber() const;
    const std::string* asString() const;
    const Array* asArray() const;
    const Object* asObject() const;

    /** Member lookup; null when this is not an object or the key is absent. */
    const Value* find(std::string_view key) const;

private:
    Storage storage_;
};

struct ParseError {
    std::string message;
    std::size_t offset = 0;
};

/** Parses a whole document. Trailing content is an error, so a truncated file
 *  cannot half-succeed. */
std::optional<Value> parse(std::string_view text, ParseError* error = nullptr);

/** Reads a file and parses it. */
std::optional<Value> parseFile(const std::string& path, ParseError* error = nullptr);

}  // namespace gbui::json
