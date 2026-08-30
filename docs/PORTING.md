# Porting notes and feature parity

How wandaashell is laid out across platforms, what each platform actually does,
and — importantly — how much of each row below was verified rather than
assumed.

## Layers

```
core/         100% portable C++17. Zero #ifdef, zero platform headers.
              tokenizer, pipeline parser, .waa parser + interpreter,
              REPL loop, version, output capture.

platform/     platform.h is the contract. Exactly one backend is compiled in.
  windows/    Win32: UAC, CreateProcess-era std::system spawning, resource script
  posix/      Shared by Linux and macOS: geteuid, fork/exec, terminal, paths
  linux/      /proc/self/exe, xdg-open, pkexec/sudo, apt/dnf/pacman/zypper/apk
  macos/      _NSGetExecutablePath, open(1), osascript, brew/port
  mobile/     Tier 2: capabilities off, sandbox paths, AVAudioPlayer / MediaPlayer
  audio_miniaudio.cpp   One audio implementation for all three desktop platforms

builtins/     builtins_common.cpp  - filesystem and text, std::filesystem only
              builtins_process.cpp - anything that leaves the process, via platform.h

apps/         desktop_main.cpp   - Windows, Linux and macOS entry point
  mobile/     ios_bridge.mm, android_bridge.cpp
```

**The rule:** no file outside `platform/<os>/` contains `#ifdef _WIN32`,
`__linux__`, `__APPLE__`, `<windows.h>`, `<unistd.h>` or any other
OS-conditional. If something outside the platform layer needs to know about the
host OS, it calls a function in `platform.h`. Wanting an `#ifdef` anywhere else
means `platform.h` is missing a function.

Verify it at any time:

```sh
grep -rn "_WIN32\|__linux__\|__APPLE__\|windows\.h\|unistd\.h" core builtins apps
```

The only expected hits are the comment in `platform.h` that states the rule.

**One documented deviation from the spec's tree.** Section 3 lists only
`platform/{windows,linux,macos}/`. `platform/posix/` exists as well, holding
the fork/exec implementation and everything else Linux and macOS genuinely
share. Duplicating ~120 lines of process handling into two backends is exactly
the drift this layering prevents. That file contains no OS conditional of its
own, which is the test for whether code belongs there.
`platform/audio_miniaudio.cpp` sits at the `platform/` root for the same
reason: one portable implementation, no conditionals.

## Feature parity

Legend: **verified** = built and run; **built** = compiles and links, not run
here; **written** = code exists, needs the platform's own toolchain.

| Feature | Windows | Linux | macOS | iOS / Android (Tier 2) |
|---|---|---|---|---|
| REPL + `.waa` scripting | built (cross-compiled) | **verified** | **verified** (backend run on Linux with one symbol shimmed) | **verified** on Android via JNI; iOS written |
| `cd` `ls` `mkdir` `rm` `cp` `mv` `cat` etc. | built | **verified** | **verified** | **verified** (sandbox storage only) |
| Pipes, `>`, `>>`, `<`, `;` chaining | built | **verified** | **verified** | **verified** |
| External process spawn | built — original `std::system` + temp-file piping, unchanged | **verified** — real `fork`/`exec` with real pipes | **verified** — same POSIX path | **absent by design** — command table omits them |
| `fille` elevation | built — UAC via `ShellExecuteEx` `runas` | written — new terminal under `pkexec`/`sudo`/`doas`, terminal detected at runtime | written — new Terminal.app/iTerm2 window under `sudo` via `osascript` | **absent by design** — no privilege concept exists |
| Package manager (`pkg`/`winget`) | built — winget, pass-through | **verified** — detects apt-get/dnf/pacman/zypper/apk/xbps | written — brew, else MacPorts | **absent by design** |
| Startup / failed-delete voice | built — miniaudio | **verified** — miniaudio, silent with no sound card | written — miniaudio (CoreAudio) | written — `AVAudioPlayer` / `MediaPlayer`; Java callback **verified** |
| Custom icon | **verified** — embedded via `.rc`, 0x158a0 `.rsrc` section | **verified** — `.desktop` + hicolor PNG installed | n/a for a bare CLI binary; needs a `.app` bundle, which this project does not build | app icon asset sets, platform packaging |
| Terminal background image | Windows Terminal profile `backgroundImage` | terminal-emulator dependent, not guaranteed | Terminal.app profile, same caveat | app must draw its own — no terminal involved |

### Honest differences, not gaps

- **Elevation is not the same experience.** Windows shows a UAC consent dialog;
  Linux and macOS prompt for a password in a new terminal window. No supported
  OS lets a running process raise its own privileges, so all three launch a
  privileged copy. On Linux a machine with no terminal emulator installed
  cannot do this at all and `requestElevation` returns false rather than
  pretending it launched something.
- **Windows piping still uses temp files.** That is the original
  implementation, kept deliberately: every existing wandaashell script was
  written against its behaviour. The POSIX backends use real pipes, with the
  child's stdin written on a separate thread so a child that fills the output
  pipe mid-write cannot deadlock.
- **`ps` output differs by design.** `tasklist` on Windows,
  `ps -e -o pid,ppid,stat,comm` on POSIX. The shell does not reformat them into
  a fake common shape.
- **Package manager verbs are translated, not invented.** `apt-get` has no
  `search` verb, so `pkg search` routes to `apt-cache`; an unknown verb or an
  unsupported operation reports that rather than running something else.

### Known pre-existing quirks, unchanged by the port

These behave identically on every platform, which is what parity means. They
predate this work and were left alone rather than quietly changed:

- Built-ins shadow external programs, so `grep -c foo` runs the built-in `grep`
  with `-c` as the pattern, not GNU grep.
- `cat` requires a filename argument and ignores `<` input redirection.

## Building

```sh
cmake -B build && cmake --build build          # any of the three desktop OSes
./tests/run_tests.sh                           # then always run this
```

Cross-compiling Windows from Linux, which is how the Windows build was checked:

```sh
cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-w64.cmake
cmake --build build-win
```

The Tier 2 shared library, for CI on any machine with a JDK:

```sh
cmake -B build-mobile -DWANDAASHELL_BUILD_MOBILE_CORE=ON
cmake --build build-mobile --target wandaashell_mobile
```

## What a new platform backend has to do

1. Implement every function in `platform/platform.h`. Where a capability
   genuinely does not exist, implement the closest honest equivalent and say so
   in a comment at the implementation site — never a silent stub.
2. Add the branch to `CMakeLists.txt`.
3. Run `./tests/run_tests.sh` and get 4/4. **That, not compiling, is what
   "ported" means.**
4. Fill in the column above with what you verified, not what you expect.
