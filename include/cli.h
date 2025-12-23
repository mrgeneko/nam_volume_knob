#ifndef CLI_H
#define CLI_H

#include <string>
#include <vector>

struct CliArgs {
    std::vector<std::string> inputPaths;

    // Only valid when there is exactly one output.
    std::string outputPath;
    // If set, outputs are written into this directory (recommended for multi-output).
    std::string outputDir;

    std::vector<float> gainDbs;
    std::vector<float> gainLinears;
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
    std::vector<std::string> outputPaths;
};

class CliHandler {
public:
    static CliParseResult parseArgs(int argc, char* argv[]);
    static CliRunResult run(const CliArgs& args);
    static std::string usage();
};

#endif // CLI_H