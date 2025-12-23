#include "nam_parser.h"
#include <fstream>
#include <iostream>

nlohmann::json NamParser::parseNamFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    nlohmann::json j;
    file >> j;
    return j;
}