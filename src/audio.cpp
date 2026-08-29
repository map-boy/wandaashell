#include "audio.h"
#include <windows.h>
#include <mmsystem.h>
#include <thread>
#include <string>

static std::string voiceFilePath() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string path(exePath);
    size_t pos = path.find_last_of("\\/");
    std::string dir = (pos != std::string::npos) ? path.substr(0, pos) : ".";
    return dir + "\\assets\\wandaa-voice.mp3";
}

void playVoiceAsync() {
    std::string mp3Path = voiceFilePath();
    std::thread([mp3Path]() {
        mciSendStringA("close wandaavoice", NULL, 0, NULL);
        std::string openCmd = "open \"" + mp3Path + "\" type mpegvideo alias wandaavoice";
        if (mciSendStringA(openCmd.c_str(), NULL, 0, NULL) == 0) {
            mciSendStringA("play wandaavoice", NULL, 0, NULL);
        }
    }).detach();
}