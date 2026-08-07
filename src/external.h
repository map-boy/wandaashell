#pragma once
#include <vector>
#include <string>
#include <istream>
#include <ostream>

int run_external(const std::vector<std::string>& args, std::istream& in, std::ostream& out,
                  bool inFromPipe, int redirectMode, const std::string& redirectFile);
