#include "interpreter.h"
#include "shell.h"
#include <iostream>
#include <stdexcept>
#include <cmath>

Interpreter::Interpreter() {
    registerBuiltin("shell", [](const std::vector<Value>& args) -> Value {
        if (!args.empty()) {
            runShellLine(args[0].toDisplayString());
        }
        return Value::voidVal();
    });
    registerBuiltin("print", [](const std::vector<Value>& args) -> Value {
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].toDisplayString();
        }
        std::cout << "\n";
        return Value::voidVal();
    });
}

void Interpreter::registerBuiltin(const std::string& name, BuiltinFn fn) {
    builtins[name] = std::move(fn);
}

void Interpreter::run(const NodePtr& program) {
    for (const auto& stmt : program->statements) {
        exec(stmt, globals);
    }
}

void Interpreter::execBlock(const NodePtr& block, Env& env) {
    for (const auto& stmt : block->statements) {
        exec(stmt, env);
    }
}

void Interpreter::exec(const NodePtr& node, Env& env) {
    switch (node->type) {
        case NodeType::Assign: {
            Value v = eval(node->value, env);
            env[node->target] = v;
            return;
        }
        case NodeType::If: {
            Value cond = eval(node->condition, env);
            if (cond.truthy()) {
                execBlock(node->thenBlock, env);
            } else if (node->elseBlock) {
                execBlock(node->elseBlock, env);
            }
            return;
        }
        case NodeType::Loop: {
            Value condVal = eval(node->condition, env);
            if (condVal.kind == Value::Kind::Number) {
                long long times = (long long)condVal.num;
                for (long long i = 0; i < times; i++) {
                    execBlock(node->body, env);
                }
            } else {
                if (condVal.truthy()) execBlock(node->body, env);
            }
            return;
        }
        case NodeType::Call:
        case NodeType::BinaryOp:
        case NodeType::Literal:
        case NodeType::Identifier: {
            eval(node, env);
            return;
        }
        default:
            throw std::runtime_error("waa runtime error: cannot execute this node as a statement");
    }
}

Value Interpreter::eval(const NodePtr& node, Env& env) {
    switch (node->type) {
        case NodeType::Literal:
            return node->isNumber ? Value::fromNumber(node->numValue) : Value::fromString(node->strValue);

        case NodeType::Identifier: {
            auto it = env.find(node->strValue);
            if (it != env.end()) return it->second;
            auto git = globals.find(node->strValue);
            if (git != globals.end()) return git->second;
            throw std::runtime_error("waa runtime error: undefined variable '" + node->strValue + "'");
        }

        case NodeType::BinaryOp: {
            Value l = eval(node->left, env);
            Value r = eval(node->right, env);
            if (node->op == "+") {
                if (l.kind == Value::Kind::String || r.kind == Value::Kind::String)
                    return Value::fromString(l.toDisplayString() + r.toDisplayString());
                return Value::fromNumber(l.num + r.num);
            }
            if (node->op == "-") return Value::fromNumber(l.num - r.num);
            if (node->op == "*") return Value::fromNumber(l.num * r.num);
            if (node->op == "/") return Value::fromNumber(r.num != 0 ? l.num / r.num : 0.0);
            if (node->op == "==") return Value::fromNumber(l.toDisplayString() == r.toDisplayString() ? 1.0 : 0.0);
            if (node->op == "<") return Value::fromNumber(l.num < r.num ? 1.0 : 0.0);
            if (node->op == ">") return Value::fromNumber(l.num > r.num ? 1.0 : 0.0);
            throw std::runtime_error("waa runtime error: unknown operator '" + node->op + "'");
        }

        case NodeType::Call: {
            std::vector<Value> argVals;
            argVals.reserve(node->args.size());
            for (const auto& a : node->args) argVals.push_back(eval(a, env));

            auto it = builtins.find(node->callee);
            if (it != builtins.end()) return it->second(argVals);

            throw std::runtime_error("waa runtime error: unknown function '" + node->callee + "'");
        }

        default:
            throw std::runtime_error("waa runtime error: cannot evaluate this node as an expression");
    }
}