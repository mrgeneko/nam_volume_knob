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
        start = weightsSize - 1;
        end = weightsSize;
        return true;
    }

    if (arch == "ConvNet") {
        if (!config.contains("channels") || !config["channels"].is_number_integer()) {
            error = "Missing or invalid config.channels for ConvNet.";
            return false;
        }
        if (!config.contains("out_channels") || !config["out_channels"].is_number_integer()) {
            error = "Missing or invalid config.out_channels for ConvNet.";
            return false;
        }
        const int channels = config["channels"].get<int>();
        const int outChannels = config["out_channels"].get<int>();
        if (channels <= 0 || outChannels <= 0) {
            error = "config.channels and config.out_channels must be > 0 for ConvNet.";
            return false;
        }
        const size_t headSize = static_cast<size_t>(channels * outChannels + outChannels);
        if (headSize > weightsSize) {
            error = "ConvNet head size is larger than weights array.";
            return false;
        }
        start = weightsSize - headSize;
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