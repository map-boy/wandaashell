#pragma once
#include <vector>
#include <string>
#include <functional>
#include <istream>
#include <ostream>

using CommandFn = std::function<int(const std::vector<std::string>&, std::istream&, std::ostream&)>;

struct Stage {
    std::vector<std::string> args;
    int redirectMode = 0;      // 0 none, 1 truncate(>), 2 append(>>)
    std::string redirectFile;
    bool hasInputRedirect = false;
    std::string inputFile;
};
