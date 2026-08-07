# wandaashell

A custom command-line shell written in C++, built from scratch on Windows.

## Features

- REPL with a custom prompt: wandaa <cwd> >
- Built-in commands: cd, pwd, echo, exit / quit
- Unknown commands fall through and run as external Windows processes
- I/O redirection: > (truncate), >> (append), < (input from file)
- Pipes: cmd1 | cmd2 | cmd3, chaining built-ins and external commands freely
- Modular codebase (tokenizer, pipeline parser, builtins, external process runner, shell loop)

## Build

Requires a C++17 compiler (MinGW / g++). No build system needed - single compile step:

g++ -std=c++17 -o wandaashell.exe src\main.cpp src\tokenizer.cpp src\pipeline.cpp src\builtins.cpp src\external.cpp src\shell.cpp

## Run

.\wandaashell.exe

## Usage examples

pwd
echo hello wandaa
cd ..
echo hello wandaa > out.txt
pwd >> out.txt
echo piped | echo through
type out.txt | echo got it
exit

## Project structure

src/
  types.h            - shared structs (Stage, CommandFn)
  tokenizer.h/.cpp   - splits input into tokens
  pipeline.h/.cpp    - parses pipes and redirection into Stage objects
  builtins.h/.cpp    - cd, pwd, echo
  external.h/.cpp    - runs external processes, handles piping via temp files
  shell.h/.cpp       - main REPL loop
  main.cpp           - entry point

## Roadmap

- [x] Redirection (>, >>, <)
- [x] Pipes (cmd1 | cmd2)
- [ ] Proper tokenizer with quoting, escaping, variable expansion ($VAR)
- [ ] Scripting support (.wandaa script files with variables, if/loops)
- [ ] Plugin system (load extra built-ins from .dll at runtime)
- [ ] Command history + tab-completion
- [ ] Config/aliases (PowerShell-profile-style)

## Notes

- Built and tested on Windows with PowerShell + MinGW g++.
- Pipes currently shell out per-stage via temp files rather than true OS-level pipe handles, so very large data through a pipe is not optimized for throughput yet.
