//
// platform_windows.cpp - Windows implementation of the platform contract.
//
// Everything in this file was previously scattered across shell.cpp,
// builtins.cpp, external.cpp and audio.cpp. Behaviour is intentionally
// unchanged from those originals: this file is a relocation, not a rewrite.
//
#include "../platform.h"

#include <windows.h>
#include <shellapi.h>

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace platform {

// --- capabilities ----------------------------------------------------------

bool supportsExternalProcesses() { return true; }
bool supportsElevation() { return true; }

// --- identity / privilege --------------------------------------------------

bool isElevated() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin != FALSE;
}

std::string elevationMechanism() { return "UAC"; }

// Launches an elevated copy through the "runas" verb, which raises the UAC
// consent dialog. Preference is Windows Terminal with the "wandaashell"
// profile (this is what the original `fille` did, and it is what gives the
// elevated window the configured icon and background image); when wt.exe is
// not installed we fall back to launching the shell executable directly, which
// still elevates but without the custom terminal chrome.
bool requestElevation(const std::string& exePath,
                      const std::string& args,
                      const std::string& workingDir) {
    // Keep the strings alive for the duration of the call - SHELLEXECUTEINFOA
    // stores raw pointers into them.
    std::string dir = workingDir;
    std::string file = "wt.exe";
    std::string params = "-p \"wandaashell\"";

    bool haveWindowsTerminal = false;
    {
        char found[MAX_PATH];
        char* filePart = nullptr;
        haveWindowsTerminal = SearchPathA(NULL, "wt.exe", NULL, MAX_PATH, found, &filePart) != 0;
    }
    if (!haveWindowsTerminal) {
        file = exePath;
        params = args;
    }

    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = "runas";
    sei.lpFile = file.c_str();
    sei.lpParameters = params.empty() ? NULL : params.c_str();
    sei.lpDirectory = dir.empty() ? NULL : dir.c_str();
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExA(&sei)) return false;
    if (sei.hProcess) CloseHandle(sei.hProcess);
    return true;
}

std::string currentUserName() {
    const char* user = std::getenv("USERNAME");
    return user ? user : "unknown";
}

// --- filesystem / paths ----------------------------------------------------

std::string getExecutablePath() {
    char exePath[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    return n ? std::string(exePath, n) : std::string();
}

std::string getExecutableDir() {
    std::string path = getExecutablePath();
    size_t pos = path.find_last_of("\\/");
    return (pos != std::string::npos) ? path.substr(0, pos) : ".";
}

std::string pathSeparator() { return "\\"; }

std::string assetPath(const std::string& assetRelativePath) {
    std::error_code ec;
    const std::string dir = getExecutableDir();
    // Search order: next to the exe (the shipped layout), then one level up
    // (a build tree, where the exe sits in build/), then the working directory.
    const fs::path candidates[] = {
        fs::path(dir) / "assets" / assetRelativePath,
        fs::path(dir) / ".." / "assets" / assetRelativePath,
        fs::current_path(ec) / "assets" / assetRelativePath,
    };
    for (const auto& c : candidates) {
        if (fs::exists(c, ec)) return c.lexically_normal().string();
    }
    return std::string();
}

void openPath(const std::string& path) {
    ShellExecuteA(NULL, "open", "explorer.exe", ("\"" + path + "\"").c_str(), NULL, SW_SHOWNORMAL);
}

// --- process execution -----------------------------------------------------

// Piping is done through temporary files rather than real pipe handles. This
// is the original implementation, kept unchanged: it is not throughput
// optimal, but it is the behaviour every existing wandaashell script has been
// written against. The POSIX backends use real fork/exec pipes.
int spawnProcess(const std::vector<std::string>& args,
                 std::istream& in, std::ostream& out,
                 bool pipeInput, int redirectMode,
                 const std::string& redirectFile) {
    std::vector<std::string> realArgs = args;
    if (!realArgs.empty()) {
        std::string cmd0 = realArgs[0];
        std::string lower = cmd0;
        for (auto& c : lower) c = (char)tolower((unsigned char)c);
        if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".ps1") {
            std::vector<std::string> wrapped = {"powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", cmd0};
            for (size_t i = 1; i < realArgs.size(); ++i) wrapped.push_back(realArgs[i]);
            realArgs = wrapped;
        }
    }
    std::string full;
    for (auto& a : realArgs) full += a + " ";

    std::string tmpIn, tmpOut;
    bool usedTmpIn = false, usedTmpOut = false;

    if (pipeInput) {
        tmpIn = "wandaa_stdin.tmp";
        std::ofstream ofs(tmpIn, std::ios::binary);
        ofs << in.rdbuf();
        ofs.close();
        full += "< " + tmpIn + " ";
        usedTmpIn = true;
    }

    bool captureToStream = (&out != &std::cout);
    if (redirectMode == 1) full += "> " + redirectFile + " ";
    else if (redirectMode == 2) full += ">> " + redirectFile + " ";
    else if (captureToStream) {
        tmpOut = "wandaa_stdout.tmp";
        full += "> " + tmpOut + " ";
        usedTmpOut = true;
    }

    int rc = std::system(full.c_str());

    // Some console programs leave the input handle in a mode that breaks
    // std::getline for the REPL; restore a sane one.
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE) {
        SetConsoleMode(hIn, ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT |
                            ENABLE_INSERT_MODE | ENABLE_EXTENDED_FLAGS | ENABLE_QUICK_EDIT_MODE);
    }
    std::cin.clear();

    if (usedTmpOut) {
        std::ifstream ifs(tmpOut, std::ios::binary);
        out << ifs.rdbuf();
        ifs.close();
        std::error_code ec;
        fs::remove(tmpOut, ec);
    }
    if (usedTmpIn) {
        std::error_code ec;
        fs::remove(tmpIn, ec);
    }
    return rc;
}

std::vector<std::string> processListCommand() {
    return {"tasklist"};
}

std::vector<std::string> killProcessCommand(const std::string& pid) {
    return {"taskkill", "/PID", pid, "/F"};
}

std::vector<std::string> whichCommand(const std::string& name) {
    return {"where", name};
}

std::string packageManagerName() { return "winget"; }

std::vector<std::string> packageManagerCommand(const std::vector<std::string>& verbAndArgs) {
    // winget's verbs are already the shell's verbs, so this is a pass-through.
    std::vector<std::string> cmd = {"winget"};
    for (const auto& a : verbAndArgs) cmd.push_back(a);
    return cmd;
}

// --- console / presentation ------------------------------------------------

bool enableAnsiColors() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) return false;
    return SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}

void setConsoleTitle(const std::string& title) {
    SetConsoleTitleA(title.c_str());
}

void clearScreen() {
    std::system("cls");
}

// --- diagnostics -----------------------------------------------------------

// WASAPI reports failures through return codes and never writes to the
// terminal, so there is nothing to silence on Windows.
void silenceAudioDiagnostics() {}

} // namespace platform
