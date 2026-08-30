#include "core/pipeline.h"
#include <iostream>

std::vector<Stage> parsePipeline(const std::vector<std::string>& tokens) {
    std::vector<std::vector<std::string>> rawStages;
    rawStages.push_back({});
    for (auto& t : tokens) {
        if (t == "|") rawStages.push_back({});
        else rawStages.back().push_back(t);
    }

    std::vector<Stage> stages;
    for (auto& raw : rawStages) {
        Stage s;
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] == ">" || raw[i] == ">>") {
                s.redirectMode = (raw[i] == ">") ? 1 : 2;
                if (i + 1 < raw.size()) { s.redirectFile = raw[++i]; }
                else { std::cerr << "syntax error: expected filename after " << raw[i] << "\n"; s.redirectMode = 0; }
            } else if (raw[i] == "<") {
                if (i + 1 < raw.size()) { s.hasInputRedirect = true; s.inputFile = raw[++i]; }
                else { std::cerr << "syntax error: expected filename after <\n"; }
            } else {
                s.args.push_back(raw[i]);
            }
        }
        stages.push_back(s);
    }
    return stages;
}
