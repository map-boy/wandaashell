//
// platform_android.cpp - the two things Android does differently from iOS
// within the Tier 2 backend: finding the executable, and playing a sound.
//
// Everything else in the mobile contract is in platform_mobile_common.cpp.
//
#include "platform/platform.h"
#include "platform/mobile/platform_mobile.h"

#include <jni.h>
#include <limits.h>
#include <unistd.h>

#include <string>

namespace platform {
namespace android {

// Set by the bridge in JNI_OnLoad. Audio has to go back through Java because
// MediaPlayer and SoundPool have no NDK equivalent worth using for a one-shot
// clip: AAudio and OpenSL ES are streaming APIs that would mean decoding the
// MP3 ourselves.
JavaVM*  g_vm = nullptr;
jclass   g_shellClass = nullptr;      // global ref to the app's WandaaShell class
jmethodID g_playVoiceMethod = nullptr;   // static void playVoice(String path)
jmethodID g_stopVoiceMethod = nullptr;   // static void stopVoice()

namespace {

// JavaVM::AttachCurrentThread takes JNIEnv** in the Android NDK and void** in
// desktop JDK headers. Deducing the parameter type from the member pointer
// keeps this file compiling against both - which matters because it lets the
// bridge be syntax-checked on a build machine that has a JDK but no NDK.
template <typename EnvPtrPtr>
jint attachCurrentThread(JavaVM* vm, jint (JavaVM::*fn)(EnvPtrPtr, void*), JNIEnv** env) {
    return (vm->*fn)(reinterpret_cast<EnvPtrPtr>(env), nullptr);
}

// Attaches the calling thread to the JVM if it is not already attached, and
// detaches again on scope exit only if we were the ones who attached it.
class JniScope {
public:
    JniScope() {
        if (!g_vm) return;
        const jint status = g_vm->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6);
        if (status == JNI_EDETACHED) {
            if (attachCurrentThread(g_vm, &JavaVM::AttachCurrentThread, &env_) == JNI_OK)
                attached_ = true;
            else
                env_ = nullptr;
        } else if (status != JNI_OK) {
            env_ = nullptr;
        }
    }
    ~JniScope() { if (attached_ && g_vm) g_vm->DetachCurrentThread(); }

    JniScope(const JniScope&) = delete;
    JniScope& operator=(const JniScope&) = delete;

    JNIEnv* env() const { return env_; }

private:
    JNIEnv* env_ = nullptr;
    bool attached_ = false;
};

} // namespace
} // namespace android

std::string getExecutablePath() {
    // Android is Linux underneath, so this is the same call the Linux backend
    // makes. It points at the app process binary, not at anything the user
    // could run - the path is only used to anchor asset lookup.
    char buf[PATH_MAX];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return std::string();
    buf[n] = '\0';
    return std::string(buf);
}

void playVoiceAsync(const std::string& assetRelativePath) {
    const std::string path = assetPath(assetRelativePath);
    if (path.empty()) return;
    if (!android::g_shellClass || !android::g_playVoiceMethod) return;

    android::JniScope scope;
    JNIEnv* env = scope.env();
    if (!env) return;

    jstring jPath = env->NewStringUTF(path.c_str());
    if (!jPath) return;
    // MediaPlayer.start() is itself asynchronous, so this returns immediately.
    env->CallStaticVoidMethod(android::g_shellClass, android::g_playVoiceMethod, jPath);
    if (env->ExceptionCheck()) env->ExceptionClear();   // a silent clip, never a crash
    env->DeleteLocalRef(jPath);
}

void shutdownAudio() {
    if (!android::g_shellClass || !android::g_stopVoiceMethod) return;
    android::JniScope scope;
    JNIEnv* env = scope.env();
    if (!env) return;
    env->CallStaticVoidMethod(android::g_shellClass, android::g_stopVoiceMethod);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

} // namespace platform
