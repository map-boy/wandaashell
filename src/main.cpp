#include "shell.h"
#include "script_parser.h"
#include "interpreter.h"
#include <fstream>
#include <sstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc > 1) {
        std::ifstream ifs(argv[1], std::ios::binary);
        if (!ifs) {
            std::cerr << "wandaashell: cannot open " << argv[1] << "\n";
            return 1;
        }
        std::ostringstream buf;
        buf << ifs.rdbuf();
        std::string source = buf.str();
        try {
            NodePtr program = parseScript(source);
            Interpreter interp;
            interp.run(program);
        } catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
        return 0;
    }
    runShell();
    return 0;
}