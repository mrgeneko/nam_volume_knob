#ifndef WEIGHT_SCALER_H
#define WEIGHT_SCALER_H

#include <nlohmann/json.hpp>
#include <vector>
#include <utility>
#include <string>

class WeightScaler {
public:
    static bool tryGetHeadWeightIndices(const std::string& arch, const nlohmann::json& config, size_t weightsSize, size_t& start, size_t& end, std::string& error);
    static std::pair<size_t, size_t> getHeadWeightIndices(const std::string& arch, const nlohmann::json& config, size_t weightsSize);
    static void scaleWeights(std::vector<float>& weights, size_t start, size_t end, float factor);
};

#endif // WEIGHT_SCALER_H