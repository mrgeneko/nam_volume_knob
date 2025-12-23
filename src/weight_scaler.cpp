#include "weight_scaler.h"
#include <stdexcept>

bool WeightScaler::tryGetHeadWeightIndices(const std::string& arch, const nlohmann::json& config, size_t weightsSize, size_t& start, size_t& end, std::string& error) {
    if (weightsSize == 0) {
        error = "Weights array is empty.";
        return false;
    }

    if (arch == "LSTM") {
        if (!config.contains("hidden_size") || !config["hidden_size"].is_number_integer()) {
            error = "Missing or invalid config.hidden_size for LSTM.";
            return false;
        }
        const int hiddenSize = config["hidden_size"].get<int>();
        if (hiddenSize <= 0) {
            error = "config.hidden_size must be > 0 for LSTM.";
            return false;
        }
        if (static_cast<size_t>(hiddenSize) > weightsSize) {
            error = "config.hidden_size is larger than weights array.";
            return false;
        }
        start = weightsSize - static_cast<size_t>(hiddenSize);
        end = weightsSize;
        return true;
    }

    if (arch == "WaveNet") {
        if (weightsSize < 10) {
            error = "Weights array too small for WaveNet placeholder logic.";
            return false;
        }
        start = weightsSize - 10;
        end = weightsSize;
        return true;
    }

    if (arch == "ConvNet") {
        if (weightsSize < 5) {
            error = "Weights array too small for ConvNet placeholder logic.";
            return false;
        }
        start = weightsSize - 5;
        end = weightsSize;
        return true;
    }

    if (arch == "Linear") {
        start = 0;
        end = weightsSize;
        return true;
    }

    error = "Unsupported architecture: " + arch;
    return false;
}

std::pair<size_t, size_t> WeightScaler::getHeadWeightIndices(const std::string& arch, const nlohmann::json& config, size_t weightsSize) {
    size_t start = 0;
    size_t end = 0;
    std::string error;
    if (!tryGetHeadWeightIndices(arch, config, weightsSize, start, end, error)) {
        throw std::runtime_error(error);
    }
    return {start, end};
}

void WeightScaler::scaleWeights(std::vector<float>& weights, size_t start, size_t end, float factor) {
    for (size_t i = start; i < end; ++i) {
        weights[i] *= factor;
    }
}