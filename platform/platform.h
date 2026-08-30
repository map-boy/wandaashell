#pragma once
//
// platform.h - the abstraction contract every supported OS must implement.
//
// RULE (non-negotiable): no file outside platform/<os>/ may contain an
// OS-specific #ifdef (_WIN32, __linux__, __APPLE__, ...) or an OS-specific
// header (<windows.h>, <unistd.h>, ...). If code elsewhere needs to know
// something about the host OS, it asks a function declared here. If the
// function it needs does not exist, add it here and implement it in every
// platform/<os>/ file - do not reach for an #ifdef.
//
// Every function below must be implemented by every platform backend. Where a
// capability genuinely does not exist on a platform, the backend implements
// the closest honest equivalent and explains the difference in a comment at
// the implementation site - it never silently no-ops.
//
#include <string>
#include <vector>
#include <istream>
#include <ostream>

namespace platform {

// ---------------------------------------------------------------------------
// Identity / privilege
// ---------------------------------------------------------------------------

// True when the current process holds administrative privileges.
// Windows: membership of BUILTIN\Administrators in the process token.
// POSIX:   geteuid() == 0.
bool isElevated();

// Relaunch wandaashell with administrative privileges in a NEW window.
// Returns true if the elevated process was launched (not if it succeeded).
//
// This is deliberately "launch a privileged copy", not "raise privileges of
// this process": no supported OS allows the latter for a running process.
// The mechanism differs per OS (UAC consent vs. a password prompt), so the
// user-visible experience is not identical across platforms - see each
// implementation's comment for exactly what it does.
bool requestElevation(const std::string& exePath,
                      const std::string& args,
                      const std::string& workingDir);

// A one-line human-readable name for this platform's elevation mechanism,
// e.g. "UAC", "pkexec", "osascript / administrator privileges". Used in
// messages so the shell never claims to have done something it did not do.
std::string elevationMechanism();

// Login name of the current user, or "unknown" if it cannot be determined.
std::string currentUserName();

// ---------------------------------------------------------------------------
// Filesystem / paths
// ---------------------------------------------------------------------------

// Directory containing the running executable (no trailing separator).
std::string getExecutableDir();

// Full path of the running executable.
std::string getExecutablePath();

// Native path separator: "\\" on Windows, "/" elsewhere.
std::string pathSeparator();

// Resolve a bundled asset (e.g. "wandaa-voice.mp3") to an absolute path by
// searching the platform's plausible install layouts. Returns an empty string
// when the asset cannot be found anywhere.
std::string assetPath(const std::string& assetRelativePath);

// Reveal a path in the platform's file manager
// (Explorer / Finder / xdg-open). Never blocks.
void openPath(const std::string& path);

// ---------------------------------------------------------------------------
// Process execution
// ---------------------------------------------------------------------------

// Run an external program. `redirectMode` matches Stage::redirectMode:
// 0 = none, 1 = truncate (>), 2 = append (>>). When `pipeInput` is true the
// contents of `in` are fed to the child's stdin. When `redirectMode` is 0 and
// `out` is not the process stdout, the child's stdout is captured into `out`.
// Returns the child's exit status, or -1 if it could not be started.
int spawnProcess(const std::vector<std::string>& args,
                 std::istream& in, std::ostream& out,
                 bool pipeInput, int redirectMode,
                 const std::string& redirectFile);

// argv for listing running processes (Windows: tasklist, POSIX: ps -e ...).
std::vector<std::string> processListCommand();

// argv for force-terminating a process by pid.
std::vector<std::string> killProcessCommand(const std::string& pid);

// argv for locating an executable on PATH (Windows: where, POSIX: which -a).
std::vector<std::string> whichCommand(const std::string& name);

// argv for this platform's system package manager, translating the shell's
// verbs (search|install|list|upgrade|uninstall) to the native tool. Returns an
// empty vector when no supported package manager is present, in which case the
// caller must say so rather than pretending the command ran.
std::vector<std::string> packageManagerCommand(const std::vector<std::string>& verbAndArgs);

// Name of the package manager packageManagerCommand() would use, or "" if none.
std::string packageManagerName();

// ---------------------------------------------------------------------------
// Console / presentation
// ---------------------------------------------------------------------------

// Enable ANSI/VT escape sequence handling on the terminal.
// Returns true if colour output can be expected to work.
bool enableAnsiColors();

// Set the terminal window title, where the platform supports it.
void setConsoleTitle(const std::string& title);

// Clear the terminal screen.
void clearScreen();

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------

// Play a bundled sound asset (resolved via assetPath) without blocking the
// caller. Silently does nothing when the asset or an audio device is missing -
// the voice clip is a flourish and must never take the shell down with it.
void playVoiceAsync(const std::string& assetRelativePath);

// Release any audio resources held by playVoiceAsync. Called at shutdown.
void shutdownAudio();

} // namespace platform
