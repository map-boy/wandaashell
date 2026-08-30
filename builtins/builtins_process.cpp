//
// builtins_process.cpp - built-ins that reach outside the process.
//
// These never touch an OS API directly: they ask platform.h for the argv of
// the native tool and hand it to platform::spawnProcess. That is what lets
// `ps` mean tasklist on Windows and `ps -e` on POSIX without a single #ifdef
// living here.
//
#include "builtins/builtins_internal.h"
#include "platform/platform.h"
#include <iostream>

int cmd_which(const std::vector<std::string>& args, std::istream& in, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "which: usage: which <command>\n"; return 1; }
    for (const auto& n : builtinNames()) {
        if (n == args[1]) { out << args[1] << ": shell built-in\n"; return 0; }
    }
    return platform::spawnProcess(platform::whichCommand(args[1]), in, out, false, 0, "");
}

int cmd_ps(const std::vector<std::string>&, std::istream& in, std::ostream& out) {
    return platform::spawnProcess(platform::processListCommand(), in, out, false, 0, "");
}

int cmd_kill(const std::vector<std::string>& args, std::istream& in, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "kill: usage: kill <pid>\n"; return 1; }
    return platform::spawnProcess(platform::killProcessCommand(args[1]), in, out, false, 0, "");
}

int cmd_curl(const std::vector<std::string>& args, std::istream& in, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "curl: usage: curl <url> [-o file] [options...]\n"; return 1; }
    return platform::spawnProcess(args, in, out, false, 0, "");
}

int cmd_wget(const std::vector<std::string>& args, std::istream& in, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "wget: usage: wget <url> [-o file]\n"; return 1; }
    std::vector<std::string> curlArgs = {"curl", "-L"};
    for (size_t i = 1; i < args.size(); ++i) curlArgs.push_back(args[i]);
    return platform::spawnProcess(curlArgs, in, out, false, 0, "");
}

// Registered as both `pkg` and, for compatibility with existing scripts,
// `winget`. On Windows this is winget verb-for-verb; elsewhere platform.h
// translates the verb to the host's package manager, and when the host has
// none we say so instead of silently doing nothing.
int cmd_pkg(const std::vector<std::string>& args, std::istream& in, std::ostream& out) {
    const std::string invoked = args.empty() ? "pkg" : args[0];
    if (args.size() < 2) {
        std::cerr << invoked << ": usage: " << invoked
                  << " <search|install|list|upgrade|uninstall> [args...]\n";
        return 1;
    }
    std::vector<std::string> verbAndArgs(args.begin() + 1, args.end());
    auto cmd = platform::packageManagerCommand(verbAndArgs);
    if (cmd.empty()) {
        std::cerr << invoked << ": no supported package manager found on this system\n";
        return 1;
    }
    return platform::spawnProcess(cmd, in, out, false, 0, "");
}

int cmd_disasm(const std::vector<std::string>& args, std::istream& in, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "disasm: usage: disasm <file.exe|.dll|.o|.so|.dylib>\n"; return 1; }
    return platform::spawnProcess({"objdump", "-d", args[1]}, in, out, false, 0, "");
}
