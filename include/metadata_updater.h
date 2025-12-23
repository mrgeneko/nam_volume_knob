#ifndef METADATA_UPDATER_H
#define METADATA_UPDATER_H

#include <nlohmann/json.hpp>

class MetadataUpdater {
public:
    static void updateMetadata(nlohmann::json& meta, float gainDb);
};

#endif // METADATA_UPDATER_H