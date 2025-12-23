#include "cli.h"
#include "nam_parser.h"
#include "weight_scaler.h"
#include "metadata_updater.h"
#include "validator.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <filesystem>

std::string CliHandler::usage() {
    return "Usage: nam-volume-knob --input <file> [--output <file>] (--gain-db <dB> | --gain-linear <factor>)";
}

static bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

static bool fileExists(const std::string& path) {
    std::error_code ec;
    return !path.empty() && std::filesystem::exists(path, ec);
}

CliParseResult CliHandler::parseArgs(int argc, char* argv[]) {
    CliParseResult result;
    CliArgs args;

    bool seenGainDb = false;
    bool seenGainLinear = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            args.showHelp = true;
            result.ok = true;
            result.args = args;
            return result;
        }

        if (arg == "--input") {
            if (i + 1 >= argc) {
                result.error = "Error: Missing value for --input.\n" + usage();
                return result;
            }
            args.inputPath = argv[++i];
            continue;
        }

        if (arg == "--output") {
            if (i + 1 >= argc) {
                result.error = "Error: Missing value for --output.\n" + usage();
                return result;
            }
            args.outputPath = argv[++i];
            continue;
        }

        if (arg == "--gain-db") {
            if (i + 1 >= argc) {
                result.error = "Error: Missing value for --gain-db.\n" + usage();
                return result;
            }
            try {
                args.gainDb = std::stof(argv[++i]);
            } catch (...) {
                result.error = "Error: Invalid float for --gain-db.\n" + usage();
                return result;
            }
            args.useDb = true;
            seenGainDb = true;
            continue;
        }

        if (arg == "--gain-linear") {
            if (i + 1 >= argc) {
                result.error = "Error: Missing value for --gain-linear.\n" + usage();
                return result;
            }
            try {
                args.gainLinear = std::stof(argv[++i]);
            } catch (...) {
                result.error = "Error: Invalid float for --gain-linear.\n" + usage();
                return result;
            }
            args.useDb = false;
            seenGainLinear = true;
            continue;
        }

        if (startsWith(arg, "-")) {
            result.error = "Error: Unknown option: " + arg + "\n" + usage();
            return result;
        }
    }

    if (args.inputPath.empty()) {
        result.error = "Error: --input is required.\n" + usage();
        return result;
    }

    if (seenGainDb && seenGainLinear) {
        result.error = "Error: --gain-db and --gain-linear are mutually exclusive.\n" + usage();
        return result;
    }

    if (!seenGainDb && !seenGainLinear) {
        result.error = "Error: One of --gain-db or --gain-linear is required.\n" + usage();
        return result;
    }

    if (!fileExists(args.inputPath)) {
        result.error = "Error: Input file not found: " + args.inputPath;
        return result;
    }

    if (!args.useDb && args.gainLinear <= 0.0f) {
        result.error = "Error: --gain-linear must be > 0 (required for log10 + metadata update).";
        return result;
    }

    if (args.outputPath.empty()) {
        // Default output name
        const size_t dotPos = args.inputPath.find_last_of('.');
        std::string base = (dotPos == std::string::npos) ? args.inputPath : args.inputPath.substr(0, dotPos);
        std::string ext = (dotPos == std::string::npos) ? std::string() : args.inputPath.substr(dotPos);
        if (args.useDb) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(7) << args.gainDb;
            std::string gainStr = oss.str();
            const size_t dotPos2 = gainStr.find('.');
            if (dotPos2 != std::string::npos) {
                while (gainStr.size() > dotPos2 + 1 && gainStr.back() == '0') gainStr.pop_back();
                if (gainStr.back() == '.') gainStr += '0';
            }
            std::replace(gainStr.begin(), gainStr.end(), '.', '_');
            if (args.gainDb >= 0) gainStr = "+" + gainStr;
            args.outputPath = base + "_" + gainStr + "db" + ext;
        } else {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(7) << args.gainLinear;
            std::string gainStr = oss.str();
            const size_t dotPos2 = gainStr.find('.');
            if (dotPos2 != std::string::npos) {
                while (gainStr.size() > dotPos2 + 1 && gainStr.back() == '0') gainStr.pop_back();
                if (gainStr.back() == '.') gainStr += '0';
            }
            std::replace(gainStr.begin(), gainStr.end(), '.', '_');
            args.outputPath = base + "_" + gainStr + "lin" + ext;
        }
    }

    result.ok = true;
    result.args = args;
    return result;
}

CliRunResult CliHandler::run(const CliArgs& args) {
    CliRunResult result;

    try {
        auto j = NamParser::parseNamFile(args.inputPath);
        if (!Validator::validateNam(j)) {
            result.exitCode = 3;
            result.error = "Error: Invalid .nam file: " + args.inputPath;
            return result;
        }

        std::string arch = j["architecture"];
        auto config = j["config"];
        auto weightsVec = j["weights"].get<std::vector<float>>();
        size_t weightsSize = weightsVec.size();
        auto [start, end] = WeightScaler::getHeadWeightIndices(arch, config, weightsSize);

        float factor = args.useDb ? std::pow(10.0f, args.gainDb / 20.0f) : args.gainLinear;
        WeightScaler::scaleWeights(weightsVec, start, end, factor);
        j["weights"] = weightsVec;

        float gainDbForMetadata = args.useDb ? args.gainDb : 20.0f * std::log10(args.gainLinear);
        MetadataUpdater::updateMetadata(j["metadata"], gainDbForMetadata);

        // Avoid overwriting existing files
        std::string outputPath = args.outputPath;
        int version = 2;
        while (std::filesystem::exists(outputPath)) {
            size_t dotPos = args.outputPath.find_last_of('.');
            std::string base = (dotPos == std::string::npos) ? args.outputPath : args.outputPath.substr(0, dotPos);
            std::string ext = (dotPos == std::string::npos) ? std::string() : args.outputPath.substr(dotPos);
            outputPath = base + "_v" + std::to_string(version) + ext;
            version++;
        }

        std::ofstream out(outputPath);
        if (!out.is_open()) {
            result.exitCode = 4;
            result.error = "Error: Failed to open output file for writing: " + outputPath;
            return result;
        }
        out << j.dump(4);
        if (!out.good()) {
            result.exitCode = 4;
            result.error = "Error: Failed while writing output file: " + outputPath;
            return result;
        }

        result.exitCode = 0;
        result.outputPath = outputPath;
        return result;
    } catch (const std::exception& e) {
        result.exitCode = 1;
        result.error = std::string("Error: ") + e.what();
        return result;
    }
}
