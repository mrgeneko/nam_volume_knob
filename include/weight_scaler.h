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

    // Scale A2 (SlimmableContainer) model by recursively scaling each submodel's head weights
    static bool tryScaleA2Model(nlohmann::json& model, float factor, std::string& error);
    static void scaleA2Model(nlohmann::json& model, float factor);

    // Update model metadata (loudness, gain, output_level) to reflect scaling applied to weights
    // This prevents host normalization from negating the weight-level changes
    static void updateMetadata(nlohmann::json& model, float dbGain);
};

#endif // WEIGHT_SCALER_H