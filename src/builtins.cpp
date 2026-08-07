#include "builtins.h"
#include "external.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <chrono>

namespace fs = std::filesystem;

static std::vector<std::string> g_history;
static std::unordered_map<std::string, std::string> g_aliases;
static std::unordered_map<std::string, std::string> g_variables;
static std::chrono::steady_clock::time_point g_startTime;

void pushHistory(const std::string& line) {
    if (!line.empty()) g_history.push_back(line);
}

void markShellStart() {
    g_startTime = std::chrono::steady_clock::now();
}

void resolveAlias(std::vector<std::string>& tokens) {
    if (tokens.empty()) return;
    auto it = g_aliases.find(tokens[0]);
    if (it == g_aliases.end()) return;
    std::istringstream iss(it->second);
    std::vector<std::string> expanded;
    std::string tok;
    while (iss >> tok) expanded.push_back(tok);
    for (size_t i = 1; i < tokens.size(); ++i) expanded.push_back(tokens[i]);
    tokens = expanded;
}

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

static int cmd_ls(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    fs::path target = args.size() > 1 ? fs::path(args[1]) : fs::current_path();
    std::error_code ec;
    if (!fs::exists(target, ec)) { std::cerr << "ls: path not found: " << target.string() << "\n"; return 1; }
    for (auto& entry : fs::directory_iterator(target, ec)) {
        out << (entry.is_directory() ? "[dir]  " : "       ") << entry.path().filename().string() << "\n";
    }
    return 0;
}

static int cmd_mkdir(const std::vector<std::string>& args, std::istream&, std::ostream&) {
    if (args.size() < 2) { std::cerr << "mkdir: missing directory name\n"; return 1; }
    std::error_code ec;
    fs::create_directories(args[1], ec);
    if (ec) { std::cerr << "mkdir: " << ec.message() << "\n"; return 1; }
    return 0;
}

static int cmd_rmdir(const std::vector<std::string>& args, std::istream&, std::ostream&) {
    if (args.size() < 2) { std::cerr << "rmdir: missing directory name\n"; return 1; }
    std::error_code ec;
    bool recursive = args.size() > 2 && (args[2] == "-r" || args[2] == "/s");
    if (recursive) fs::remove_all(args[1], ec);
    else fs::remove(args[1], ec);
    if (ec) { std::cerr << "rmdir: " << ec.message() << "\n"; return 1; }
    return 0;
}

static int cmd_rm(const std::vector<std::string>& args, std::istream&, std::ostream&) {
    if (args.size() < 2) { std::cerr << "rm: missing target\n"; return 1; }
    std::error_code ec;
    bool recursive = args.size() > 2 && (args[2] == "-r" || args[2] == "/s");
    if (recursive) fs::remove_all(args[1], ec);
    else fs::remove(args[1], ec);
    if (ec) { std::cerr << "rm: " << ec.message() << "\n"; return 1; }
    return 0;
}

static int cmd_cp(const std::vector<std::string>& args, std::istream&, std::ostream&) {
    if (args.size() < 3) { std::cerr << "cp: usage: cp <src> <dst>\n"; return 1; }
    std::error_code ec;
    fs::copy(args[1], args[2], fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) { std::cerr << "cp: " << ec.message() << "\n"; return 1; }
    return 0;
}

static int cmd_mv(const std::vector<std::string>& args, std::istream&, std::ostream&) {
    if (args.size() < 3) { std::cerr << "mv: usage: mv <src> <dst>\n"; return 1; }
    std::error_code ec;
    fs::rename(args[1], args[2], ec);
    if (ec) { std::cerr << "mv: " << ec.message() << "\n"; return 1; }
    return 0;
}

static int cmd_touch(const std::vector<std::string>& args, std::istream&, std::ostream&) {
    if (args.size() < 2) { std::cerr << "touch: missing filename\n"; return 1; }
    std::ofstream ofs(args[1], std::ios::app);
    if (!ofs) { std::cerr << "touch: cannot create " << args[1] << "\n"; return 1; }
    return 0;
}

static int cmd_cat(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "cat: missing filename\n"; return 1; }
    std::ifstream ifs(args[1], std::ios::binary);
    if (!ifs) { std::cerr << "cat: cannot open " << args[1] << "\n"; return 1; }
    out << ifs.rdbuf();
    return 0;
}

static int cmd_clear(const std::vector<std::string>&, std::istream&, std::ostream&) {
    std::system("cls");
    return 0;
}

static int cmd_whoami(const std::vector<std::string>&, std::istream&, std::ostream& out) {
    const char* user = std::getenv("USERNAME");
    out << (user ? user : "unknown") << "\n";
    return 0;
}

static int cmd_date(const std::vector<std::string>&, std::istream&, std::ostream& out) {
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    out << buf << "\n";
    return 0;
}

static int cmd_open(const std::vector<std::string>& args, std::istream&, std::ostream&) {
    fs::path target = args.size() > 1 ? fs::path(args[1]) : fs::current_path();
    std::string cmdStr = "explorer \"" + target.string() + "\"";
    std::system(cmdStr.c_str());
    return 0;
}

static int cmd_find(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "find: usage: find <name-substring>\n"; return 1; }
    std::error_code ec;
    int count = 0;
    for (auto& entry : fs::recursive_directory_iterator(fs::current_path(), fs::directory_options::skip_permission_denied, ec)) {
        std::string name = entry.path().filename().string();
        if (name.find(args[1]) != std::string::npos) {
            out << entry.path().string() << "\n";
            ++count;
        }
    }
    if (count == 0) out << "find: no matches for \"" << args[1] << "\"\n";
    return 0;
}

static int cmd_grep(const std::vector<std::string>& args, std::istream& in, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "grep: usage: grep <pattern> [file]\n"; return 1; }
    const std::string& pattern = args[1];
    std::ifstream fileIn;
    std::istream* src = &in;
    if (args.size() > 2) {
        fileIn.open(args[2]);
        if (!fileIn) { std::cerr << "grep: cannot open " << args[2] << "\n"; return 1; }
        src = &fileIn;
    }
    std::string line;
    while (std::getline(*src, line)) {
        if (line.find(pattern) != std::string::npos) out << line << "\n";
    }
    return 0;
}

static int cmd_set(const std::vector<std::string>& args, std::istream&, std::ostream&) {
    if (args.size() < 3) { std::cerr << "set: usage: set <VAR> <value>\n"; return 1; }
    std::string value;
    for (size_t i = 2; i < args.size(); ++i) value += args[i] + (i + 1 < args.size() ? " " : "");
    g_variables[args[1]] = value;
    return 0;
}

static int cmd_env(const std::vector<std::string>&, std::istream&, std::ostream& out) {
    for (auto& kv : g_variables) out << kv.first << "=" << kv.second << "\n";
    return 0;
}

static int cmd_history(const std::vector<std::string>&, std::istream&, std::ostream& out) {
    for (size_t i = 0; i < g_history.size(); ++i)
        out << (i + 1) << "  " << g_history[i] << "\n";
    return 0;
}

static int cmd_alias(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    if (args.size() < 2) {
        for (auto& kv : g_aliases) out << kv.first << " -> " << kv.second << "\n";
        return 0;
    }
    if (args.size() < 3) { std::cerr << "alias: usage: alias <name> <command...>\n"; return 1; }
    std::string cmdStr;
    for (size_t i = 2; i < args.size(); ++i) cmdStr += args[i] + (i + 1 < args.size() ? " " : "");
    g_aliases[args[1]] = cmdStr;
    return 0;
}

static int cmd_which(const std::vector<std::string>& args, std::istream& in, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "which: usage: which <command>\n"; return 1; }
    static const std::vector<std::string> builtinNames = {
        "cd","pwd","echo","ls","dir","mkdir","md","rmdir","rd","rm","del","cp","copy",
        "mv","move","ren","touch","cat","type","clear","cls","whoami","date","open",
        "waa","help","find","grep","set","env","history","alias","which","ps","kill",
        "curl","wget","winget"
    };
    for (auto& n : builtinNames) {
        if (n == args[1]) { out << args[1] << ": shell built-in\n"; return 0; }
    }
    run_external({"where", args[1]}, in, out, false, 0, "");
    return 0;
}

static int cmd_ps(const std::vector<std::string>&, std::istream& in, std::ostream& out) {
    return run_external({"tasklist"}, in, out, false, 0, "");
}

static int cmd_kill(const std::vector<std::string>& args, std::istream& in, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "kill: usage: kill <pid>\n"; return 1; }
    return run_external({"taskkill", "/PID", args[1], "/F"}, in, out, false, 0, "");
}

static int cmd_curl(const std::vector<std::string>& args, std::istream& in, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "curl: usage: curl <url> [-o file] [options...]\n"; return 1; }
    return run_external(args, in, out, false, 0, "");
}

static int cmd_wget(const std::vector<std::string>& args, std::istream& in, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "wget: usage: wget <url> [-o file]\n"; return 1; }
    std::vector<std::string> curlArgs = {"curl", "-L"};
    for (size_t i = 1; i < args.size(); ++i) curlArgs.push_back(args[i]);
    return run_external(curlArgs, in, out, false, 0, "");
}

static int cmd_winget(const std::vector<std::string>& args, std::istream& in, std::ostream& out) {
    if (args.size() < 2) {
        std::cerr << "winget: usage: winget <search|install|list|upgrade|uninstall> [args...]\n";
        return 1;
    }
    return run_external(args, in, out, false, 0, "");
}

static int cmd_waa(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    if (args.size() > 1 && args[1] == "version") { out << "wandaashell v0.5.0\n"; return 0; }
    if (args.size() > 1 && args[1] == "about") {
        out << "wandaashell - a custom C++ shell. github.com/map-boy/wandaashell\n";
        return 0;
    }
    if (args.size() > 1 && args[1] == "uptime") {
        auto elapsed = std::chrono::steady_clock::now() - g_startTime;
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
        out << "uptime: " << (secs / 60) << "m " << (secs % 60) << "s\n";
        return 0;
    }
    out << "wandaashell v0.5.0\n\"waa\" - a small shell with big plans.\nType help to see available commands.\n";
    return 0;
}

static int cmd_help(const std::vector<std::string>&, std::istream&, std::ostream& out) {
    out << "Built-in commands:\n"
        << "  cd, pwd, echo, ls, mkdir, rmdir, rm, cp, mv, touch, cat,\n"
        << "  clear, whoami, date, open, find, grep, set, env, history,\n"
        << "  alias, which, ps, kill, curl, wget, winget, waa, help, exit, quit\n"
        << "Redirection: >, >>, <    Pipes: cmd1 | cmd2\n";
    return 0;
}

std::unordered_map<std::string, CommandFn> makeBuiltins() {
    return {
        {"cd", cmd_cd}, {"pwd", cmd_pwd}, {"echo", cmd_echo},
        {"ls", cmd_ls}, {"dir", cmd_ls},
        {"mkdir", cmd_mkdir}, {"md", cmd_mkdir},
        {"rmdir", cmd_rmdir}, {"rd", cmd_rmdir},
        {"rm", cmd_rm}, {"del", cmd_rm},
        {"cp", cmd_cp}, {"copy", cmd_cp},
        {"mv", cmd_mv}, {"move", cmd_mv}, {"ren", cmd_mv},
        {"touch", cmd_touch},
        {"cat", cmd_cat}, {"type", cmd_cat},
        {"clear", cmd_clear}, {"cls", cmd_clear},
        {"whoami", cmd_whoami},
        {"date", cmd_date},
        {"open", cmd_open},
        {"find", cmd_find},
        {"grep", cmd_grep},
        {"set", cmd_set},
        {"env", cmd_env},
        {"history", cmd_history},
        {"alias", cmd_alias},
        {"which", cmd_which}, {"where", cmd_which},
        {"ps", cmd_ps},
        {"kill", cmd_kill},
        {"curl", cmd_curl},
        {"wget", cmd_wget},
        {"winget", cmd_winget},
        {"waa", cmd_waa},
        {"help", cmd_help},
    };
}
