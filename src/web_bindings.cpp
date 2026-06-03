#include <emscripten/bind.h>
#include "nam_parser.h"
#include "weight_scaler.h"
#include "validator.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>

// Gain limits (must match CLI limits for consistency)
static constexpr float kMaxGainDb = 9.0f;
static constexpr float kMaxGainLinear = 2.8183829312644537f;  // pow(10, 9/20)

std::string processNam(const std::string& jsonStr, float factor, float gainDb) {
    // Important: the shipped wasm may be built without exception catching.
    // Avoid throwing C++ exceptions here; return "Error: ..." strings instead.

    // Validate gain parameters (defensive programming - web layer should also validate)
    if (!std::isfinite(factor) || factor <= 0.0f || factor > kMaxGainLinear) {
        return "Error: Gain factor must be > 0 and <= " + std::to_string(kMaxGainLinear);
    }
    if (!std::isfinite(gainDb) || gainDb > kMaxGainDb) {
        return "Error: Gain must be <= " + std::to_string(kMaxGainDb) + " dB";
    }

    auto j = nlohmann::json::parse(jsonStr, nullptr, false);
    if (j.is_discarded()) {
        return "Error: Failed to parse JSON.";
    }

    if (!Validator::validateNam(j)) {
        return "Error: Invalid .nam file format (missing required fields or corrupted).";
    }

    // Validation passed; extract guaranteed-safe fields
    const std::string arch = j["architecture"].get<std::string>();
    const auto& config = j["config"];

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

        // Update metadata to reflect the scaling
        WeightScaler::updateMetadata(j, gainDb);
    }

    return j.dump(4);
}

EMSCRIPTEN_BINDINGS(my_module) {
    emscripten::function("processNam", &processNam);
}