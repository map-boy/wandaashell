#include "external.h"
#include <fstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

int run_external(const std::vector<std::string>& args, std::istream& in, std::ostream& out,
                  bool inFromPipe, int redirectMode, const std::string& redirectFile) {
    std::string full;
    for (auto& a : args) full += a + " ";

    std::string tmpIn, tmpOut;
    bool usedTmpIn = false, usedTmpOut = false;

    if (inFromPipe) {
        tmpIn = "wandaa_stdin.tmp";
        std::ofstream ofs(tmpIn, std::ios::binary);
        ofs << in.rdbuf();
        ofs.close();
        full += "< " + tmpIn + " ";
        usedTmpIn = true;
    }

    bool captureToStream = (&out != &std::cout);
    if (redirectMode == 1) full += "> " + redirectFile + " ";
    else if (redirectMode == 2) full += ">> " + redirectFile + " ";
    else if (captureToStream) {
        tmpOut = "wandaa_stdout.tmp";
        full += "> " + tmpOut + " ";
        usedTmpOut = true;
    }

    int rc = std::system(full.c_str());

    if (usedTmpOut) {
        std::ifstream ifs(tmpOut, std::ios::binary);
        out << ifs.rdbuf();
        ifs.close();
        fs::remove(tmpOut);
    }
    if (usedTmpIn) fs::remove(tmpIn);
    return rc;
}
