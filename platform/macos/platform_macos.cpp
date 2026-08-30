//
// platform_macos.cpp - the parts of the platform contract that are specific to
// macOS. Everything POSIX-generic lives in platform/posix/platform_posix.cpp,
// which this backend shares with Linux.
//
#include "platform/platform.h"
#include "platform/posix/platform_posix.h"

#include <mach-o/dyld.h>
#include <limits.h>
#include <stdlib.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace platform {

// --- filesystem / paths ----------------------------------------------------

std::string getExecutablePath() {
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);          // first call reports the size
    if (size == 0) return std::string();
    std::vector<char> buf(size + 1, '\0');
    if (_NSGetExecutablePath(buf.data(), &size) != 0) return std::string();
    // _NSGetExecutablePath can hand back a path containing symlinks or "..",
    // which matters because assetPath() walks relative to this directory.
    char resolved[PATH_MAX];
    if (realpath(buf.data(), resolved)) return std::string(resolved);
    return std::string(buf.data());
}

void openPath(const std::string& path) {
    std::error_code ec;
    // open(1) on a directory opens it in Finder; -R on a file reveals it in
    // its enclosing folder, which is what Explorer does on Windows.
    if (fs::is_directory(path, ec)) spawnDetachedProcess({"open", path});
    else                            spawnDetachedProcess({"open", "-R", path});
}

// --- privilege -------------------------------------------------------------

namespace {

// Escape a string for embedding in an AppleScript double-quoted literal.
std::string escapeForAppleScript(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    return out;
}

// Escape a string for embedding in a single-quoted POSIX shell word.
std::string escapeForShell(const std::string& in) {
    std::string out = "'";
    for (char c : in) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

// Terminal.app ships with every macOS install, so there is always a fallback;
// iTerm2 wins when it is the terminal the user is already sitting in.
std::string hostTerminal() {
    if (const char* termProgram = std::getenv("TERM_PROGRAM")) {
        if (std::string(termProgram) == "iTerm.app") return "iTerm";
    }
    return "Terminal";
}

} // namespace

std::string elevationMechanism() {
    return "sudo in a new " + hostTerminal() + " window";
}

// macOS has no UAC either, and its two native options both fail this use case:
//
//   - `osascript -e 'do shell script "..." with administrator privileges'`
//     raises the familiar GUI authentication dialog, which is the closest
//     analogue to UAC, but runs the command detached with no controlling
//     terminal. That is fine for a one-shot command and useless for an
//     interactive shell, which needs a tty to read from.
//   - AuthorizationExecuteWithPrivileges() has been deprecated since 10.7.
//     The supported replacement is a privileged helper installed via
//     SMJobBless, which requires a signed, sandboxed .app bundle - a real
//     option if wandaashell is ever shipped as a bundle, and out of scope
//     for a standalone CLI binary.
//
// So this does what the Linux backend does: open a new terminal window running
// the shell under sudo, and let the user authenticate there. The visible
// difference from Windows is a typed password rather than a consent click.
bool requestElevation(const std::string& exePath,
                      const std::string& args,
                      const std::string& workingDir) {
    if (!commandExists("osascript")) {
        std::fputs("fille: osascript not available; cannot open an elevated window\n", stderr);
        return false;
    }

    std::string command;
    if (!workingDir.empty()) command += "cd " + escapeForShell(workingDir) + "; ";
    command += "exec sudo -E " + escapeForShell(exePath);
    if (!args.empty()) command += " " + args;

    const std::string terminal = hostTerminal();
    const std::string script =
        "tell application \"" + terminal + "\" to do script \"" +
        escapeForAppleScript(command) + "\"\n"
        "tell application \"" + terminal + "\" to activate";

    return spawnDetachedProcess({"osascript", "-e", script});
}

// --- packages --------------------------------------------------------------

namespace {

struct PackageManager {
    const char* program;
    bool needsRoot;
    const char* search;
    const char* install;
    const char* list;
    const char* upgrade;
    const char* uninstall;
};

// Homebrew first: it is what the overwhelming majority of macOS developers
// have, and it deliberately does not want to be run under sudo.
const PackageManager kPackageManagers[] = {
    {"brew", false, "search", "install", "list",             "upgrade", "uninstall"},
    {"port", true,  "search", "install", "installed",        "upgrade outdated", "uninstall"},
};

const PackageManager* detect() {
    for (const auto& pm : kPackageManagers) {
        if (commandExists(pm.program)) return &pm;
    }
    return nullptr;
}

} // namespace

std::string packageManagerName() {
    const PackageManager* pm = detect();
    return pm ? pm->program : std::string();
}

std::vector<std::string> packageManagerCommand(const std::vector<std::string>& verbAndArgs) {
    const PackageManager* pm = detect();
    if (!pm || verbAndArgs.empty()) return {};

    const std::string& verb = verbAndArgs[0];
    std::string native;
    bool mutating = false;
    if      (verb == "search")  native = pm->search;
    else if (verb == "install") { native = pm->install; mutating = true; }
    else if (verb == "list")    native = pm->list;
    else if (verb == "upgrade") { native = pm->upgrade; mutating = true; }
    else if (verb == "uninstall" || verb == "remove") { native = pm->uninstall; mutating = true; }
    else return {};   // unknown verb: caller reports it rather than guessing

    if (native.empty()) return {};

    std::vector<std::string> cmd;
    if (mutating && pm->needsRoot && !isElevated() && commandExists("sudo")) cmd.push_back("sudo");
    cmd.push_back(pm->program);
    // A verb can expand to more than one token (e.g. port's "upgrade outdated").
    for (size_t start = 0; start < native.size();) {
        size_t sp = native.find(' ', start);
        if (sp == std::string::npos) sp = native.size();
        cmd.push_back(native.substr(start, sp - start));
        start = sp + 1;
    }
    for (size_t i = 1; i < verbAndArgs.size(); ++i) cmd.push_back(verbAndArgs[i]);
    return cmd;
}

// --- diagnostics -----------------------------------------------------------

// CoreAudio reports failures through OSStatus codes and does not write to the
// terminal, so unlike ALSA on Linux there is nothing here to silence.
void silenceAudioDiagnostics() {}

} // namespace platform
