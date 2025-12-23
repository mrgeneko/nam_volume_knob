#include "metadata_updater.h"

void MetadataUpdater::updateMetadata(nlohmann::json& meta, float gainDb) {
    if (meta.contains("loudness")) {
        meta["loudness"] = meta["loudness"].get<float>() + gainDb;
    }
    if (meta.contains("output_level_dbu")) {
        meta["output_level_dbu"] = meta["output_level_dbu"].get<float>() + gainDb;
    }
}