#include "core/script_parser.h"
#include <cctype>
#include <stdexcept>

namespace {

enum class TokType { Ident, Number, String, Op, LParen, RParen, LBrace, RBrace, Comma, Eq, EqEq, Lt, Gt, End };

struct Tok {
    TokType type;
    std::string text;
};

std::vector<Tok> tokenizeScript(const std::string& src) {
    std::vector<Tok> toks;
    size_t i = 0, n = src.size();
    while (i < n) {
        char c = src[i];
        if (isspace((unsigned char)c)) { i++; continue; }
        if (c == '#') { while (i < n && src[i] != '\n') i++; continue; }
        if (isalpha((unsigned char)c) || c == '_') {
            size_t start = i;
            while (i < n && (isalnum((unsigned char)src[i]) || src[i] == '_')) i++;
            toks.push_back({TokType::Ident, src.substr(start, i - start)});
            continue;
        }
        if (isdigit((unsigned char)c)) {
            size_t start = i;
            while (i < n && (isdigit((unsigned char)src[i]) || src[i] == '.')) i++;
            toks.push_back({TokType::Number, src.substr(start, i - start)});
            continue;
        }
        if (c == '"') {
            size_t start = ++i;
            while (i < n && src[i] != '"') i++;
            toks.push_back({TokType::String, src.substr(start, i - start)});
            i++;
            continue;
        }
        if (c == '=' && i + 1 < n && src[i + 1] == '=') { toks.push_back({TokType::EqEq, "=="}); i += 2; continue; }
        if (c == '=') { toks.push_back({TokType::Eq, "="}); i++; continue; }
        if (c == '<') { toks.push_back({TokType::Lt, "<"}); i++; continue; }
        if (c == '>') { toks.push_back({TokType::Gt, ">"}); i++; continue; }
        if (c == '(') { toks.push_back({TokType::LParen, "("}); i++; continue; }
        if (c == ')') { toks.push_back({TokType::RParen, ")"}); i++; continue; }
        if (c == '{') { toks.push_back({TokType::LBrace, "{"}); i++; continue; }
        if (c == '}') { toks.push_back({TokType::RBrace, "}"}); i++; continue; }
        if (c == ',') { toks.push_back({TokType::Comma, ","}); i++; continue; }
        if (c == '+' || c == '-' || c == '*' || c == '/') {
            toks.push_back({TokType::Op, std::string(1, c)}); i++; continue;
        }
        throw std::runtime_error(std::string("waa parse error: unexpected character '") + c + "'");
    }
    toks.push_back({TokType::End, ""});
    return toks;
}

struct Parser {
    std::vector<Tok> toks;
    size_t pos = 0;

    const Tok& peek() { return toks[pos]; }
    const Tok& advance() { return toks[pos++]; }
    bool check(TokType t) { return peek().type == t; }
    bool match(TokType t) { if (check(t)) { pos++; return true; } return false; }
    const Tok& expect(TokType t, const char* msg) {
        if (!check(t)) throw std::runtime_error(std::string("waa parse error: expected ") + msg);
        return advance();
    }

    NodePtr parseProgram() {
        auto prog = makeNode(NodeType::Program);
        while (!check(TokType::End)) {
            prog->statements.push_back(parseStatement());
        }
        return prog;
    }

    NodePtr parseBlock() {
        expect(TokType::LBrace, "'{'");
        auto blk = makeNode(NodeType::Block);
        while (!check(TokType::RBrace) && !check(TokType::End)) {
            blk->statements.push_back(parseStatement());
        }
        expect(TokType::RBrace, "'}'");
        return blk;
    }

    NodePtr parseStatement() {
        if (check(TokType::Ident) && peek().text == "if") return parseIf();
        if (check(TokType::Ident) && peek().text == "loop") return parseLoop();

        if (check(TokType::Ident)) {
            size_t save = pos;
            std::string name = advance().text;
            if (check(TokType::Eq)) {
                advance();
                auto node = makeNode(NodeType::Assign);
                node->target = name;
                node->value = parseExpr();
                return node;
            }
            pos = save;
        }
        return parseExpr();
    }

    NodePtr parseIf() {
        advance();
        auto node = makeNode(NodeType::If);
        node->condition = parseExpr();
        node->thenBlock = parseBlock();
        if (check(TokType::Ident) && peek().text == "else") {
            advance();
            node->elseBlock = parseBlock();
        }
        return node;
    }

    NodePtr parseLoop() {
        advance();
        auto node = makeNode(NodeType::Loop);
        node->condition = parseExpr();
        node->body = parseBlock();
        return node;
    }

    NodePtr parseExpr() {
        NodePtr left = parseTerm();
        while (check(TokType::Op) || check(TokType::EqEq) || check(TokType::Lt) || check(TokType::Gt)) {
            std::string op = advance().text;
            NodePtr right = parseTerm();
            auto node = makeNode(NodeType::BinaryOp);
            node->op = op;
            node->left = left;
            node->right = right;
            left = node;
        }
        return left;
    }

    NodePtr parseTerm() {
        if (check(TokType::Number)) {
            auto node = makeNode(NodeType::Literal);
            node->isNumber = true;
            node->numValue = std::stod(advance().text);
            return node;
        }
        if (check(TokType::String)) {
            auto node = makeNode(NodeType::Literal);
            node->isNumber = false;
            node->strValue = advance().text;
            return node;
        }
        if (check(TokType::Ident)) {
            std::string name = advance().text;
            if (check(TokType::LParen)) {
                advance();
                auto node = makeNode(NodeType::Call);
                node->callee = name;
                if (!check(TokType::RParen)) {
                    node->args.push_back(parseExpr());
                    while (match(TokType::Comma)) {
                        node->args.push_back(parseExpr());
                    }
                }
                expect(TokType::RParen, "')'");
                return node;
            }
            auto node = makeNode(NodeType::Identifier);
            node->strValue = name;
            return node;
        }
        if (match(TokType::LParen)) {
            NodePtr inner = parseExpr();
            expect(TokType::RParen, "')'");
            return inner;
        }
        throw std::runtime_error("waa parse error: expected expression");
    }
};

} // namespace

NodePtr parseScript(const std::string& source) {
    Parser p;
    p.toks = tokenizeScript(source);
    return p.parseProgram();
}