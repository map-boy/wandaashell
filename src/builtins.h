#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include "types.h"

std::unordered_map<std::string, CommandFn> makeBuiltins();
void pushHistory(const std::string& line);
void resolveAlias(std::vector<std::string>& tokens);
void markShellStart();
