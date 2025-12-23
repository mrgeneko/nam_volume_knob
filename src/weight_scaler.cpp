#include "weight_scaler.h"
#include <stdexcept>

std::pair<size_t, size_t> WeightScaler::getHeadWeightIndices(const std::string& arch, const nlohmann::json& config, size_t weightsSize) {
    if (arch == "LSTM") {
        int hidden_size = config["hidden_size"];
        return {weightsSize - hidden_size, weightsSize};
    } else if (arch == "WaveNet") {
        // Placeholder: assume last some weights
        return {weightsSize - 10, weightsSize}; // Need proper logic
    } else if (arch == "ConvNet") {
        // Placeholder
        return {weightsSize - 5, weightsSize};
    } else if (arch == "Linear") {
        // All weights are head
        return {0, weightsSize};
    }
    throw std::runtime_error("Unsupported architecture: " + arch);
}

void WeightScaler::scaleWeights(std::vector<float>& weights, size_t start, size_t end, float factor) {
    for (size_t i = start; i < end; ++i) {
        weights[i] *= factor;
    }
}