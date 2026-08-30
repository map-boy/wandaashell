# Tier 2 — the mobile companion app

**wandaashell on iOS and Android is not the shell. It is a companion app that
shares the shell's language and built-in commands, and it never gains the
ability to run other programs.**

That sentence is the whole point of this document. If you are writing a store
listing, a README section or a release note for a mobile build, say that
plainly. Users who install a thing called a "shell" and discover it cannot run
`curl` will file bugs about a missing feature; the honest framing prevents a
support burden that is otherwise permanent.

## Why, precisely

- **iOS.** App Store apps cannot `fork`/`exec` arbitrary binaries or spawn a
  shell process. This is enforced by the OS, not by a policy that a
  well-written app can work around, and not a gap that will be filled by a
  future SDK. There is no workaround to find.
- **Android.** The OS itself is more permissive — an app *can* exec a binary it
  ships. But Google Play policy and ordinary distribution rule out executing
  arbitrary or dynamically-fetched binaries, and an app that shipped a set of
  Unix tools to exec would be shipping a different product with a different
  review risk. This build does not do it.
- **Privilege.** Neither platform has a privilege-escalation concept available
  to an app. The sandbox, not the uid, is what limits what the app can reach,
  and nothing an app calls changes the sandbox.

## What the mobile build has

Everything in `core/` and the filesystem half of `builtins/`, compiled from the
same source files the desktop shell uses — no mobile fork, no `#ifdef`:

- the tokenizer, the pipeline parser (`|`, `>`, `>>`, `<`), `;` chaining
- the whole `.waa` language: variables, `print()`, `shell()`, `if`, `loop`
- `cd` `pwd` `echo` `ls` `dir` `mkdir` `rmdir` `rm` `cp` `mv` `touch` `cat`
  `find` `grep` `grepn` `insertafter` `set` `env` `history` `alias` `which`
  `date` `whoami` `clear` `hexcat` `b64encode` `b64decode` `encrypt` `decrypt`
  `waa` `help` `run`
- the startup and failed-delete voice clip, through `AVAudioPlayer` on iOS and
  `MediaPlayer` on Android

All filesystem access is confined to the app's own sandbox storage — the
Documents directory on iOS, the private files directory on Android. `pwd`
reports that directory at launch.

## What the mobile build does not have, and will not get

| Dropped | Why |
|---|---|
| `ps`, `kill` | Requires enumerating and signalling other processes. Not available to a sandboxed app. |
| `curl`, `wget` | These shell out to a system binary. See "the curl question" below. |
| `pkg` / `winget` | There is no system package manager to drive. |
| `disasm` | Shells out to `objdump`. |
| `fille` | No privilege escalation concept exists. Dropped rather than made a no-op. |
| Fall-through to external programs | Unknown commands report that this build runs built-ins only. |
| Terminal background image | There is no terminal emulator to configure. If a mobile build wants a background image it must be drawn by the app's own UI. |

These are **absent from the command table**, not registered as stubs. `help`
does not list them and `which` does not claim they exist. That is enforced in
one place — `platform::supportsExternalProcesses()` and
`platform::supportsElevation()` in `platform/mobile/platform_mobile_common.cpp`
— and read by `builtins/builtins_common.cpp` and `core/shell_loop.cpp`.

## The curl question

The spec's Tier 2 section offers two honest options for a command currently
implemented by shelling out: reimplement it natively in-process, or drop it
with a clear message. `curl` and `wget` are **dropped** in this pass.

Doing it properly means a native HTTP client behind a new platform function —
`NSURLSession` on iOS, `HttpURLConnection` or OkHttp over JNI on Android — with
the desktop backends implementing the same function so behaviour matches. That
is a real piece of work with its own error-handling, redirect, timeout and TLS
surface, and it is the obvious next Tier 2 task. It is not started, and nothing
in the tree pretends it is.

## What is in this repository, and what is not

In the repository:

- `platform/mobile/platform_mobile_common.cpp` — the Tier 2 platform backend
- `platform/mobile/platform_ios.mm` — `_NSGetExecutablePath`, `AVAudioPlayer`
- `platform/mobile/platform_android.cpp` — `/proc/self/exe`, audio via a JNI
  callback into Java
- `apps/mobile/ios_bridge.mm` — the `WandaaShell` Objective-C class an iOS app
  talks to
- `apps/mobile/android_bridge.cpp` — the JNI entry points an Android app calls
- `core/output_capture.h` — returns printed text to the app instead of writing
  to a terminal that does not exist

Not in the repository, and deliberately so: the Xcode project and the
Gradle/NDK build. Neither can be built or verified on a Linux CI machine, and
an unbuildable project file that drifts out of date is worse than none. The
`WANDAASHELL_BUILD_MOBILE_CORE` CMake option builds the shared C++ half as a
JNI library so it is compiled and checked everywhere, which is the part that
can actually rot silently.

## Wiring up an app

**Android.** The Java side must be `com.wandaa.shell.WandaaShell` (the JNI
symbol names encode it) and declare:

```java
public static native void     nativeInit(String assetDir, String workingDir);
public static native String   nativeRunLine(String line);
public static native String   nativeRunScript(String source);
public static native String[] nativeBuiltinNames();
public static native String   nativeVersion();
public static native void     nativePlayStartupVoice();
public static native void     nativeShutdown();

// called back from native code; there is no NDK API worth using for a
// one-shot MP3, so audio goes back through Java
static void playVoice(String path) { /* MediaPlayer */ }
static void stopVoice()            { /* MediaPlayer.stop/release */ }
```

Call `nativeInit` once with the directory the app extracted `assets/` into and
its private files directory. Every `nativeRun*` call returns the text the shell
printed, `stdout` and `stderr` together.

**iOS.** Instantiate `WandaaShell` (from `ios_bridge.mm`) per output view;
`-runLine:` and `-runScript:` return the printed text as an `NSString`. The
initialiser locates the bundle's assets and changes to the Documents directory.
Add `platform_ios.mm`, `platform_mobile_common.cpp`, `core/*.cpp` and the
filesystem half of `builtins/*.cpp` to the app target.

Neither bridge is thread-safe: capturing output swaps a process-global stream
buffer, so drive one instance from one queue.

## Versioning

Ship mobile builds under their own version line and their own release notes,
separate from the desktop shell, so a user reading "wandaashell 0.6.0" for iOS
is not reading the desktop feature list.
