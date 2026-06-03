#include <catch2/catch_all.hpp>
#include "validator.h"
#include "weight_scaler.h"
#include <vector>
#include <nlohmann/json.hpp>
#include <cmath>

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
    } else if (arch == "ConvNet") {
        j["config"]["channels"] = 8;
        j["config"]["out_channels"] = 16;
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

TEST_CASE("WeightScaler updateMetadata") {
    SECTION("updates loudness, gain, and output_level fields") {
        json model;
        model["metadata"]["loudness"] = -12.0f;
        model["metadata"]["gain"] = 0.0f;
        model["config"]["output_level"] = -6.0f;

        WeightScaler::updateMetadata(model, 6.0f);

        REQUIRE(model["metadata"]["loudness"] == Catch::Approx(-6.0f));
        REQUIRE(model["metadata"]["gain"] == Catch::Approx(6.0f));
        REQUIRE(model["config"]["output_level"] == Catch::Approx(0.0f));
    }

    SECTION("handles missing metadata fields safely") {
        json model;
        model["config"]["output_level"] = 0.0f;

        // Should not create metadata field if it doesn't exist
        WeightScaler::updateMetadata(model, 3.0f);
        REQUIRE(model["metadata"].is_null());
        REQUIRE(model["config"]["output_level"] == Catch::Approx(3.0f));
    }

    SECTION("handles missing config fields safely") {
        json model;
        model["metadata"]["loudness"] = 0.0f;

        // Should not throw or create config if it doesn't exist
        WeightScaler::updateMetadata(model, 3.0f);
        REQUIRE(model["metadata"]["loudness"] == Catch::Approx(3.0f));
    }
}

TEST_CASE("Validator rejects invalid weight values") {
    SECTION("rejects empty weights array") {
        auto j = makeNamJson("0.5.0");
        j["weights"] = json::array();
        REQUIRE(Validator::validateNam(j) == false);
    }

    SECTION("rejects non-numeric weights") {
        auto j = makeNamJson("0.5.0");
        j["weights"] = {1.0, "not a number", 3.0};
        REQUIRE(Validator::validateNam(j) == false);
    }
}

TEST_CASE("Validator rejects architecture-specific config violations") {
    SECTION("rejects LSTM without hidden_size") {
        auto j = makeNamJson("0.5.0", "LSTM");
        j["config"].erase("hidden_size");
        REQUIRE(Validator::validateNam(j) == false);
    }

    SECTION("rejects LSTM with non-integer hidden_size") {
        auto j = makeNamJson("0.5.0", "LSTM");
        j["config"]["hidden_size"] = 3.5;
        REQUIRE(Validator::validateNam(j) == false);
    }

    SECTION("rejects ConvNet without channels") {
        auto j = makeNamJson("0.5.0");
        j["architecture"] = "ConvNet";
        j["config"]["out_channels"] = 16;
        REQUIRE(Validator::validateNam(j) == false);
    }

    SECTION("rejects ConvNet without out_channels") {
        auto j = makeNamJson("0.5.0");
        j["architecture"] = "ConvNet";
        j["config"]["channels"] = 8;
        REQUIRE(Validator::validateNam(j) == false);
    }

    SECTION("rejects unknown architecture") {
        auto j = makeNamJson("0.5.0");
        j["architecture"] = "UnknownArch";
        REQUIRE(Validator::validateNam(j) == false);
    }
}

TEST_CASE("Validator rejects invalid version formats") {
    SECTION("rejects version without proper semantic format") {
        auto j = makeNamJson("0.5");  // Missing patch version
        REQUIRE(Validator::validateNam(j) == false);
    }

    SECTION("rejects version below 0.5.0") {
        auto j = makeNamJson("0.4.9");
        REQUIRE(Validator::validateNam(j) == false);
    }

    SECTION("rejects non-semantic version formats") {
        auto j = makeNamJson("0.5.0a");  // Invalid format
        REQUIRE(Validator::validateNam(j) == false);

        j["version"] = "0.5";  // Missing patch
        REQUIRE(Validator::validateNam(j) == false);
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

TEST_CASE("A2 (SlimmableContainer) weight scaling") {
    SECTION("scales submodels recursively") {
        json model;
        model["architecture"] = "SlimmableContainer";
        model["config"]["submodels"] = json::array();

        // Add first submodel (WaveNet)
        json submodel1;
        submodel1["max_value"] = 0.5f;
        submodel1["model"]["architecture"] = "WaveNet";
        submodel1["model"]["config"] = json::object();
        submodel1["model"]["weights"] = {0.1f, 0.2f, 0.02f};
        model["config"]["submodels"].push_back(submodel1);

        // Add second submodel (WaveNet)
        json submodel2;
        submodel2["max_value"] = 1.0f;
        submodel2["model"]["architecture"] = "WaveNet";
        submodel2["model"]["config"] = json::object();
        submodel2["model"]["weights"] = {0.3f, 0.4f, 0.03f};
        model["config"]["submodels"].push_back(submodel2);

        // Scale by factor 2.0
        WeightScaler::scaleA2Model(model, 2.0f);

        // Verify first submodel (last weight scaled)
        REQUIRE(model["config"]["submodels"][0]["model"]["weights"][0] == Catch::Approx(0.1f));
        REQUIRE(model["config"]["submodels"][0]["model"]["weights"][1] == Catch::Approx(0.2f));
        REQUIRE(model["config"]["submodels"][0]["model"]["weights"][2] == Catch::Approx(0.04f));

        // Verify second submodel (last weight scaled)
        REQUIRE(model["config"]["submodels"][1]["model"]["weights"][0] == Catch::Approx(0.3f));
        REQUIRE(model["config"]["submodels"][1]["model"]["weights"][1] == Catch::Approx(0.4f));
        REQUIRE(model["config"]["submodels"][1]["model"]["weights"][2] == Catch::Approx(0.06f));
    }

    SECTION("tryScaleA2Model returns false for invalid input") {
        json model;
        model["architecture"] = "SlimmableContainer";
        // Missing config
        std::string err;
        REQUIRE_FALSE(WeightScaler::tryScaleA2Model(model, 2.0f, err));
        REQUIRE_FALSE(err.empty());
    }

    SECTION("SlimmableContainer in tryGetHeadWeightIndices returns error") {
        json config = json::object();
        size_t start, end;
        std::string err;
        REQUIRE_FALSE(WeightScaler::tryGetHeadWeightIndices("SlimmableContainer", config, 10, start, end, err));
        REQUIRE(err.find("SlimmableContainer") != std::string::npos);
    }
}