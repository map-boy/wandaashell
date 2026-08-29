#include "shell.h"
#include "types.h"
#include "tokenizer.h"
#include "pipeline.h"
#include "builtins.h"
#include "external.h"
#include "audio.h"
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
namespace fs = std::filesystem;

static std::unordered_map<std::string, CommandFn>& sharedBuiltins() {
    static auto builtins = makeBuiltins();
    return builtins;
}

static std::vector<std::string> splitStatements(const std::string& line) {
    std::vector<std::string> parts;
    std::string cur;
    char quoteChar = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (quoteChar != 0) {
            cur += c;
            if (c == quoteChar) quoteChar = 0;
            continue;
        }
        if (c == '"' || c == '\'') {
            quoteChar = c;
            cur += c;
            continue;
        }
        if (c == ';') {
            parts.push_back(cur);
            cur.clear();
            continue;
        }
        cur += c;
    }
    parts.push_back(cur);
    return parts;
}

static int runSingleStatement(const std::string& line) {
    if (line.empty()) return 0;
    pushHistory(line);
    auto tokens = tokenize(line);
    if (tokens.empty()) return 0;
    resolveAlias(tokens);
    if (tokens.empty()) return 0;
    if (tokens[0] == "exit" || tokens[0] == "quit") return 1;

    auto& builtins = sharedBuiltins();
    auto stages = parsePipeline(tokens);
    if (stages.empty()) return 0;
    std::ostringstream carry;
    bool havePrevOutput = false;
    for (size_t i = 0; i < stages.size(); ++i) {
        Stage& s = stages[i];
        if (s.args.empty()) continue;
        bool isLast = (i + 1 == stages.size());
        std::istringstream in(havePrevOutput ? carry.str() : "");
        std::istream* inStream = &in;
        std::ifstream fileIn;
        if (s.hasInputRedirect) {
            fileIn.open(s.inputFile, std::ios::binary);
            if (!fileIn) { std::cerr << "wandaashell: cannot open " << s.inputFile << "\n"; break; }
            inStream = &fileIn;
        }
        std::ostringstream nextCarry;
        std::ofstream fileOut;
        std::ostream* outStream = isLast ? static_cast<std::ostream*>(&std::cout)
                                          : static_cast<std::ostream*>(&nextCarry);
        if (isLast && s.redirectMode != 0) {
            fileOut.open(s.redirectFile, s.redirectMode == 2 ? std::ios::app : std::ios::trunc);
            if (!fileOut) { std::cerr << "wandaashell: cannot open " << s.redirectFile << "\n"; break; }
            outStream = &fileOut;
        }
        auto it = builtins.find(s.args[0]);
        if (it != builtins.end()) {
            it->second(s.args, *inStream, *outStream);
        } else {
            run_external(s.args, *inStream, *outStream,
                         havePrevOutput || s.hasInputRedirect,
                         isLast ? s.redirectMode : 0,
                         isLast ? s.redirectFile : "");
        }
        if (!isLast) {
            carry.str(nextCarry.str());
            havePrevOutput = true;
        }
    }
    return 0;
}

int runShellLine(const std::string& line) {
    auto statements = splitStatements(line);
    for (auto& stmt : statements) {
        std::string trimmed = stmt;
        size_t start = trimmed.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = trimmed.find_last_not_of(" \t\r\n");
        trimmed = trimmed.substr(start, end - start + 1);
        int rc = runSingleStatement(trimmed);
        if (rc == 1) return 1;
    }
    return 0;
}

static bool isElevated() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

void runShell() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleTitleA("wandaa");
    markShellStart();
    playVoiceAsync();
    std::cout << "wandaashell v0.4.0\n";
    std::string line;
    while (true) {
        bool admin = isElevated();
        std::cout << (admin ? "\x1b[31mwandaa[ADMIN] " : "\x1b[32mwandaa ")
                   << fs::current_path().string() << " >\x1b[0m ";
        if (!std::getline(std::cin, line)) break;
        int rc = runShellLine(line);
        if (rc == 1) break;
    }
}