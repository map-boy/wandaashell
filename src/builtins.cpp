#include "builtins.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static int cmd_cd(const std::vector<std::string>& args, std::istream&, std::ostream&) {
    if (args.size() < 2) { std::cerr << "cd: missing path\n"; return 1; }
    std::error_code ec;
    fs::current_path(args[1], ec);
    if (ec) { std::cerr << "cd: " << ec.message() << "\n"; return 1; }
    return 0;
}

static int cmd_pwd(const std::vector<std::string>&, std::istream&, std::ostream& out) {
    out << fs::current_path().string() << "\n";
    return 0;
}

static int cmd_echo(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    for (size_t i = 1; i < args.size(); ++i)
        out << args[i] << (i + 1 < args.size() ? " " : "");
    out << "\n";
    return 0;
}

std::unordered_map<std::string, CommandFn> makeBuiltins() {
    return {
        {"cd", cmd_cd},
        {"pwd", cmd_pwd},
        {"echo", cmd_echo},
    };
}
