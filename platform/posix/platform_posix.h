#pragma once
//
// Helpers shared between the POSIX platform backends (Linux, macOS). These are
// deliberately not in platform/platform.h: nothing in core/ or builtins/ may
// call them, because they have no Windows counterpart.
//
#include <string>
#include <vector>

namespace platform {

// True when `name` is an executable on PATH (or an executable path itself).
bool commandExists(const std::string& name);

// Launch argv without waiting for it, detached from this process group.
// Returns true if the launch itself succeeded.
bool spawnDetachedProcess(const std::vector<std::string>& args);

} // namespace platform
