//
// audio_miniaudio.cpp - platform::playVoiceAsync / shutdownAudio for every
// desktop platform.
//
// This file lives in platform/ rather than platform/<os>/ on purpose: it is
// the one part of the platform layer that genuinely has a single portable
// implementation. It contains no OS conditionals of its own - miniaudio does
// that work internally and picks WASAPI, ALSA/PulseAudio or CoreAudio at
// runtime. Replacing three OS-specific audio backends with one shared file is
// exactly why miniaudio was vendored.
//
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING   // playback only; nothing here ever writes audio files
#define MA_NO_CAPTURE    // wandaashell never records
#include "third_party/miniaudio.h"

#include "platform/platform.h"

#include <mutex>
#include <thread>

namespace {

std::mutex g_audioMutex;
ma_engine  g_engine;
bool       g_engineReady = false;
bool       g_engineFailed = false;

// Brings the audio engine up on first use. Returns false when there is no
// usable audio device - a headless box, a container, a machine with sound
// disabled - in which case every later call is a cheap no-op.
bool ensureEngine() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (g_engineReady) return true;
    if (g_engineFailed) return false;
    platform::silenceAudioDiagnostics();
    if (ma_engine_init(NULL, &g_engine) != MA_SUCCESS) {
        g_engineFailed = true;
        return false;
    }
    g_engineReady = true;
    return true;
}

} // namespace

namespace platform {

void playVoiceAsync(const std::string& assetRelativePath) {
    const std::string path = assetPath(assetRelativePath);
    if (path.empty()) return;
    // Device init and file decode both block for a few milliseconds; the REPL
    // prompt should not wait on a sound effect.
    std::thread([path]() {
        if (!ensureEngine()) return;
        std::lock_guard<std::mutex> lock(g_audioMutex);
        if (!g_engineReady) return;
        // Fire-and-forget: miniaudio streams and frees the sound itself.
        ma_engine_play_sound(&g_engine, path.c_str(), NULL);
    }).detach();
}

void shutdownAudio() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (!g_engineReady) return;
    ma_engine_uninit(&g_engine);
    g_engineReady = false;
}

} // namespace platform
