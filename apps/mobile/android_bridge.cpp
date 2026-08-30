//
// android_bridge.cpp - the JNI surface an Android app talks to.
//
// SCOPE. This is the Tier 2 companion app bridge, not the desktop shell. The
// app gets the same tokenizer, pipeline parser, .waa interpreter and
// filesystem built-ins that the desktop shell runs - and nothing that needs to
// launch a program or escalate privilege. Android permits subprocesses at the
// OS level, but Play policy and normal distribution rule out running arbitrary
// or dynamically fetched binaries, so this build does not offer them.
// See docs/TIER2_MOBILE.md before adding anything to this file.
//
// Java side (com.wandaa.shell.WandaaShell) must declare:
//
//   static native void   nativeInit(String assetDir, String workingDir);
//   static native String nativeRunLine(String line);
//   static native String nativeRunScript(String source);
//   static native String[] nativeBuiltinNames();
//   static native String nativeVersion();
//   static native void   nativePlayStartupVoice();
//   static native void   nativeShutdown();
//
//   // called back from native code for audio - no NDK equivalent is worth
//   // using for a one-shot clip
//   static void playVoice(String path) { ... MediaPlayer ... }
//   static void stopVoice() { ... }
//
#include "core/shell_loop.h"
#include "core/script_parser.h"
#include "core/interpreter.h"
#include "core/output_capture.h"
#include "core/version.h"
#include "builtins/builtins.h"
#include "builtins/builtins_internal.h"
#include "platform/platform.h"
#include "platform/mobile/platform_mobile.h"

#include <jni.h>

#include <exception>
#include <filesystem>
#include <string>

// Set in platform_android.cpp; populated here because JNI_OnLoad is the only
// place the JavaVM and the app's class are available.
namespace platform { namespace android {
extern JavaVM*   g_vm;
extern jclass    g_shellClass;
extern jmethodID g_playVoiceMethod;
extern jmethodID g_stopVoiceMethod;
} }

namespace {

constexpr const char* kShellClassName = "com/wandaa/shell/WandaaShell";

std::string toStdString(JNIEnv* env, jstring js) {
    if (!js) return std::string();
    const char* chars = env->GetStringUTFChars(js, nullptr);
    if (!chars) return std::string();
    std::string result(chars);
    env->ReleaseStringUTFChars(js, chars);
    return result;
}

} // namespace

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    platform::android::g_vm = vm;

    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    // The class is looked up here, on the loader thread, because a background
    // thread attached later cannot see app classes through FindClass.
    jclass localClass = env->FindClass(kShellClassName);
    if (localClass) {
        platform::android::g_shellClass =
            static_cast<jclass>(env->NewGlobalRef(localClass));
        env->DeleteLocalRef(localClass);
        platform::android::g_playVoiceMethod = env->GetStaticMethodID(
            platform::android::g_shellClass, "playVoice", "(Ljava/lang/String;)V");
        platform::android::g_stopVoiceMethod = env->GetStaticMethodID(
            platform::android::g_shellClass, "stopVoice", "()V");
    }
    // A missing class or method means no voice clip, never a failed load.
    if (env->ExceptionCheck()) env->ExceptionClear();

    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL
Java_com_wandaa_shell_WandaaShell_nativeInit(JNIEnv* env, jclass,
                                             jstring assetDir, jstring workingDir) {
    platform::setBundleAssetDir(toStdString(env, assetDir));
    // The app's private files directory is the only place it can write, so
    // that is what `pwd` should report on launch.
    const std::string cwd = toStdString(env, workingDir);
    if (!cwd.empty()) {
        std::error_code ec;
        std::filesystem::current_path(cwd, ec);   // on failure, stay put
    }
    markShellStart();
}

JNIEXPORT jstring JNICALL
Java_com_wandaa_shell_WandaaShell_nativeRunLine(JNIEnv* env, jclass, jstring line) {
    const std::string input = toStdString(env, line);
    if (input.empty()) return env->NewStringUTF("");
    std::string output;
    {
        OutputCapture capture;
        runShellLine(input);
        output = capture.text();
    }
    return env->NewStringUTF(output.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_wandaa_shell_WandaaShell_nativeRunScript(JNIEnv* env, jclass, jstring source) {
    const std::string src = toStdString(env, source);
    std::string output;
    {
        OutputCapture capture;
        try {
            NodePtr program = parseScript(src);
            Interpreter interp;
            interp.run(program);
        } catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
        }
        output = capture.text();
    }
    return env->NewStringUTF(output.c_str());
}

// Commands this build actually has. Excludes everything the sandbox rules out,
// so the app can build a command bar from it without offering commands that
// cannot work.
JNIEXPORT jobjectArray JNICALL
Java_com_wandaa_shell_WandaaShell_nativeBuiltinNames(JNIEnv* env, jclass) {
    const auto& names = builtinNames();
    jclass stringClass = env->FindClass("java/lang/String");
    if (!stringClass) return nullptr;
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(names.size()),
                                             stringClass, nullptr);
    if (!array) return nullptr;
    for (jsize i = 0; i < static_cast<jsize>(names.size()); ++i) {
        jstring item = env->NewStringUTF(names[static_cast<size_t>(i)].c_str());
        env->SetObjectArrayElement(array, i, item);
        env->DeleteLocalRef(item);
    }
    return array;
}

JNIEXPORT jstring JNICALL
Java_com_wandaa_shell_WandaaShell_nativeVersion(JNIEnv* env, jclass) {
    return env->NewStringUTF(WANDAASHELL_VERSION);
}

JNIEXPORT void JNICALL
Java_com_wandaa_shell_WandaaShell_nativePlayStartupVoice(JNIEnv*, jclass) {
    platform::playVoiceAsync("wandaa-voice.mp3");
}

JNIEXPORT void JNICALL
Java_com_wandaa_shell_WandaaShell_nativeShutdown(JNIEnv*, jclass) {
    platform::shutdownAudio();
}

} // extern "C"
