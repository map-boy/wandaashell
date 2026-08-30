#pragma once
//
// Redirects std::cout and std::cerr into a string for the duration of a scope.
//
// The desktop shell writes straight to the process's streams, which is exactly
// right for a terminal. A mobile app has no terminal: it needs the text back
// so it can render it in its own view. Rather than thread an ostream through
// every built-in, the interpreter's print() and every std::cerr diagnostic
// included, the bridges wrap a call in one of these.
//
// Not thread-safe, by construction: it swaps a process-global stream buffer.
// Bridge calls happen on one thread at a time, which is the only way this is
// safe to use.
//
#include <iostream>
#include <sstream>
#include <string>

class OutputCapture {
public:
    OutputCapture()
        : savedOut_(std::cout.rdbuf(buffer_.rdbuf())),
          savedErr_(std::cerr.rdbuf(buffer_.rdbuf())) {}

    ~OutputCapture() {
        std::cout.rdbuf(savedOut_);
        std::cerr.rdbuf(savedErr_);
    }

    OutputCapture(const OutputCapture&) = delete;
    OutputCapture& operator=(const OutputCapture&) = delete;

    std::string text() const { return buffer_.str(); }

private:
    std::ostringstream buffer_;
    std::streambuf* savedOut_;
    std::streambuf* savedErr_;
};
