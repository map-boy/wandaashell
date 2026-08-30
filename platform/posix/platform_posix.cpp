//
// platform_posix.cpp - the parts of the platform contract that are identical
// on every POSIX host.
//
// Linux and macOS agree on process control, privilege checks, terminal
// handling and path shape; they differ on how you find your own executable,
// how you raise privileges, how you open a file manager and which package
// manager exists. Those four live in platform/linux/ and platform/macos/;
// everything else lives here so the two backends cannot drift apart.
//
// There is not a single OS conditional in this file - that is the test for
// whether something belongs here rather than in a per-OS backend.
//
#include "platform/platform.h"
#include "platform/posix/platform_posix.h"

#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace platform {

// --- identity / privilege --------------------------------------------------

bool isElevated() {
    return geteuid() == 0;
}

std::string currentUserName() {
    if (const char* user = std::getenv("USER")) return user;
    if (const char* logname = std::getenv("LOGNAME")) return logname;
    if (const struct passwd* pw = getpwuid(geteuid())) return pw->pw_name;
    return "unknown";
}

// --- filesystem / paths ----------------------------------------------------
//
// getExecutablePath() is per-OS (/proc/self/exe vs _NSGetExecutablePath), so
// it is not here - but everything derived from it is.

std::string pathSeparator() { return "/"; }

std::string getExecutableDir() {
    const std::string path = getExecutablePath();
    const size_t pos = path.find_last_of('/');
    return (pos != std::string::npos) ? path.substr(0, pos) : ".";
}

std::string assetPath(const std::string& assetRelativePath) {
    std::error_code ec;
    const fs::path dir = getExecutableDir();
    // Search order: next to the binary (build tree and portable layouts),
    // then the installed layout (bin/../share/wandaashell/assets), then a
    // build subdirectory, then the working directory.
    const fs::path candidates[] = {
        dir / "assets" / assetRelativePath,
        dir / ".." / "share" / "wandaashell" / "assets" / assetRelativePath,
        dir / ".." / "assets" / assetRelativePath,
        fs::current_path(ec) / "assets" / assetRelativePath,
    };
    for (const auto& c : candidates) {
        if (fs::exists(c, ec)) return c.lexically_normal().string();
    }
    return std::string();
}

// --- process execution -----------------------------------------------------

namespace {

// Launch argv detached from this process: double-fork so the grandchild is
// reparented to init and we never leave a zombie behind, and setsid so it
// survives the shell exiting. Used for "open a window and forget about it"
// actions (file manager, elevated relaunch).
bool spawnDetached(const std::vector<std::string>& args) {
    if (args.empty()) return false;
    pid_t first = fork();
    if (first < 0) return false;
    if (first == 0) {
        pid_t second = fork();
        if (second < 0) _exit(127);
        if (second > 0) _exit(0);
        setsid();
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        // Detached GUI helpers should not scribble on the shell's terminal.
        int devnull = ::open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            if (devnull > STDERR_FILENO) close(devnull);
        }
        execvp(argv[0], argv.data());
        _exit(127);
    }
    int status = 0;
    waitpid(first, &status, 0);
    return true;
}

} // namespace

bool commandExists(const std::string& name) {
    if (name.empty()) return false;
    if (name.find('/') != std::string::npos) return access(name.c_str(), X_OK) == 0;
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) return false;
    std::string paths(pathEnv);
    size_t start = 0;
    while (start <= paths.size()) {
        size_t sep = paths.find(':', start);
        if (sep == std::string::npos) sep = paths.size();
        std::string dir = paths.substr(start, sep - start);
        if (dir.empty()) dir = ".";
        if (access((dir + "/" + name).c_str(), X_OK) == 0) return true;
        if (sep == paths.size()) break;
        start = sep + 1;
    }
    return false;
}

bool spawnDetachedProcess(const std::vector<std::string>& args) {
    return spawnDetached(args);
}

// Real fork/exec with real pipes - no temp files, unlike the Windows backend
// which kept its original std::system implementation.
int spawnProcess(const std::vector<std::string>& args,
                 std::istream& in, std::ostream& out,
                 bool pipeInput, int redirectMode,
                 const std::string& redirectFile) {
    if (args.empty()) return -1;

    const bool captureOut = (redirectMode == 0) && (&out != &std::cout);
    const bool fileOut = (redirectMode == 1 || redirectMode == 2);

    int inPipe[2] = {-1, -1};
    int outPipe[2] = {-1, -1};
    int errPipe[2] = {-1, -1};   // carries execvp's errno back to the parent

    if (pipeInput && pipe(inPipe) != 0) return -1;
    if (captureOut && pipe(outPipe) != 0) return -1;
    if (pipe(errPipe) != 0) return -1;
    fcntl(errPipe[1], F_SETFD, FD_CLOEXEC);

    int outFd = -1;
    if (fileOut) {
        const int flags = O_WRONLY | O_CREAT | (redirectMode == 2 ? O_APPEND : O_TRUNC);
        outFd = ::open(redirectFile.c_str(), flags, 0644);
        if (outFd < 0) {
            std::cerr << "wandaashell: cannot open " << redirectFile << ": "
                      << std::strerror(errno) << "\n";
            return -1;
        }
    }

    const pid_t pid = fork();
    if (pid < 0) {
        if (outFd >= 0) close(outFd);
        return -1;
    }

    if (pid == 0) {
        if (pipeInput) { dup2(inPipe[0], STDIN_FILENO); close(inPipe[0]); close(inPipe[1]); }
        if (captureOut) { dup2(outPipe[1], STDOUT_FILENO); close(outPipe[0]); close(outPipe[1]); }
        if (outFd >= 0) { dup2(outFd, STDOUT_FILENO); close(outFd); }
        close(errPipe[0]);

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        const int err = errno;
        ssize_t ignored = write(errPipe[1], &err, sizeof(err));
        (void)ignored;
        _exit(127);
    }

    // Parent.
    close(errPipe[1]);
    if (pipeInput) close(inPipe[0]);
    if (captureOut) close(outPipe[1]);
    if (outFd >= 0) close(outFd);

    // Feeding stdin and draining stdout have to happen concurrently, or a
    // child that fills the output pipe while we are still writing its input
    // deadlocks both sides.
    std::thread writer;
    if (pipeInput) {
        const int fd = inPipe[1];
        std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        writer = std::thread([fd, data]() {
            // A child that exits early leaves us writing to a closed pipe;
            // that is a normal end of input, not a reason to die on SIGPIPE.
            signal(SIGPIPE, SIG_IGN);
            size_t written = 0;
            while (written < data.size()) {
                ssize_t n = write(fd, data.data() + written, data.size() - written);
                if (n <= 0) break;
                written += static_cast<size_t>(n);
            }
            close(fd);
        });
    }

    if (captureOut) {
        char buf[4096];
        ssize_t n;
        while ((n = read(outPipe[0], buf, sizeof(buf))) > 0) out.write(buf, n);
        close(outPipe[0]);
    }

    if (writer.joinable()) writer.join();

    int execErrno = 0;
    ssize_t got = read(errPipe[0], &execErrno, sizeof(execErrno));
    close(errPipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (got == static_cast<ssize_t>(sizeof(execErrno))) {
        std::cerr << "wandaashell: " << args[0] << ": " << std::strerror(execErrno) << "\n";
        return 127;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return status;
}

std::vector<std::string> processListCommand() {
    return {"ps", "-e", "-o", "pid,ppid,stat,comm"};
}

std::vector<std::string> killProcessCommand(const std::string& pid) {
    return {"kill", "-9", pid};
}

std::vector<std::string> whichCommand(const std::string& name) {
    // `command -v` is in POSIX; which(1) is not, and minimal images drop it.
    return {"sh", "-c", "command -v -- \"$0\"", name};
}

// --- console / presentation ------------------------------------------------

bool enableAnsiColors() {
    // Nothing to enable: POSIX terminals handle VT sequences natively. We only
    // report whether emitting them is a good idea.
    if (!isatty(STDOUT_FILENO)) return false;
    const char* term = std::getenv("TERM");
    if (term && std::string(term) == "dumb") return false;
    return true;
}

void setConsoleTitle(const std::string& title) {
    if (!isatty(STDOUT_FILENO)) return;
    // OSC 0: set both icon name and window title. Terminals that do not
    // understand it ignore it.
    std::cout << "\033]0;" << title << "\007" << std::flush;
}

void clearScreen() {
    if (!isatty(STDOUT_FILENO)) return;
    // Cursor home, erase screen, erase scrollback - what `clear` does.
    std::cout << "\033[H\033[2J\033[3J" << std::flush;
}

} // namespace platform
