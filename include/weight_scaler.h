#ifndef WEIGHT_SCALER_H
#define WEIGHT_SCALER_H

#include <nlohmann/json.hpp>
#include <vector>
#include <utility>

class WeightScaler {
public:
    static std::pair<size_t, size_t> getHeadWeightIndices(const std::string& arch, const nlohmann::json& config, size_t weightsSize);
    static void scaleWeights(std::vector<float>& weights, size_t start, size_t end, float factor);
};

#endif // WEIGHT_SCALER_H