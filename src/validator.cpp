#include "validator.h"
#include <string>
#include <regex>
#include <cmath>

bool Validator::validateNam(const nlohmann::json& j) {
    if (!j.contains("version") || !j["version"].is_string()) return false;
    std::string version = j["version"].get<std::string>();

    // Validate semantic version format: "0.X.Y" where X and Y are integers
    std::regex versionPattern(R"(^0\.\d+\.\d+$)");
    if (!std::regex_match(version, versionPattern)) return false;

    // Require minimum version 0.5.0
    std::string minorPart = version.substr(2, version.find('.', 2) - 2);
    int minor;
    try {
        minor = std::stoi(minorPart);
    } catch (const std::out_of_range&) {
        // Version number is too large to fit in int
        return false;
    }
    if (minor < 5) return false;

    if (!j.contains("architecture") || !j["architecture"].is_string()) return false;
    if (!j.contains("config") || !j["config"].is_object()) return false;

    // Check architecture first to distinguish A1 (flat) from A2 (SlimmableContainer)
    std::string arch = j["architecture"].get<std::string>();

    // A1 models (non-SlimmableContainer) require weights at top level
    if (arch != "SlimmableContainer") {
        if (!j.contains("weights") || !j["weights"].is_array() || j["weights"].empty()) return false;

        // Validate weights are all numeric and finite (no NaN or Infinity)
        for (const auto& w : j["weights"]) {
            if (!w.is_number()) return false;
            double value = w.get<double>();
            if (!std::isfinite(value)) return false;  // Reject NaN, Infinity, -Infinity
        }

        // Validate architecture-specific config fields for A1 models
        if (arch == "LSTM") {
            if (!j["config"].contains("hidden_size") || !j["config"]["hidden_size"].is_number_integer()) {
                return false;
            }
        } else if (arch == "ConvNet") {
            if (!j["config"].contains("channels") || !j["config"]["channels"].is_number_integer()) {
                return false;
            }
            if (!j["config"].contains("out_channels") || !j["config"]["out_channels"].is_number_integer()) {
                return false;
            }
        } else if (arch == "WaveNet") {
            // WaveNet doesn't strictly require specific config fields for weight scaling
            // (head_scale is encoded as the last weight value)
        } else if (arch == "Linear") {
            // Linear model: minimal config requirements, no specific fields needed
        } else {
            // Unknown architecture (not a recognized A1 type)
            return false;
        }
    } else {
        // A2 (SlimmableContainer) models: weights are inside submodels, not at top level
        if (!j["config"].is_object()) return false;
        if (!j["config"].contains("submodels") || !j["config"]["submodels"].is_array()) return false;
        // Validate that submodels array is not empty and each has required structure
        if (j["config"]["submodels"].empty()) return false;
        for (const auto& submodel_entry : j["config"]["submodels"]) {
            if (!submodel_entry.is_object() || !submodel_entry.contains("model")) return false;
            const auto& model = submodel_entry["model"];
            if (!model.is_object() || !model.contains("architecture") || !model.contains("config") || !model.contains("weights")) {
                return false;
            }
            // Validate submodel weights
            if (!model["weights"].is_array() || model["weights"].empty()) {
                return false;
            }
            for (const auto& w : model["weights"]) {
                if (!w.is_number()) return false;
                double value = w.get<double>();
                if (!std::isfinite(value)) return false;
            }
        }
    }

    return true;
}