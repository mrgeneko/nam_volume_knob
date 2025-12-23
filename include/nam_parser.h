#ifndef NAM_PARSER_H
#define NAM_PARSER_H

#include <nlohmann/json.hpp>
#include <string>

class NamParser {
public:
    static nlohmann::json parseNamFile(const std::string& path);
};

#endif // NAM_PARSER_H