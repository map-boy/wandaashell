$funcs = @'

static int cmd_insertafter(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    if (args.size() < 4) { std::cerr << "insertafter: usage: insertafter <file> <pattern> <text...>\n"; return 1; }
    std::ifstream ifs(args[1]);
    if (!ifs) { std::cerr << "insertafter: cannot open " << args[1] << "\n"; return 1; }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ifs, line)) lines.push_back(line);
    ifs.close();

    std::string insertText;
    for (size_t i = 3; i < args.size(); ++i) insertText += args[i] + (i + 1 < args.size() ? " " : "");

    bool found = false;
    std::vector<std::string> result;
    for (auto& l : lines) {
        result.push_back(l);
        if (!found && l.find(args[2]) != std::string::npos) {
            result.push_back(insertText);
            found = true;
        }
    }
    if (!found) { std::cerr << "insertafter: pattern not found: " << args[2] << "\n"; return 1; }

    std::ofstream ofs(args[1], std::ios::trunc);
    for (auto& l : result) ofs << l << "\n";
    out << "insertafter: inserted 1 line into " << args[1] << "\n";
    return 0;
}

static int cmd_grepn(const std::vector<std::string>& args, std::istream&, std::ostream& out) {
    if (args.size() < 3) { std::cerr << "grepn: usage: grepn <pattern> <file>\n"; return 1; }
    std::ifstream ifs(args[2]);
    if (!ifs) { std::cerr << "grepn: cannot open " << args[2] << "\n"; return 1; }
    std::string line;
    int num = 0;
    while (std::getline(ifs, line)) {
        ++num;
        if (line.find(args[1]) != std::string::npos) out << num << ": " << line << "\n";
    }
    return 0;
}
'@

$content = Get-Content builtins.cpp -Raw
$content = $content -replace 'static int cmd_help', ($funcs + "`nstatic int cmd_help")
$content = $content -replace '(\{"grep", cmd_grep\},)', "`$1`n        {`"grepn`", cmd_grepn},`n        {`"insertafter`", cmd_insertafter},"
Set-Content builtins.cpp $content -NoNewline
Write-Host "done"