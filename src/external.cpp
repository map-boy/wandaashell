#include "external.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <fstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

int run_external(const std::vector<std::string>& args, std::istream& in, std::ostream& out,
                  bool inFromPipe, int redirectMode, const std::string& redirectFile) {
    std::vector<std::string> realArgs = args;
    if (!realArgs.empty()) {
        std::string cmd0 = realArgs[0];
        std::string lower = cmd0;
        for (auto& c : lower) c = (char)tolower((unsigned char)c);
        if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".ps1") {
            std::vector<std::string> wrapped = {"powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", cmd0};
            for (size_t i = 1; i < realArgs.size(); ++i) wrapped.push_back(realArgs[i]);
            realArgs = wrapped;
        }
    }
    std::string full;
    for (auto& a : realArgs) full += a + " ";

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
#ifdef _WIN32
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE) {
        SetConsoleMode(hIn, ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_INSERT_MODE | ENABLE_EXTENDED_FLAGS | ENABLE_QUICK_EDIT_MODE);
    }
    std::cin.clear();
#endif

    if (usedTmpOut) {
        std::ifstream ifs(tmpOut, std::ios::binary);
        out << ifs.rdbuf();
        ifs.close();
        fs::remove(tmpOut);
    }
    if (usedTmpIn) fs::remove(tmpIn);
    return rc;
}
