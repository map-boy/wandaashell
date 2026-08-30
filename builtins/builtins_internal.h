#pragma once
//
// Declarations shared between the builtins translation units. Only builtins
// that live in builtins_process.cpp need to be visible to the registry in
// builtins_common.cpp; everything else stays file-static.
//
#include <istream>
#include <ostream>
#include <string>
#include <vector>

int cmd_which(const std::vector<std::string>& args, std::istream& in, std::ostream& out);
int cmd_ps(const std::vector<std::string>& args, std::istream& in, std::ostream& out);
int cmd_kill(const std::vector<std::string>& args, std::istream& in, std::ostream& out);
int cmd_curl(const std::vector<std::string>& args, std::istream& in, std::ostream& out);
int cmd_wget(const std::vector<std::string>& args, std::istream& in, std::ostream& out);
int cmd_pkg(const std::vector<std::string>& args, std::istream& in, std::ostream& out);
int cmd_disasm(const std::vector<std::string>& args, std::istream& in, std::ostream& out);

// Names of every registered built-in, for `which`.
const std::vector<std::string>& builtinNames();
