//
// platform_linux.cpp - the parts of the platform contract that are specific to
// Linux. Everything POSIX-generic lives in platform/posix/platform_posix.cpp.
//
#include "platform/platform.h"
#include "platform/posix/platform_posix.h"

#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace platform {

// --- filesystem / paths ----------------------------------------------------

std::string getExecutablePath() {
    char buf[PATH_MAX];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return std::string();
    buf[n] = '\0';
    return std::string(buf);
}

void openPath(const std::string& path) {
    // xdg-open is the freedesktop.org standard indirection; it hands off to
    // whatever file manager the desktop environment has registered.
    if (commandExists("xdg-open")) { spawnDetachedProcess({"xdg-open", path}); return; }
    // Bare desktops sometimes ship only the file manager itself.
    for (const char* fm : {"nautilus", "dolphin", "thunar", "nemo", "pcmanfm"}) {
        if (commandExists(fm)) { spawnDetachedProcess({fm, path}); return; }
    }
    std::fputs("open: no file manager found (install xdg-utils)\n", stderr);
}

// --- privilege -------------------------------------------------------------

namespace {

// Terminal emulators and the flag each one uses to run a command. This is
// looked up at runtime rather than hardcoded, because which of these exists
// varies wildly between distributions and desktop environments.
struct TerminalSpec { const char* program; const char* execFlag; };

const TerminalSpec kTerminals[] = {
    {"x-terminal-emulator", "-e"},   // Debian/Ubuntu alternatives system
    {"gnome-terminal",      "--"},   // gnome-terminal dropped -e
    {"konsole",             "-e"},
    {"xfce4-terminal",      "-x"},
    {"mate-terminal",       "-x"},
    {"tilix",               "-e"},
    {"alacritty",           "-e"},
    {"kitty",               "--"},
    {"wezterm",             "-e"},
    {"foot",                "-e"},
    {"urxvt",               "-e"},
    {"xterm",               "-e"},
};

// $TERMINAL is the user's explicit choice and wins over anything we detect.
bool findTerminal(std::string& program, std::string& execFlag) {
    if (const char* envTerm = std::getenv("TERMINAL")) {
        if (commandExists(envTerm)) {
            program = envTerm;
            execFlag = "-e";
            for (const auto& t : kTerminals) {
                if (program.size() >= std::string(t.program).size() &&
                    program.compare(program.size() - std::string(t.program).size(),
                                    std::string::npos, t.program) == 0) {
                    execFlag = t.execFlag;
                    break;
                }
            }
            return true;
        }
    }
    for (const auto& t : kTerminals) {
        if (commandExists(t.program)) { program = t.program; execFlag = t.execFlag; return true; }
    }
    return false;
}

// pkexec shows a graphical PolicyKit dialog where one is available; sudo -E
// falls back to a password prompt inside the new terminal window.
bool findAuthTool(std::vector<std::string>& prefix, std::string& mechanism) {
    if (commandExists("pkexec")) { prefix = {"pkexec"}; mechanism = "pkexec"; return true; }
    if (commandExists("sudo"))   { prefix = {"sudo", "-E"}; mechanism = "sudo"; return true; }
    if (commandExists("doas"))   { prefix = {"doas"}; mechanism = "doas"; return true; }
    return false;
}

} // namespace

std::string elevationMechanism() {
    std::vector<std::string> prefix;
    std::string mechanism;
    if (!findAuthTool(prefix, mechanism)) return "no privilege escalation tool found";
    return mechanism;
}

// Linux has no UAC. There is no OS-level "consent to this program running as
// root" dialog that a process can raise for itself, so this does the closest
// honest equivalent: open a new terminal window running the shell under
// pkexec/sudo/doas, which prompts the user for authentication. Two visible
// differences from the Windows behaviour, both unavoidable:
//   - the user types a password rather than clicking Yes;
//   - a machine with no terminal emulator installed cannot do this at all,
//     and we return false rather than pretending we launched something.
bool requestElevation(const std::string& exePath,
                      const std::string& args,
                      const std::string& workingDir) {
    std::vector<std::string> auth;
    std::string mechanism;
    if (!findAuthTool(auth, mechanism)) {
        std::fputs("fille: no pkexec, sudo or doas on this system\n", stderr);
        return false;
    }

    std::string terminal, execFlag;
    if (!findTerminal(terminal, execFlag)) {
        std::fputs("fille: no terminal emulator found to host the elevated shell\n", stderr);
        return false;
    }

    // `cd <dir> && exec <auth> <exe> <args>` keeps the elevated shell in the
    // directory the user launched it from, which is what the Windows path does
    // through SHELLEXECUTEINFO::lpDirectory.
    std::string inner;
    if (!workingDir.empty()) inner += "cd '" + workingDir + "' && ";
    inner += "exec";
    for (const auto& a : auth) inner += " " + a;
    inner += " '" + exePath + "'";
    if (!args.empty()) inner += " " + args;

    return spawnDetachedProcess({terminal, execFlag, "sh", "-c", inner});
}

// --- packages --------------------------------------------------------------

namespace {

struct PackageManager {
    const char* program;
    bool needsRoot;                 // mutating verbs require privilege
    const char* search;
    const char* install;
    const char* list;
    const char* upgrade;
    const char* uninstall;
};

// One row per manager, in detection order. Empty string = verb unsupported.
const PackageManager kPackageManagers[] = {
    {"apt-get", true,  "",         "install", "",            "upgrade", "remove"},
    {"dnf",     true,  "search",   "install", "list",        "upgrade", "remove"},
    {"pacman",  true,  "-Ss",      "-S",      "-Q",          "-Syu",    "-R"},
    {"zypper",  true,  "search",   "install", "packages -i", "update",  "remove"},
    {"apk",     true,  "search",   "add",     "info",        "upgrade", "del"},
    {"xbps-install", true, "",     "-S",      "",            "-Su",     ""},
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
    if      (verb == "search")    native = pm->search;
    else if (verb == "install")   { native = pm->install;   mutating = true; }
    else if (verb == "list")      native = pm->list;
    else if (verb == "upgrade")   { native = pm->upgrade;   mutating = true; }
    else if (verb == "uninstall" || verb == "remove") { native = pm->uninstall; mutating = true; }
    else return {};   // unknown verb: caller reports it rather than guessing

    if (native.empty()) {
        // apt-get has no search verb; apt-cache does. Special-case rather than
        // silently running the wrong thing.
        if (verb == "search" && std::string(pm->program) == "apt-get" && commandExists("apt-cache")) {
            std::vector<std::string> cmd = {"apt-cache", "search"};
            for (size_t i = 1; i < verbAndArgs.size(); ++i) cmd.push_back(verbAndArgs[i]);
            return cmd;
        }
        if (verb == "list" && std::string(pm->program) == "apt-get" && commandExists("apt")) {
            return {"apt", "list", "--installed"};
        }
        return {};
    }

    std::vector<std::string> cmd;
    if (mutating && pm->needsRoot && !isElevated() && commandExists("sudo")) cmd.push_back("sudo");
    cmd.push_back(pm->program);
    // A verb can expand to more than one token (e.g. zypper's "packages -i").
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

namespace {

// ALSA's error handler signature. Declared here rather than pulled from
// <alsa/error.h> so the build does not need ALSA headers installed - the
// library itself is resolved at runtime, exactly as miniaudio resolves it.
using SndLibErrorHandler = void (*)(const char* file, int line, const char* function,
                                    int err, const char* fmt, ...);
using SndLibErrorSetHandler = int (*)(SndLibErrorHandler);

void swallowAlsaDiagnostic(const char*, int, const char*, int, const char*, ...) {}

} // namespace

// Without this, opening the audio device on a machine with no sound card -
// a container, a headless server, a VM - dumps roughly two dozen "cannot find
// card '0'" lines into the middle of the user's session. ALSA writes those
// itself, so only ALSA can be asked to stop.
void silenceAudioDiagnostics() {
    // Same soname miniaudio loads, so this handler applies to its device
    // probing too. If ALSA is not present there is no noise to silence.
    void* alsa = dlopen("libasound.so.2", RTLD_LAZY | RTLD_LOCAL);
    if (!alsa) return;
    auto setHandler = reinterpret_cast<SndLibErrorSetHandler>(
        dlsym(alsa, "snd_lib_error_set_handler"));
    if (setHandler) setHandler(&swallowAlsaDiagnostic);
    // Deliberately not dlclose()d: the handler must stay valid for the life of
    // the process, and miniaudio keeps its own reference to the library anyway.
}

} // namespace platform
