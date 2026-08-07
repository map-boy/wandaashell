#pragma once
#include <unordered_map>
#include <string>
#include "types.h"

std::unordered_map<std::string, CommandFn> makeBuiltins();
