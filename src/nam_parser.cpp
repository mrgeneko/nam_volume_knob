#include "nam_parser.h"
#include <fstream>
#include <iostream>

nlohmann::json NamParser::parseNamFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }

    nlohmann::json j;
    try {
        file >> j;
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("Failed to parse JSON from " + path + ": " + e.what());
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("JSON error in " + path + ": " + e.what());
    }

    if (file.fail() && !file.eof()) {
        throw std::runtime_error("Error reading file: " + path);
    }

    return j;
}