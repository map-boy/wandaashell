//
// platform_mobile_common.cpp - the Tier 2 platform backend, shared by iOS and
// Android.
//
// READ THIS BEFORE ADDING ANYTHING HERE.
//
// This is not a cut-down desktop backend that will grow into a full one. iOS
// App Store apps cannot fork/exec arbitrary binaries or spawn a shell process
// - that is an OS-level restriction, not a missing feature to code around -
// and while Android permits it at the OS level, Google Play policy and normal
// distribution rule out running arbitrary or dynamically fetched binaries. So
// supportsExternalProcesses() and supportsElevation() return false, the shell
// removes the commands that depend on them from its command table, and users
// of a mobile build never see a command that pretends to work.
//
// The tokenizer, pipeline parser, .waa interpreter and every filesystem
// built-in are the same code the desktop shell runs, unchanged.
//
// Audio and the sandbox's own directories are the two things iOS and Android
// genuinely do differently, so they live in platform_ios.mm and
// platform_android.cpp respectively - not here.
//
#include "platform/platform.h"
#include "platform/mobile/platform_mobile.h"

#include <pwd.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace platform {

// --- capabilities ----------------------------------------------------------

bool supportsExternalProcesses() { return false; }
bool supportsElevation() { return false; }

// --- identity / privilege --------------------------------------------------

// An app never runs as root, and there is nothing to become root for: the
// sandbox, not the uid, is what limits the app.
bool isElevated() { return false; }

std::string elevationMechanism() {
    return "none: mobile apps are sandboxed and have no privilege to escalate to";
}

// Never launches anything. `fille` is not registered on this platform at all,
// so nothing in the shell calls this; it returns false for any code that does.
bool requestElevation(const std::string&, const std::string&, const std::string&) {
    return false;
}

std::string currentUserName() {
    // There is no meaningful login name inside an app sandbox. The desktop
    // answer would be the app's own uid name, which tells the user nothing.
    return "mobile";
}

// --- filesystem / paths ----------------------------------------------------

std::string pathSeparator() { return "/"; }

std::string getExecutableDir() {
    const std::string path = getExecutablePath();
    const size_t pos = path.find_last_of('/');
    return (pos != std::string::npos) ? path.substr(0, pos) : ".";
}

std::string assetPath(const std::string& assetRelativePath) {
    std::error_code ec;
    // getBundleAssetDir() is per-OS: an NSBundle resource path on iOS, the
    // extracted assets directory on Android.
    const fs::path candidates[] = {
        fs::path(getBundleAssetDir()) / assetRelativePath,
        fs::path(getExecutableDir()) / "assets" / assetRelativePath,
    };
    for (const auto& c : candidates) {
        if (fs::exists(c, ec)) return c.lexically_normal().string();
    }
    return std::string();
}

// There is no file manager to reveal a path in, and no way to hand an
// arbitrary sandbox path to another app. Anything the user should be able to
// open has to be presented by the app's own UI.
void openPath(const std::string& path) {
    std::cerr << "open: not available on this platform (" << path
              << " is inside the app sandbox; use the app's own file view)\n";
}

// --- process execution -----------------------------------------------------

// Every one of these is unreachable from a mobile build: the commands that
// would call them are not in the command table, and shell_loop checks
// supportsExternalProcesses() before falling through to spawnProcess. They are
// implemented rather than left out because the contract says every backend
// implements every function - and if a future code path does reach one, it
// gets an honest failure instead of undefined behaviour.

int spawnProcess(const std::vector<std::string>& args,
                 std::istream&, std::ostream&, bool, int, const std::string&) {
    std::cerr << "wandaashell: " << (args.empty() ? "(empty)" : args[0])
              << ": launching external programs is not possible on this platform\n";
    return -1;
}

std::vector<std::string> processListCommand()                   { return {}; }
std::vector<std::string> killProcessCommand(const std::string&) { return {}; }
std::vector<std::string> whichCommand(const std::string&)       { return {}; }
std::vector<std::string> packageManagerCommand(const std::vector<std::string>&) { return {}; }
std::string packageManagerName()                                { return {}; }

// --- console / presentation ------------------------------------------------

// There is no terminal emulator to configure. A mobile build renders its own
// output view, so colour and clearing are the app UI's business, not ours -
// which is also why the background image cannot be a terminal profile setting
// the way it is on Windows.

bool enableAnsiColors() { return false; }

void setConsoleTitle(const std::string&) {}

void clearScreen() {
    // The bridge exposes this to the app, which clears its own output view.
    std::cout << "\033[2J";
}

// --- diagnostics -----------------------------------------------------------

// AVAudioPlayer and the Android media APIs report failures through their own
// return values; neither writes to the terminal.
void silenceAudioDiagnostics() {}

namespace {
std::string g_bundleAssetDir;
}

std::string getBundleAssetDir() { return g_bundleAssetDir; }

void setBundleAssetDir(const std::string& dir) { g_bundleAssetDir = dir; }

} // namespace platform
