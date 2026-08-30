# wandaashell

A custom command-line shell written in C++17, from scratch. Runs on Windows,
Linux and macOS from one codebase, with a separate mobile companion app.

```
wandaa C:\projects > echo hello wandaa | grep hello
hello wandaa
```

## Features

- REPL with a custom prompt that turns red when the shell is elevated
- ~40 built-in commands: `cd` `pwd` `echo` `ls` `mkdir` `rmdir` `rm` `cp` `mv`
  `touch` `cat` `clear` `whoami` `date` `open` `find` `grep` `grepn`
  `insertafter` `set` `env` `history` `alias` `which` `ps` `kill` `curl` `wget`
  `pkg` `hexcat` `b64encode` `b64decode` `encrypt` `decrypt` `disasm` `run`
  `fille` `waa` `help`
- Unknown commands fall through to external programs
- I/O redirection `>` `>>` `<`, pipes `cmd1 | cmd2 | cmd3`, and `;` statement
  chaining, all mixing built-ins and external commands freely
- The `.waa` scripting language: variables, `print()`, `shell()`, `if`/`else`,
  `loop`
- `pkg` drives the host's package manager — winget, apt, dnf, pacman, zypper,
  apk or Homebrew — through one set of verbs

## Build

Requires CMake 3.20+ and a C++17 compiler (MSVC or MinGW, gcc, or clang).

```sh
cmake -B build
cmake --build build
./tests/run_tests.sh
```

That one pair of commands produces the right binary on whichever OS it runs
on. On Windows it also builds `wandaa.exe`, a launcher stub that opens the
shell in a new console.

Cross-compiling the Windows build from Linux:

```sh
cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-w64.cmake
cmake --build build-win
```

## Run

```sh
./build/wandaashell              # interactive
./build/wandaashell script.waa   # run a .waa script
```

## Usage

```
pwd
echo hello wandaa
echo hello wandaa > out.txt
pwd >> out.txt
cat out.txt | grep wandaa
echo a ; echo b ; echo c
run examples_test.waa
exit
```

## Platform support

**Tier 1 — the full shell: Windows, Linux, macOS.** Real REPL, real external
process spawning, real filesystem access, real elevation, real audio, real
`.waa` scripting. One core, one thin platform backend per OS.

**Tier 2 — the mobile companion: iOS, Android.** Same tokenizer, pipeline
parser, `.waa` interpreter and filesystem built-ins, and **no ability to run
external programs or escalate privilege** — an app sandbox cannot do either, on
either platform. `ps`, `kill`, `curl`, `wget`, `pkg`, `disasm` and `fille` are
absent from the command table rather than stubbed. Read
[docs/TIER2_MOBILE.md](docs/TIER2_MOBILE.md) before describing a mobile build
to anyone; it is a companion app, not the shell.

See [docs/PORTING.md](docs/PORTING.md) for the per-feature parity matrix,
including which rows were verified by running and which by building.

## Project structure

```
core/       portable C++17: tokenizer, pipeline, .waa parser + interpreter, REPL
platform/   platform.h contract + one backend per OS + shared miniaudio player
builtins/   builtins_common.cpp (filesystem, text), builtins_process.cpp (external)
apps/       desktop_main.cpp, mobile/ bridges
assets/     icon, background image, voice clip, Windows resource script
tests/      run_tests.sh - the acceptance suite every platform must pass
third_party/ miniaudio (vendored, unmodified)
```

No file outside `platform/<os>/` contains an OS conditional or an OS header.
That rule is what makes each new platform additive instead of a fork.

## Roadmap

- [x] Redirection, pipes, `;` chaining
- [x] Tokenizer with quoting and escaping
- [x] `.waa` scripting: variables, `if`/`loop`, `shell()`, `print()`
- [x] Cross-platform Tier 1: Windows, Linux, macOS behind one platform layer
- [x] CMake build, replacing the raw `g++` line
- [x] Tier 2 mobile bridges (iOS / Android), built-ins only
- [ ] Native HTTP behind a platform function, so `curl` works on mobile
- [ ] Variable expansion (`$VAR`) in the tokenizer
- [ ] Command history navigation + tab completion
- [ ] Config/aliases loaded from a profile file
- [ ] Plugin system (extra built-ins loaded at runtime)

## Notes

- Release binaries go to GitHub Releases, not into the repository.
- `admin_pass.txt` (used by `fille`) is gitignored and must never be committed.
- Pipes between external commands still go through temp files on Windows —
  the original behaviour, kept deliberately. Linux and macOS use real pipes.
