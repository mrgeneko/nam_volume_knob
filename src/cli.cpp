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
    return "Usage: nam-volume-knob --input <file> [--input <file> ...] [--output <file> | --output-dir <dir>] (--gain-db <dB[,dB...]> | --gain-linear <factor[,factor...]>)";
}

static bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

static bool fileExists(const std::string& path) {
    std::error_code ec;
    return !path.empty() && std::filesystem::exists(path, ec);
}

static std::vector<std::string> splitCommaSeparated(const std::string& s) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : s) {
        if (c == ',') {
            if (!current.empty()) parts.push_back(current);
            current.clear();
        } else if (!std::isspace(static_cast<unsigned char>(c))) {
            current.push_back(c);
        }
    }
    if (!current.empty()) parts.push_back(current);
    return parts;
}

static bool parseFloatList(const std::string& raw, std::vector<float>& out, std::string& err) {
    auto parts = splitCommaSeparated(raw);
    if (parts.empty()) {
        err = "Expected a number or comma-separated list.";
        return false;
    }
    for (const auto& p : parts) {
        try {
            out.push_back(std::stof(p));
        } catch (...) {
            err = "Invalid float: " + p;
            return false;
        }
    }
    return true;
}

static std::string formatGainForName(float gain, bool isDb) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(7) << gain;
    std::string gainStr = oss.str();
    const size_t dotPos = gainStr.find('.');
    if (dotPos != std::string::npos) {
        while (gainStr.size() > dotPos + 1 && gainStr.back() == '0') gainStr.pop_back();
        if (gainStr.back() == '.') gainStr += '0';
    }
    std::replace(gainStr.begin(), gainStr.end(), '.', '_');
    if (isDb && gain >= 0) gainStr = "+" + gainStr;
    return gainStr;
}

static std::string joinPath(const std::string& dir, const std::string& filename) {
    if (dir.empty()) return filename;
    std::filesystem::path p(dir);
    p /= filename;
    return p.string();
}

CliParseResult CliHandler::parseArgs(int argc, char* argv[]) {
    CliParseResult result;
    CliArgs args;

    bool seenGainDb = false;
    bool seenGainLinear = false;
    bool seenInput = false;

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
            args.inputPaths.push_back(argv[++i]);
            seenInput = true;
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

        if (arg == "--output-dir") {
            if (i + 1 >= argc) {
                result.error = "Error: Missing value for --output-dir.\n" + usage();
                return result;
            }
            args.outputDir = argv[++i];
            continue;
        }

        if (arg == "--gain-db") {
            if (i + 1 >= argc) {
                result.error = "Error: Missing value for --gain-db.\n" + usage();
                return result;
            }
            std::string err;
            if (!parseFloatList(argv[++i], args.gainDbs, err)) {
                result.error = "Error: Invalid value for --gain-db: " + err + "\n" + usage();
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
            std::string err;
            if (!parseFloatList(argv[++i], args.gainLinears, err)) {
                result.error = "Error: Invalid value for --gain-linear: " + err + "\n" + usage();
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

    if (!seenInput || args.inputPaths.empty()) {
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

    if (!args.outputPath.empty() && !args.outputDir.empty()) {
        result.error = "Error: Use either --output or --output-dir (not both).\n" + usage();
        return result;
    }

    for (const auto& in : args.inputPaths) {
        if (!fileExists(in)) {
            result.error = "Error: Input file not found: " + in;
            return result;
        }
    }

    if (!args.useDb) {
        for (float g : args.gainLinears) {
            if (g <= 0.0f) {
                result.error = "Error: --gain-linear values must be > 0 (required for log10 + metadata update).";
                return result;
            }
        }
    }

    // If user requested a single explicit output file, enforce single-output mode.
    const size_t inputCount = args.inputPaths.size();
    const size_t gainCount = args.useDb ? args.gainDbs.size() : args.gainLinears.size();
    const size_t outputCount = inputCount * gainCount;
    if (!args.outputPath.empty() && outputCount != 1) {
        result.error = "Error: --output can only be used when producing exactly one output. Use --output-dir instead.\n" + usage();
        return result;
    }

    // Default behavior: if a gain flag appeared but list is empty (shouldn't happen), provide a single default.
    if (args.useDb && args.gainDbs.empty()) args.gainDbs.push_back(0.0f);
    if (!args.useDb && args.gainLinears.empty()) args.gainLinears.push_back(1.0f);

    result.ok = true;
    result.args = args;
    return result;
}

CliRunResult CliHandler::run(const CliArgs& args) {
    CliRunResult result;

    try {
        const auto& gains = args.useDb ? args.gainDbs : args.gainLinears;

        for (const auto& inputPath : args.inputPaths) {
            auto j = NamParser::parseNamFile(inputPath);
            if (!Validator::validateNam(j)) {
                result.exitCode = 3;
                result.error = "Error: Invalid .nam file: " + inputPath;
                return result;
            }

            for (float gain : gains) {
                // Work on a fresh copy per gain.
                auto jOut = j;

                std::string arch = jOut["architecture"];
                auto config = jOut["config"];
                auto weightsVec = jOut["weights"].get<std::vector<float>>();
                size_t weightsSize = weightsVec.size();
                auto [start, end] = WeightScaler::getHeadWeightIndices(arch, config, weightsSize);

                float factor = args.useDb ? std::pow(10.0f, gain / 20.0f) : gain;
                WeightScaler::scaleWeights(weightsVec, start, end, factor);
                jOut["weights"] = weightsVec;

                float gainDbForMetadata = args.useDb ? gain : 20.0f * std::log10(gain);
                MetadataUpdater::updateMetadata(jOut["metadata"], gainDbForMetadata);

                std::string outputPath;
                if (!args.outputPath.empty()) {
                    outputPath = args.outputPath;
                } else {
                    std::filesystem::path inPath(inputPath);
                    const std::string baseName = inPath.stem().string();
                    const std::string ext = inPath.extension().string();
                    const std::string gainStr = formatGainForName(gain, args.useDb);
                    const std::string suffix = args.useDb ? "db" : "lin";
                    const std::string outName = baseName + "_" + gainStr + suffix + ext;

                    if (!args.outputDir.empty()) {
                        outputPath = joinPath(args.outputDir, outName);
                    } else {
                        // Default: write next to input file
                        std::filesystem::path outPath = inPath.parent_path() / outName;
                        outputPath = outPath.string();
                    }
                }

                // Avoid overwriting existing files
                std::string finalPath = outputPath;
                int version = 2;
                while (std::filesystem::exists(finalPath)) {
                    std::filesystem::path p(outputPath);
                    std::filesystem::path base = p;
                    base.replace_extension();
                    const std::string ext = p.extension().string();
                    finalPath = base.string() + "_v" + std::to_string(version) + ext;
                    version++;
                }

                std::ofstream out(finalPath);
                if (!out.is_open()) {
                    result.exitCode = 4;
                    result.error = "Error: Failed to open output file for writing: " + finalPath;
                    return result;
                }
                out << jOut.dump(4);
                if (!out.good()) {
                    result.exitCode = 4;
                    result.error = "Error: Failed while writing output file: " + finalPath;
                    return result;
                }

                result.outputPaths.push_back(finalPath);
            }
        }

        result.exitCode = 0;
        return result;
    } catch (const std::exception& e) {
        result.exitCode = 1;
        result.error = std::string("Error: ") + e.what();
        return result;
    }
}
