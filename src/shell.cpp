#include "shell.h"
#include "types.h"
#include "tokenizer.h"
#include "pipeline.h"
#include "builtins.h"
#include "external.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

void runShell() {
    auto builtins = makeBuiltins();

    std::cout << "wandaashell v0.3\n";
    std::string line;
    while (true) {
        std::cout << "wandaa " << fs::current_path().string() << " > ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        auto tokens = tokenize(line);
        if (tokens.empty()) continue;
        if (tokens[0] == "exit" || tokens[0] == "quit") break;

        auto stages = parsePipeline(tokens);
        if (stages.empty()) continue;

        std::ostringstream carry;
        bool havePrevOutput = false;

        for (size_t i = 0; i < stages.size(); ++i) {
            Stage& s = stages[i];
            if (s.args.empty()) continue;
            bool isLast = (i + 1 == stages.size());

            std::istringstream in(havePrevOutput ? carry.str() : "");
            std::istream* inStream = &in;
            std::ifstream fileIn;
            if (s.hasInputRedirect) {
                fileIn.open(s.inputFile, std::ios::binary);
                if (!fileIn) { std::cerr << "wandaashell: cannot open " << s.inputFile << "\n"; break; }
                inStream = &fileIn;
            }

            std::ostringstream nextCarry;
            std::ofstream fileOut;
            std::ostream* outStream = isLast ? static_cast<std::ostream*>(&std::cout)
                                              : static_cast<std::ostream*>(&nextCarry);

            if (isLast && s.redirectMode != 0) {
                fileOut.open(s.redirectFile, s.redirectMode == 2 ? std::ios::app : std::ios::trunc);
                if (!fileOut) { std::cerr << "wandaashell: cannot open " << s.redirectFile << "\n"; break; }
                outStream = &fileOut;
            }

            auto it = builtins.find(s.args[0]);
            if (it != builtins.end()) {
                it->second(s.args, *inStream, *outStream);
            } else {
                run_external(s.args, *inStream, *outStream,
                             havePrevOutput || s.hasInputRedirect,
                             isLast ? s.redirectMode : 0,
                             isLast ? s.redirectFile : "");
            }

            if (!isLast) {
                carry.str(nextCarry.str());
                havePrevOutput = true;
            }
        }
    }
}
