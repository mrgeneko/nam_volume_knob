#include <catch2/catch_all.hpp>
#include "validator.h"
#include "weight_scaler.h"
#include "metadata_updater.h"
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Helper to create a minimal valid .nam JSON
json makeNamJson(const std::string& version, const std::string& arch = "Linear") {
    json j;
    j["version"] = version;
    j["architecture"] = arch;
    j["config"] = json::object();
    if (arch == "Linear") {
        j["config"]["receptive_field"] = 1;
    } else if (arch == "WaveNet") {
        j["config"]["layers"] = json::array();
        j["config"]["head_scale"] = 0.02f;
    } else if (arch == "LSTM") {
        j["config"]["hidden_size"] = 8;
        j["config"]["num_layers"] = 1;
    }
    j["weights"] = {1.0f, 2.0f, 3.0f};
    j["metadata"] = json::object();
    return j;
}

TEST_CASE("Validator accepts valid version strings") {
    SECTION("accepts 0.5.x versions") {
        auto j = makeNamJson("0.5.0");
        REQUIRE(Validator::validateNam(j) == true);
        j["version"] = "0.5.4";
        REQUIRE(Validator::validateNam(j) == true);
    }

    SECTION("accepts 0.6.x versions") {
        auto j = makeNamJson("0.6.0");
        REQUIRE(Validator::validateNam(j) == true);
    }

    SECTION("accepts 0.7.x versions") {
        auto j = makeNamJson("0.7.0");
        REQUIRE(Validator::validateNam(j) == true);
    }

    SECTION("rejects non-0.x versions") {
        auto j = makeNamJson("1.0.0");
        REQUIRE(Validator::validateNam(j) == false);
        j["version"] = "2.0.0";
        REQUIRE(Validator::validateNam(j) == false);
    }

    SECTION("rejects missing or invalid version") {
        json j = makeNamJson("0.5.0");
        j.erase("version");
        REQUIRE(Validator::validateNam(j) == false);
        j["version"] = 123;
        REQUIRE(Validator::validateNam(j) == false);
    }

    SECTION("rejects missing required fields") {
        json j = makeNamJson("0.7.0");
        j.erase("architecture");
        REQUIRE(Validator::validateNam(j) == false);
        j["architecture"] = "Linear";
        j.erase("config");
        REQUIRE(Validator::validateNam(j) == false);
        j["config"] = json::object();
        j.erase("weights");
        REQUIRE(Validator::validateNam(j) == false);
    }
}

TEST_CASE("WeightScaler getHeadWeightIndices") {
    SECTION("Linear uses all weights") {
        json config = json::object();
        auto [start, end] = WeightScaler::getHeadWeightIndices("Linear", config, 5);
        REQUIRE(start == 0);
        REQUIRE(end == 5);
    }

    SECTION("LSTM uses last hidden_size weights") {
        json config;
        config["hidden_size"] = 3;
        auto [start, end] = WeightScaler::getHeadWeightIndices("LSTM", config, 10);
        REQUIRE(start == 7);
        REQUIRE(end == 10);
    }

    SECTION("ConvNet uses (channels * out_channels + out_channels) tail weights") {
        json config;
        config["channels"] = 8;
        config["out_channels"] = 1;
        // head size = 8*1 + 1 = 9
        auto [start, end] = WeightScaler::getHeadWeightIndices("ConvNet", config, 20);
        REQUIRE(start == 11);
        REQUIRE(end == 20);
    }

    SECTION("WaveNet uses last weight (head_scale)") {
        json config = json::object();
        auto [start, end] = WeightScaler::getHeadWeightIndices("WaveNet", config, 15);
        REQUIRE(start == 14);
        REQUIRE(end == 15);
    }

    SECTION("unsupported architecture throws") {
        json config = json::object();
        REQUIRE_THROWS(WeightScaler::getHeadWeightIndices("Unknown", config, 10));
    }
}

TEST_CASE("WeightScaler tryGetHeadWeightIndices errors") {
    size_t start, end;
    std::string err;

    SECTION("empty weights array") {
        json config = json::object();
        REQUIRE_FALSE(WeightScaler::tryGetHeadWeightIndices("Linear", config, 0, start, end, err));
        REQUIRE_FALSE(err.empty());
    }

    SECTION("LSTM missing hidden_size") {
        json config = json::object();
        REQUIRE_FALSE(WeightScaler::tryGetHeadWeightIndices("LSTM", config, 10, start, end, err));
        REQUIRE_FALSE(err.empty());
    }

    SECTION("LSTM hidden_size larger than weights") {
        json config;
        config["hidden_size"] = 20;
        REQUIRE_FALSE(WeightScaler::tryGetHeadWeightIndices("LSTM", config, 10, start, end, err));
        REQUIRE_FALSE(err.empty());
    }

    SECTION("ConvNet missing channels") {
        json config = json::object();
        config["out_channels"] = 1;
        REQUIRE_FALSE(WeightScaler::tryGetHeadWeightIndices("ConvNet", config, 20, start, end, err));
        REQUIRE_FALSE(err.empty());
    }

    SECTION("ConvNet missing out_channels") {
        json config = json::object();
        config["channels"] = 8;
        REQUIRE_FALSE(WeightScaler::tryGetHeadWeightIndices("ConvNet", config, 20, start, end, err));
        REQUIRE_FALSE(err.empty());
    }

    SECTION("ConvNet head size larger than weights") {
        json config;
        config["channels"] = 8;
        config["out_channels"] = 1;
        // head size = 9, weights = 5
        REQUIRE_FALSE(WeightScaler::tryGetHeadWeightIndices("ConvNet", config, 5, start, end, err));
        REQUIRE_FALSE(err.empty());
    }


    SECTION("unsupported architecture") {
        json config = json::object();
        REQUIRE_FALSE(WeightScaler::tryGetHeadWeightIndices("Unknown", config, 10, start, end, err));
        REQUIRE_FALSE(err.empty());
    }
}

TEST_CASE("WeightScaler scaleWeights") {
    SECTION("scales only the specified range") {
        std::vector<float> w = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        WeightScaler::scaleWeights(w, 1, 4, 2.0f);
        REQUIRE(w[0] == Catch::Approx(1.0f));
        REQUIRE(w[1] == Catch::Approx(4.0f));
        REQUIRE(w[2] == Catch::Approx(6.0f));
        REQUIRE(w[3] == Catch::Approx(8.0f));
        REQUIRE(w[4] == Catch::Approx(5.0f));
    }

    SECTION("factor 1.0 leaves weights unchanged") {
        std::vector<float> w = {1.0f, 2.0f, 3.0f};
        WeightScaler::scaleWeights(w, 0, 3, 1.0f);
        REQUIRE(w[0] == Catch::Approx(1.0f));
        REQUIRE(w[1] == Catch::Approx(2.0f));
        REQUIRE(w[2] == Catch::Approx(3.0f));
    }
}

TEST_CASE("MetadataUpdater updates gain metadata") {
    SECTION("updates loudness and output_level_dbu") {
        json meta;
        meta["loudness"] = -12.0f;
        meta["output_level_dbu"] = -12.0f;
        MetadataUpdater::updateMetadata(meta, 3.0f);
        REQUIRE(meta["loudness"] == -9.0f);
        REQUIRE(meta["output_level_dbu"] == -9.0f);
    }

    SECTION("handles missing metadata fields") {
        json meta;
        MetadataUpdater::updateMetadata(meta, 3.0f);
        REQUIRE(meta.contains("loudness") == false);
    }
}

TEST_CASE("WaveNet scales last weight (head_scale in weights array)") {
    SECTION("last weight is scaled, others unchanged") {
        std::vector<float> w = {0.1f, 0.2f, 0.3f, 0.02f};
        json config = json::object();
        auto [start, end] = WeightScaler::getHeadWeightIndices("WaveNet", config, w.size());
        WeightScaler::scaleWeights(w, start, end, 2.0f);
        REQUIRE(w[0] == Catch::Approx(0.1f));
        REQUIRE(w[1] == Catch::Approx(0.2f));
        REQUIRE(w[2] == Catch::Approx(0.3f));
        REQUIRE(w[3] == Catch::Approx(0.04f));
    }
}