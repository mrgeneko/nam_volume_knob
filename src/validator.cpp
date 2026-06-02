#include "validator.h"
#include <string>

bool Validator::validateNam(const nlohmann::json& j) {
    if (!j.contains("version") || !j["version"].is_string()) return false;
    std::string version = j["version"];
    // Accept model versions 0.5.x and later (0.6, 0.7, etc.)
    // Model version is independent from NAM package version
    if (version.size() < 2 || version.substr(0, 2) != "0.") return false;
    if (!j.contains("architecture") || !j.contains("config") || !j.contains("weights")) return false;

    // Additional validation for A2 (SlimmableContainer) models
    if (j["architecture"].is_string() && j["architecture"].get<std::string>() == "SlimmableContainer") {
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
        }
    }

    return true;
}