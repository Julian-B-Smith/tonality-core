// Minimal recursive-descent JSON parser — test scaffolding only (reads the
// vendored conformance fixture). Not part of the core; the core never parses
// JSON, it only emits it.
#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace mini_json {

struct Value;
using ValuePtr = std::shared_ptr<Value>;

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    bool number_is_int = false;
    long long integer = 0;
    std::string string;
    std::vector<ValuePtr> array;
    std::map<std::string, ValuePtr> object;

    bool is_null() const { return type == Type::Null; }
    const Value& at(const std::string& key) const {
        auto it = object.find(key);
        if (it == object.end()) throw std::runtime_error("missing key: " + key);
        return *it->second;
    }
    bool has(const std::string& key) const { return object.count(key) != 0; }
};

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    ValuePtr parse() {
        ValuePtr value = parse_value();
        skip_ws();
        if (pos_ != text_.size()) throw std::runtime_error("trailing content");
        return value;
    }

private:
    const std::string& text_;
    std::size_t pos_ = 0;

    void skip_ws() {
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
            else break;
        }
    }

    char peek() {
        skip_ws();
        if (pos_ >= text_.size()) throw std::runtime_error("unexpected end");
        return text_[pos_];
    }

    void expect(char c) {
        if (peek() != c) throw std::runtime_error(std::string("expected '") + c + "'");
        ++pos_;
    }

    ValuePtr parse_value() {
        const char c = peek();
        switch (c) {
            case '{': return parse_object();
            case '[': return parse_array();
            case '"': return parse_string();
            case 't': case 'f': return parse_bool();
            case 'n': return parse_null();
            default: return parse_number();
        }
    }

    ValuePtr parse_object() {
        auto value = std::make_shared<Value>();
        value->type = Value::Type::Object;
        expect('{');
        if (peek() == '}') { ++pos_; return value; }
        while (true) {
            ValuePtr key = parse_string();
            expect(':');
            value->object[key->string] = parse_value();
            const char c = peek();
            if (c == ',') { ++pos_; continue; }
            if (c == '}') { ++pos_; break; }
            throw std::runtime_error("bad object");
        }
        return value;
    }

    ValuePtr parse_array() {
        auto value = std::make_shared<Value>();
        value->type = Value::Type::Array;
        expect('[');
        if (peek() == ']') { ++pos_; return value; }
        while (true) {
            value->array.push_back(parse_value());
            const char c = peek();
            if (c == ',') { ++pos_; continue; }
            if (c == ']') { ++pos_; break; }
            throw std::runtime_error("bad array");
        }
        return value;
    }

    ValuePtr parse_string() {
        auto value = std::make_shared<Value>();
        value->type = Value::Type::String;
        expect('"');
        while (pos_ < text_.size()) {
            char c = text_[pos_++];
            if (c == '"') return value;
            if (c == '\\') {
                if (pos_ >= text_.size()) break;
                const char esc = text_[pos_++];
                switch (esc) {
                    case '"': value->string += '"'; break;
                    case '\\': value->string += '\\'; break;
                    case '/': value->string += '/'; break;
                    case 'b': value->string += '\b'; break;
                    case 'f': value->string += '\f'; break;
                    case 'n': value->string += '\n'; break;
                    case 'r': value->string += '\r'; break;
                    case 't': value->string += '\t'; break;
                    case 'u': {  // keep raw — fixture keys/values we read are ASCII
                        value->string += "\\u";
                        for (int i = 0; i < 4 && pos_ < text_.size(); ++i)
                            value->string += text_[pos_++];
                        break;
                    }
                    default: throw std::runtime_error("bad escape");
                }
            } else {
                value->string += c;
            }
        }
        throw std::runtime_error("unterminated string");
    }

    ValuePtr parse_bool() {
        auto value = std::make_shared<Value>();
        value->type = Value::Type::Bool;
        if (text_.compare(pos_, 4, "true") == 0) { value->boolean = true; pos_ += 4; }
        else if (text_.compare(pos_, 5, "false") == 0) { value->boolean = false; pos_ += 5; }
        else throw std::runtime_error("bad literal");
        return value;
    }

    ValuePtr parse_null() {
        if (text_.compare(pos_, 4, "null") != 0) throw std::runtime_error("bad literal");
        pos_ += 4;
        return std::make_shared<Value>();
    }

    ValuePtr parse_number() {
        const std::size_t start = pos_;
        bool is_int = true;
        if (pos_ < text_.size() && text_[pos_] == '-') ++pos_;
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c >= '0' && c <= '9') { ++pos_; continue; }
            if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
                if (c == '.' || c == 'e' || c == 'E') is_int = false;
                ++pos_;
                continue;
            }
            break;
        }
        auto value = std::make_shared<Value>();
        value->type = Value::Type::Number;
        const std::string token = text_.substr(start, pos_ - start);
        value->number = std::stod(token);
        value->number_is_int = is_int;
        if (is_int) value->integer = std::stoll(token);
        return value;
    }
};

inline ValuePtr parse(const std::string& text) { return Parser(text).parse(); }

}  // namespace mini_json
