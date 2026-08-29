#pragma once
#include <string>
#include <unordered_map>
#include <variant>
#include <memory>
#include <functional>
#include "ast.h"

// A runtime value in .waa: number, string, or nothing (void)
struct Value {
    enum class Kind { Number, String, Void } kind = Kind::Void;
    double num = 0.0;
    std::string str;

    static Value fromNumber(double n) { Value v; v.kind = Kind::Number; v.num = n; return v; }
    static Value fromString(const std::string& s) { Value v; v.kind = Kind::String; v.str = s; return v; }
    static Value voidVal() { return Value{}; }

    bool truthy() const {
        if (kind == Kind::Number) return num != 0.0;
        if (kind == Kind::String) return !str.empty();
        return false;
    }

    std::string toDisplayString() const {
        if (kind == Kind::Number) {
            if (num == (long long)num) return std::to_string((long long)num);
            return std::to_string(num);
        }
        if (kind == Kind::String) return str;
        return "";
    }
};

using Env = std::unordered_map<std::string, Value>;

using BuiltinFn = std::function<Value(const std::vector<Value>&)>;

class Interpreter {
public:
    Interpreter();

    void run(const NodePtr& program);
    void registerBuiltin(const std::string& name, BuiltinFn fn);

    Env globals;

private:
    std::unordered_map<std::string, BuiltinFn> builtins;

    Value eval(const NodePtr& node, Env& env);
    void exec(const NodePtr& node, Env& env);
    void execBlock(const NodePtr& block, Env& env);
};