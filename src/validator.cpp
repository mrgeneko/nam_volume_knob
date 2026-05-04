#include "validator.h"
#include <string>

bool Validator::validateNam(const nlohmann::json& j) {
    if (!j.contains("version") || !j["version"].is_string()) return false;
    std::string version = j["version"];
    // Accept model versions 0.5.x and later (0.6, 0.7, etc.)
    // Model version is independent from NAM package version
    if (version.size() < 2 || version.substr(0, 2) != "0.") return false;
    if (!j.contains("architecture") || !j.contains("config") || !j.contains("weights")) return false;
    return true;
}