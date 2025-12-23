#include "cli.h"
#include <iostream>

int main(int argc, char* argv[]) {
    auto parsed = CliHandler::parseArgs(argc, argv);
    if (!parsed.ok) {
        std::cerr << parsed.error << std::endl;
        return 2;
    }

    if (parsed.args.showHelp) {
        std::cout << CliHandler::usage() << std::endl;
        return 0;
    }

    auto runResult = CliHandler::run(parsed.args);
    if (runResult.exitCode != 0) {
        std::cerr << runResult.error << std::endl;
        return runResult.exitCode;
    }

    return 0;
}