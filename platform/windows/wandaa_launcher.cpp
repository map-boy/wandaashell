#include <windows.h>
#include <string>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string path(exePath);
    size_t pos = path.find_last_of("\\/");
    std::string dir = (pos != std::string::npos) ? path.substr(0, pos) : ".";
    std::string target = dir + "\\wandaashell.exe";

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    std::string cmdLine = "\"" + target + "\"";
    BOOL ok = CreateProcessA(
        NULL,
        &cmdLine[0],
        NULL, NULL, FALSE,
        CREATE_NEW_CONSOLE,
        NULL,
        dir.c_str(),
        &si, &pi
    );

    if (!ok) {
        MessageBoxA(NULL, "Could not find wandaashell.exe next to wandaa.exe", "wandaa launcher", MB_OK | MB_ICONERROR);
        return 1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}