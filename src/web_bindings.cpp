#ifdef EMSCRIPTEN
#include <emscripten/bind.h>
#include "nam_parser.h"
#include "weight_scaler.h"
#include "metadata_updater.h"
#include "validator.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>

std::string processNam(const std::string& jsonStr, float factor, float gainDb) {
    try {
        auto j = nlohmann::json::parse(jsonStr);
        if (!Validator::validateNam(j)) {
            return "Error: Invalid .nam file";
        }
        std::string arch = j["architecture"];
        auto config = j["config"];
        auto weightsVec = j["weights"].get<std::vector<float>>();
        size_t weightsSize = weightsVec.size();
        auto [start, end] = WeightScaler::getHeadWeightIndices(arch, config, weightsSize);
        WeightScaler::scaleWeights(weightsVec, start, end, factor);
        j["weights"] = weightsVec;
        MetadataUpdater::updateMetadata(j["metadata"], gainDb);
        return j.dump(4);
    } catch (const std::exception& e) {
        return "Error: " + std::string(e.what());
    }
}

EMSCRIPTEN_BINDINGS(my_module) {
    emscripten::function("processNam", &processNam);
}
#endif