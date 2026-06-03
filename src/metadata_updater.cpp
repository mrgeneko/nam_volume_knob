#include "metadata_updater.h"

void MetadataUpdater::updateMetadata(nlohmann::json& meta, float gainDb) {
    // NOTE: DO NOT update loudness or output_level_dbu metadata.
    //
    // These fields represent the model's intrinsic loudness and should remain unchanged.
    // Applications like Chainsmith FX use loudness for normalization:
    //   normalizationGain = 10^((-18.0 - loudness) / 20.0)
    //
    // If we update loudness to reflect the scaled output, the app's normalization
    // will counteract our weight scaling, negating the volume change.
    //
    // Example:
    //   Original: loudness=-26dB, normGain=2.625x; weights scaled 2.818x
    //   If we update loudness to -20dB: normGain becomes 1.316x
    //   Result: 2.818x * 1.316x = 3.71x (not 2.818x as intended!)
    //
    // By leaving loudness unchanged, apps receive accurate metadata about the
    // model's baseline loudness, and they can apply their own normalization
    // appropriately based on their design.
}