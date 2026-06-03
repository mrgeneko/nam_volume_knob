#include "validator.h"
#include <string>
#include <regex>

bool Validator::validateNam(const nlohmann::json& j) {
    if (!j.contains("version") || !j["version"].is_string()) return false;
    std::string version = j["version"].get<std::string>();

    // Validate semantic version format: "0.X.Y" where X and Y are integers
    std::regex versionPattern(R"(^0\.\d+\.\d+$)");
    if (!std::regex_match(version, versionPattern)) return false;

    // Require minimum version 0.5.0
    std::string minorPart = version.substr(2, version.find('.', 2) - 2);
    int minor = std::stoi(minorPart);
    if (minor < 5) return false;

    if (!j.contains("architecture") || !j["architecture"].is_string()) return false;
    if (!j.contains("config") || !j["config"].is_object()) return false;
    if (!j.contains("weights") || !j["weights"].is_array()) return false;

    // Validate weights are all numeric
    for (const auto& w : j["weights"]) {
        if (!w.is_number()) return false;
    }

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