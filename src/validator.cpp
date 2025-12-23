#include "validator.h"
#include <string>

bool Validator::validateNam(const nlohmann::json& j) {
    if (!j.contains("version") || !j["version"].is_string()) return false;
    std::string version = j["version"];
    if (version.substr(0, 3) != "0.5") return false;
    if (!j.contains("architecture") || !j.contains("config") || !j.contains("weights")) return false;
    return true;
}