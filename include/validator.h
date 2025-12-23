#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <nlohmann/json.hpp>

class Validator {
public:
    static bool validateNam(const nlohmann::json& j);
};

#endif // VALIDATOR_H