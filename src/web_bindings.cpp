#include <emscripten/bind.h>
#include "nam_parser.h"
#include "weight_scaler.h"
#include "validator.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>

std::string processNam(const std::string& jsonStr, float factor, float gainDb) {
    // Important: the shipped wasm may be built without exception catching.
    // Avoid throwing C++ exceptions here; return "Error: ..." strings instead.

    auto j = nlohmann::json::parse(jsonStr, nullptr, false);
    if (j.is_discarded()) {
        return "Error: Failed to parse JSON.";
    }

    if (!Validator::validateNam(j)) {
        return "Error: Invalid .nam file.";
    }

    if (!j.contains("architecture") || !j["architecture"].is_string()) {
        return "Error: Missing or invalid architecture field.";
    }
    const std::string arch = j["architecture"].get<std::string>();

    if (!j.contains("config") || !j["config"].is_object()) {
        return "Error: Missing or invalid config field.";
    }
    const auto& config = j["config"];

    if (!j.contains("weights") || !j["weights"].is_array()) {
        return "Error: Missing or invalid weights field.";
    }

    // Handle A2 (SlimmableContainer) models differently from flat architectures
    if (arch == "SlimmableContainer") {
        std::string err;
        if (!WeightScaler::tryScaleA2Model(j, factor, err)) {
            return "Error: " + err;
        }
    } else {
        // Convert weights to float vector safely (no exceptions).
        std::vector<float> weightsVec;
        weightsVec.reserve(j["weights"].size());
        for (const auto& w : j["weights"]) {
            if (!w.is_number()) {
                return "Error: Weights array contains non-numeric value(s).";
            }
            weightsVec.push_back(static_cast<float>(w.get<double>()));
        }

        size_t start = 0;
        size_t end = 0;
        std::string err;
        if (!WeightScaler::tryGetHeadWeightIndices(arch, config, weightsVec.size(), start, end, err)) {
            return "Error: " + err;
        }

        WeightScaler::scaleWeights(weightsVec, start, end, factor);
        j["weights"] = weightsVec;

        // Update metadata to reflect the scaling (prevents host normalization from undoing it)
        if (j.contains("metadata") && j["metadata"].is_object()) {
            if (j["metadata"].contains("loudness") && j["metadata"]["loudness"].is_number()) {
                float loudness = j["metadata"]["loudness"].get<float>();
                j["metadata"]["loudness"] = loudness + gainDb;
            }
            if (j["metadata"].contains("gain") && j["metadata"]["gain"].is_number()) {
                float gain_val = j["metadata"]["gain"].get<float>();
                j["metadata"]["gain"] = gain_val + gainDb;
            }
        }
        if (j["config"].contains("output_level") && j["config"]["output_level"].is_number()) {
            float output_level = j["config"]["output_level"].get<float>();
            j["config"]["output_level"] = output_level + gainDb;
        }
    }

    return j.dump(4);
}

EMSCRIPTEN_BINDINGS(my_module) {
    emscripten::function("processNam", &processNam);
}