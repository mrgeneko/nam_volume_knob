#include "weight_scaler.h"
#include <stdexcept>
#include <cmath>

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

    if (arch == "SlimmableContainer") {
        error = "SlimmableContainer (A2) cannot be scaled directly. Use scaleA2Model() instead.";
        return false;
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

bool WeightScaler::tryScaleA2Model(nlohmann::json& model, float factor, std::string& error) {
    if (!model.contains("architecture") || !model["architecture"].is_string()) {
        error = "Missing or invalid architecture field in model.";
        return false;
    }

    std::string arch = model["architecture"].get<std::string>();

    if (arch == "SlimmableContainer") {
        if (!model.contains("config") || !model["config"].is_object()) {
            error = "SlimmableContainer missing or invalid config.";
            return false;
        }
        if (!model["config"].contains("submodels") || !model["config"]["submodels"].is_array()) {
            error = "SlimmableContainer config missing or invalid submodels array.";
            return false;
        }

        for (auto& submodel_entry : model["config"]["submodels"]) {
            if (!submodel_entry.contains("model") || !submodel_entry["model"].is_object()) {
                error = "SlimmableContainer submodel entry missing or invalid model field.";
                return false;
            }

            if (!tryScaleA2Model(submodel_entry["model"], factor, error)) {
                return false;
            }
        }
        return true;
    } else {
        // For non-container models (WaveNet, LSTM, ConvNet, Linear), scale the weights directly
        if (!model.contains("weights") || !model["weights"].is_array()) {
            error = "Model missing or invalid weights array.";
            return false;
        }

        std::vector<float> weights = model["weights"].get<std::vector<float>>();
        if (!model.contains("config") || !model["config"].is_object()) {
            error = "Model missing or invalid config.";
            return false;
        }

        size_t start, end;
        if (!tryGetHeadWeightIndices(arch, model["config"], weights.size(), start, end, error)) {
            return false;
        }

        scaleWeights(weights, start, end, factor);
        model["weights"] = weights;

        // For WaveNet, also scale the head_scale parameter which controls output level
        if (arch == "WaveNet") {
            if (model["config"].contains("head_scale") && model["config"]["head_scale"].is_number()) {
                float head_scale = model["config"]["head_scale"].get<float>();
                model["config"]["head_scale"] = head_scale * factor;
            }
        }

        // Scale loudness metadata so normalization doesn't negate the weight changes.
        // Loudness is in dB; factor of 2.0 = +6dB, so we add 20*log10(factor) to loudness.
        float dbGain = 20.0f * std::log10(factor);
        if (model["metadata"].contains("loudness") && model["metadata"]["loudness"].is_number()) {
            float loudness = model["metadata"]["loudness"].get<float>();
            model["metadata"]["loudness"] = loudness + dbGain;
        }
        if (model["config"].contains("output_level") && model["config"]["output_level"].is_number()) {
            float output_level = model["config"]["output_level"].get<float>();
            model["config"]["output_level"] = output_level + dbGain;
        }
        if (model["config"].contains("input_level") && model["config"]["input_level"].is_number()) {
            // Input level is not affected by output weight scaling, so don't modify it.
        }

        return true;
    }
}

void WeightScaler::scaleA2Model(nlohmann::json& model, float factor) {
    std::string error;
    if (!tryScaleA2Model(model, factor, error)) {
        throw std::runtime_error(error);
    }
}