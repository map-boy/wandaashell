#include "core/tokenizer.h"

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string cur;
    bool inToken = false;
    char quoteChar = 0;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (quoteChar != 0) {
            if (c == '\\' && i + 1 < line.size() && (line[i + 1] == quoteChar || line[i + 1] == '\\')) {
                cur += line[i + 1];
                ++i;
            } else if (c == quoteChar) {
                quoteChar = 0;
            } else {
                cur += c;
            }
            continue;
        }

        if (c == '"' || c == '\'') {
            quoteChar = c;
            inToken = true;
            continue;
        }

        if (isspace((unsigned char)c)) {
            if (inToken) {
                tokens.push_back(cur);
                cur.clear();
                inToken = false;
            }
            continue;
        }

        cur += c;
        inToken = true;
    }

    if (inToken) tokens.push_back(cur);
    return tokens;
}