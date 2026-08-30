#pragma once
#include <vector>
#include <string>
#include <memory>

enum class NodeType {
    Program,
    Assign,
    If,
    Loop,
    Call,
    BinaryOp,
    Literal,
    Identifier,
    Block
};

struct Node;
using NodePtr = std::shared_ptr<Node>;

struct Node {
    NodeType type;

    // Assign: target = value
    std::string target;

    // Identifier / Literal
    std::string strValue;
    double numValue = 0.0;
    bool isNumber = false;

    // BinaryOp
    std::string op; // "+", "-", "==", "<", etc.
    NodePtr left;
    NodePtr right;

    // If: condition + thenBlock + elseBlock
    NodePtr condition;
    NodePtr thenBlock;
    NodePtr elseBlock; // nullptr if none

    // Loop: condition + body
    NodePtr body;

    // Call: function name + args
    std::string callee;
    std::vector<NodePtr> args;

    // Program / Block: sequence of statements
    std::vector<NodePtr> statements;

    // value used for Assign's RHS
    NodePtr value;
};

inline NodePtr makeNode(NodeType t) {
    auto n = std::make_shared<Node>();
    n->type = t;
    return n;
}