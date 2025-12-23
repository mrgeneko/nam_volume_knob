#include "metadata_updater.h"

void MetadataUpdater::updateMetadata(nlohmann::json& meta, float gainDb) {
    if (meta.contains("loudness") && meta["loudness"].is_number()) {
        meta["loudness"] = static_cast<float>(meta["loudness"].get<double>()) + gainDb;
    }
    if (meta.contains("output_level_dbu") && meta["output_level_dbu"].is_number()) {
        meta["output_level_dbu"] = static_cast<float>(meta["output_level_dbu"].get<double>()) + gainDb;
    }
}