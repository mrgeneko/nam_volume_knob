#ifndef CLI_H
#define CLI_H

#include <string>

struct CliArgs {
    std::string inputPath;
    std::string outputPath;
    float gainDb = 0.0f;
    float gainLinear = 1.0f;
    bool useDb = true;
    bool showHelp = false;
};

struct CliParseResult {
    CliArgs args;
    bool ok = false;
    std::string error;
};

struct CliRunResult {
    int exitCode = 0;
    std::string error;
    std::string outputPath;
};

class CliHandler {
public:
    static CliParseResult parseArgs(int argc, char* argv[]);
    static CliRunResult run(const CliArgs& args);
    static std::string usage();
};

#endif // CLI_H