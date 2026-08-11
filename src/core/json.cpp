#include "gbui/core/json.hpp"

#include <charconv>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace gbui::json {
namespace {

class Parser {
public:
    Parser(std::string_view text, ParseError* error) : text_(text), error_(error) {}

    std::optional<Value> run() {
        skipSpace();
        auto value = parseValue(0);
        if (!value) return std::nullopt;
        skipSpace();
        if (pos_ != text_.size()) return fail("trailing content after the document");
        return value;
    }

private:
    static constexpr int kMaxDepth = 64;

    std::string_view text_;
    ParseError* error_;
    std::size_t pos_ = 0;

    std::nullopt_t fail(std::string message) {
        if (error_ && error_->message.empty()) {
            error_->message = std::move(message);
            error_->offset = pos_;
        }
        return std::nullopt;
    }

    bool done() const { return pos_ >= text_.size(); }
    char peek() const { return text_[pos_]; }

    void skipSpace() {
        while (!done()) {
            const char c = peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
            else break;
        }
    }

    bool literal(std::string_view word) {
        if (text_.compare(pos_, word.size(), word) != 0) return false;
        pos_ += word.size();
        return true;
    }

    std::optional<Value> parseValue(int depth) {
        // Depth is bounded so a hand-written (or hostile) file cannot recurse
        // the parser into the stack guard.
        if (depth > kMaxDepth) return fail("nesting too deep");
        if (done()) return fail("unexpected end of input");

        switch (peek()) {
            case '{': return parseObject(depth);
            case '[': return parseArray(depth);
            case '"': {
                auto s = parseString();
                if (!s) return std::nullopt;
                return Value(std::move(*s));
            }
            case 't':
                if (literal("true")) return Value(true);
                return fail("expected true");
            case 'f':
                if (literal("false")) return Value(false);
                return fail("expected false");
            case 'n':
                if (literal("null")) return Value();
                return fail("expected null");
            default: return parseNumber();
        }
    }

    std::optional<Value> parseObject(int depth) {
        ++pos_;  // '{'
        Object object;
        skipSpace();
        if (!done() && peek() == '}') { ++pos_; return Value(std::move(object)); }

        while (true) {
            skipSpace();
            if (done() || peek() != '"') return fail("expected a key");
            auto key = parseString();
            if (!key) return std::nullopt;

            skipSpace();
            if (done() || peek() != ':') return fail("expected ':' after the key");
            ++pos_;

            skipSpace();
            auto value = parseValue(depth + 1);
            if (!value) return std::nullopt;
            object.insert_or_assign(std::move(*key), std::move(*value));

            skipSpace();
            if (done()) return fail("unterminated object");
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == '}') { ++pos_; return Value(std::move(object)); }
            return fail("expected ',' or '}'");
        }
    }

    std::optional<Value> parseArray(int depth) {
        ++pos_;  // '['
        Array array;
        skipSpace();
        if (!done() && peek() == ']') { ++pos_; return Value(std::move(array)); }

        while (true) {
            skipSpace();
            auto value = parseValue(depth + 1);
            if (!value) return std::nullopt;
            array.push_back(std::move(*value));

            skipSpace();
            if (done()) return fail("unterminated array");
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == ']') { ++pos_; return Value(std::move(array)); }
            return fail("expected ',' or ']'");
        }
    }

    std::optional<std::string> parseString() {
        ++pos_;  // opening quote
        std::string out;
        while (true) {
            if (done()) { fail("unterminated string"); return std::nullopt; }
            const char c = text_[pos_++];
            if (c == '"') return out;
            if (c != '\\') { out.push_back(c); continue; }

            if (done()) { fail("unterminated escape"); return std::nullopt; }
            const char esc = text_[pos_++];
            switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    auto cp = parseHex4();
                    if (!cp) return std::nullopt;
                    // A surrogate pair is two escapes; join them before encoding,
                    // or a theme name outside the BMP comes out as mojibake.
                    if (*cp >= 0xD800 && *cp <= 0xDBFF && text_.compare(pos_, 2, "\\u") == 0) {
                        const std::size_t save = pos_;
                        pos_ += 2;
                        auto low = parseHex4();
                        if (low && *low >= 0xDC00 && *low <= 0xDFFF) {
                            *cp = 0x10000 + ((*cp - 0xD800) << 10) + (*low - 0xDC00);
                        } else {
                            pos_ = save;
                        }
                    }
                    appendUtf8(out, *cp);
                    break;
                }
                default: fail("unknown escape"); return std::nullopt;
            }
        }
    }

    std::optional<unsigned> parseHex4() {
        if (pos_ + 4 > text_.size()) { fail("truncated \\u escape"); return std::nullopt; }
        unsigned value = 0;
        const char* begin = text_.data() + pos_;
        const auto result = std::from_chars(begin, begin + 4, value, 16);
        if (result.ec != std::errc{} || result.ptr != begin + 4) {
            fail("malformed \\u escape");
            return std::nullopt;
        }
        pos_ += 4;
        return value;
    }

    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    std::optional<Value> parseNumber() {
        const std::size_t start = pos_;
        if (!done() && (peek() == '-' || peek() == '+')) ++pos_;
        while (!done()) {
            const char c = peek();
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') ++pos_;
            else break;
        }
        if (start == pos_) return fail("expected a value");

        // from_chars for double is the one conversion that does not consult the
        // C locale — a comma decimal separator would otherwise silently truncate.
        double value = 0;
        const auto result = std::from_chars(text_.data() + start, text_.data() + pos_, value);
        if (result.ec != std::errc{} || result.ptr != text_.data() + pos_) {
            pos_ = start;
            return fail("malformed number");
        }
        return Value(value);
    }
};

}  // namespace

std::optional<bool> Value::asBool() const {
    if (const auto* v = std::get_if<bool>(&storage_)) return *v;
    return std::nullopt;
}

std::optional<double> Value::asNumber() const {
    if (const auto* v = std::get_if<double>(&storage_)) return *v;
    return std::nullopt;
}

const std::string* Value::asString() const { return std::get_if<std::string>(&storage_); }
const Array* Value::asArray() const { return std::get_if<Array>(&storage_); }
const Object* Value::asObject() const { return std::get_if<Object>(&storage_); }

const Value* Value::find(std::string_view key) const {
    const auto* object = asObject();
    if (!object) return nullptr;
    const auto it = object->find(key);
    return it == object->end() ? nullptr : &it->second;
}

std::optional<Value> parse(std::string_view text, ParseError* error) {
    ParseError local;
    Parser parser(text, error ? error : &local);
    return parser.run();
}

std::optional<Value> parseFile(const std::string& path, ParseError* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) error->message = "cannot open " + path;
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    return parse(text, error);
}

}  // namespace gbui::json
