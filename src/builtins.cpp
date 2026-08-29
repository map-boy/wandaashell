#include "builtins.h"
#include "external.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <vector>
#include <iomanip>
#include "script_parser.h"
#include <windows.h>
#include "interpreter.h"
#include "audio.h"

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

static bool wildcardMatch(const std::string& name, const std::string& pattern) {
    size_t n = 0, p = 0, star = std::string::npos, mark = 0;
    while (n < name.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == name[n])) {
            ++n; ++p;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++; mark = n;
        } else if (star != std::string::npos) {
            p = star + 1; n = ++mark;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

static void listOne(const fs::path& dir, const std::string& pattern, std::ostream& out) {
    std::error_code ec;
    bool hasWildcard = pattern.find('*') != std::string::npos || pattern.find('?') != std::string::npos;
    if (!hasWildcard) {
        fs::path target = dir / pattern;
        if (!fs::exists(target, ec)) { std::cerr << "ls: path not found: " << target.string() << "\n"; return; }
        if (fs::is_directory(target, ec)) {
            for (auto& entry : fs::directory_iterator(target, ec))
                out << (entry.is_directory() ? "[dir]  " : "       ") << entry.path().filename().string() << "\n";
        } else {
            out << "       " << target.filename().string() << "\n";
        }
        return;
    }
    for (auto& entry : fs::directory_iterator(dir, ec)) {
        std::string name = entry.path().filename().string();
        if (wildcardMatch(name, pattern))
            out << (entry.is_directory() ? "[dir]  " : "       ") << name << "\n";
    }
}

static int cmd_ls(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    fs::path dir = fs::current_path();
    if (args.size() <= 1) {
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(dir, ec))
            out << (entry.is_directory() ? "[dir]  " : "       ") << entry.path().filename().string() << "\n";
        return 0;
    }
    for (size_t i = 1; i < args.size(); ++i) listOne(dir, args[i], out);
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

static int cmd_rm(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "rm: missing target\n"; return 1; }
    if (!fs::exists(args[1])) {
        playVoiceAsync();
        out << "rm: nothing left to delete here.\n";
        return 1;
    }
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

static int cmd_run(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "run: usage: run <file.waa>\n"; return 1; }
    std::ifstream ifs(args[1], std::ios::binary);
    if (!ifs) { std::cerr << "run: cannot open " << args[1] << "\n"; return 1; }
    std::ostringstream buf;
    buf << ifs.rdbuf();
    std::string source = buf.str();
    try {
        NodePtr program = parseScript(source);
        Interpreter interp;
        interp.run(program);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
static int cmd_hexcat(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "hexcat: usage: hexcat <file> [max-lines]\n"; return 1; }
    std::ifstream ifs(args[1], std::ios::binary);
    if (!ifs) { std::cerr << "hexcat: cannot open " << args[1] << "\n"; return 1; }
    std::vector<unsigned char> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    size_t maxLines = args.size() > 2 ? std::stoul(args[2]) : 32;
    size_t lineCount = 0;
    for (size_t off = 0; off < buf.size(); off += 16) {
        if (lineCount++ >= maxLines) {
            out << "... (" << (buf.size() - off) / 16 << " more lines, use: hexcat " << args[1] << " <N> to see more)\n";
            break;
        }
        out << std::hex << std::setw(8) << std::setfill('0') << off << "  ";
        for (size_t j = 0; j < 16; ++j) {
            if (off + j < buf.size()) out << std::hex << std::setw(2) << std::setfill('0') << (int)buf[off + j] << " ";
            else out << "   ";
        }
        out << " ";
        for (size_t j = 0; j < 16 && off + j < buf.size(); ++j) {
            unsigned char c = buf[off + j];
            out << (char)(isprint(c) ? c : '.');
        }
        out << std::dec << "\n";
    }
    return 0;
}

static const std::string b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64Encode(const std::vector<unsigned char>& data) {
    std::string out;
    int val = 0, bits = -6;
    for (unsigned char c : data) {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0) {
            out.push_back(b64chars[(val >> bits) & 0x3F]);
            bits -= 6;
        }
    }
    if (bits > -6) out.push_back(b64chars[((val << 8) >> (bits + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

static std::vector<unsigned char> base64Decode(const std::string& in) {
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[(unsigned char)b64chars[i]] = i;
    std::vector<unsigned char> out;
    int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        bits += 6;
        if (bits >= 0) {
            out.push_back((unsigned char)((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

static int cmd_b64encode(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "b64encode: usage: b64encode <file>\n"; return 1; }
    std::ifstream ifs(args[1], std::ios::binary);
    if (!ifs) { std::cerr << "b64encode: cannot open " << args[1] << "\n"; return 1; }
    std::vector<unsigned char> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    out << base64Encode(buf) << "\n";
    return 0;
}

static int cmd_b64decode(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "b64decode: usage: b64decode <text-or-file>\n"; return 1; }
    std::string input = args[1];
    std::ifstream ifs(args[1], std::ios::binary);
    if (ifs) { std::ostringstream ss; ss << ifs.rdbuf(); input = ss.str(); }
    auto bytes = base64Decode(input);
    for (unsigned char b : bytes) out << b;
    return 0;
}

static void xorCipher(std::vector<unsigned char>& data, const std::string& key) {
    for (size_t i = 0; i < data.size(); ++i) data[i] ^= (unsigned char)key[i % key.size()];
}

static int cmd_encrypt(const std::vector<std::string>& args, std::istream&, std::ostream&) {
    if (args.size() < 4) { std::cerr << "encrypt: usage: encrypt <infile> <outfile> <key>\n"; return 1; }
    std::ifstream ifs(args[1], std::ios::binary);
    if (!ifs) { std::cerr << "encrypt: cannot open " << args[1] << "\n"; return 1; }
    std::vector<unsigned char> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    xorCipher(buf, args[3]);
    std::ofstream ofs(args[2], std::ios::binary);
    ofs.write((char*)buf.data(), buf.size());
    std::cout << "encrypt: wrote " << args[2] << " (" << buf.size() << " bytes)\n";
    return 0;
}

static int cmd_decrypt(const std::vector<std::string>& args, std::istream&, std::ostream&) {
    if (args.size() < 4) { std::cerr << "decrypt: usage: decrypt <infile> <outfile> <key>\n"; return 1; }
    std::ifstream ifs(args[1], std::ios::binary);
    if (!ifs) { std::cerr << "decrypt: cannot open " << args[1] << "\n"; return 1; }
    std::vector<unsigned char> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    xorCipher(buf, args[3]); // XOR is symmetric: same op decrypts
    std::ofstream ofs(args[2], std::ios::binary);
    ofs.write((char*)buf.data(), buf.size());
    std::cout << "decrypt: wrote " << args[2] << " (" << buf.size() << " bytes)\n";
    return 0;
}

static int cmd_disasm(const std::vector<std::string>& args, std::istream& in, std::ostream& out) {
    if (args.size() < 2) { std::cerr << "disasm: usage: disasm <file.exe|.dll|.o>\n"; return 1; }
    return run_external({"objdump", "-d", args[1]}, in, out, false, 0, "");
}

static int cmd_insertafter(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    if (args.size() < 4) { std::cerr << "insertafter: usage: insertafter <file> <pattern> <text...>\n"; return 1; }
    std::ifstream ifs(args[1]);
    if (!ifs) { std::cerr << "insertafter: cannot open " << args[1] << "\n"; return 1; }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ifs, line)) lines.push_back(line);
    ifs.close();

    std::string insertText;
    for (size_t i = 3; i < args.size(); ++i) insertText += args[i] + (i + 1 < args.size() ? " " : "");

    bool found = false;
    std::vector<std::string> result;
    for (auto& l : lines) {
        result.push_back(l);
        if (!found && l.find(args[2]) != std::string::npos) {
            result.push_back(insertText);
            found = true;
        }
    }
    if (!found) { std::cerr << "insertafter: pattern not found: " << args[2] << "\n"; return 1; }

    std::ofstream ofs(args[1], std::ios::trunc);
    for (auto& l : result) ofs << l << "\n";
    out << "insertafter: inserted 1 line into " << args[1] << "\n";
    return 0;
}

static int cmd_grepn(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    if (args.size() < 3) { std::cerr << "grepn: usage: grepn <pattern> <file>\n"; return 1; }
    std::ifstream ifs(args[2]);
    if (!ifs) { std::cerr << "grepn: cannot open " << args[2] << "\n"; return 1; }
    std::string line;
    int num = 0;
    while (std::getline(ifs, line)) {
        ++num;
        if (line.find(args[1]) != std::string::npos) out << num << ": " << line << "\n";
    }
    return 0;
}
static int cmd_fille(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    std::ifstream passFile("admin_pass.txt");
    if (!passFile) { std::cerr << "fille: admin_pass.txt not found (create it next to wandaashell.exe)\n"; return 1; }
    std::string expected;
    std::getline(passFile, expected);
    while (!expected.empty() && (expected.back() == '\r' || expected.back() == '\n')) expected.pop_back();

    std::cout << "fille password: ";
    std::string entered;
    std::getline(std::cin, entered);
    while (!entered.empty() && (entered.back() == '\r' || entered.back() == '\n')) entered.pop_back();

    if (entered != expected) {
        std::cerr << "fille: incorrect password\n";
        return 1;
    }

    std::string wtParams = "-p \"wandaashell\"";

    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = "runas";
    sei.lpFile = "wt.exe";
    sei.lpParameters = wtParams.c_str();
    sei.lpDirectory = fs::current_path().string().c_str();
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExA(&sei)) {
        std::cerr << "fille: elevation cancelled or failed\n";
        return 1;
    }
    if (sei.hProcess) CloseHandle(sei.hProcess);
    out << "fille: opened a new elevated wandaashell window (wandaashell profile)\n";
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
        {"grepn", cmd_grepn},
        {"insertafter", cmd_insertafter},
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
        {"fille", cmd_fille},
        {"hexcat", cmd_hexcat}, {"b64encode", cmd_b64encode}, {"b64decode", cmd_b64decode},
        {"encrypt", cmd_encrypt}, {"decrypt", cmd_decrypt}, {"disasm", cmd_disasm},
        {"run", cmd_run},
    };
}
