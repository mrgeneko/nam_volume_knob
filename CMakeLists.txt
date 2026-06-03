cmake_minimum_required(VERSION 3.10)
project(nam-volume-knob VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(include)
include_directories(third_party)
include_directories(third_party/NeuralAmpModelerCore)

# Find Eigen (required by NeuralAmpModelerCore)
find_package(Eigen3 REQUIRED)

# Source files
set(SOURCES
    src/nam_parser.cpp
    src/weight_scaler.cpp
    src/metadata_updater.cpp
    src/validator.cpp
    src/cli.cpp
)

# CLI executable
add_executable(nam-volume-knob src/main.cpp ${SOURCES})
target_include_directories(nam-volume-knob PRIVATE third_party)

# For web (Emscripten)
if(EMSCRIPTEN)
    add_executable(nam-volume-knob-web ${SOURCES} src/web_bindings.cpp)
    set_target_properties(nam-volume-knob-web PROPERTIES SUFFIX ".js")
    target_link_libraries(nam-volume-knob-web --bind)
    # Keep web UX friendly: prevent hard aborts on thrown exceptions.
    target_link_options(nam-volume-knob-web PRIVATE "-sDISABLE_EXCEPTION_CATCHING=0")
endif()

# Tests
option(NAM_VOLUME_KNOB_BUILD_TESTS "Build Catch2 unit tests" OFF)
if(NAM_VOLUME_KNOB_BUILD_TESTS AND NOT EMSCRIPTEN)
    enable_testing()
    find_package(Catch2 QUIET)
    if(Catch2_FOUND)
        add_executable(tests tests/test_main.cpp ${SOURCES})
        target_link_libraries(tests Catch2::Catch2WithMain)
        target_include_directories(tests PRIVATE third_party)
    else()
        message(WARNING "Catch2 not found; tests target will not be built. Set Catch2_DIR or install Catch2.")
    endif()
endif()

# Audio processing test
option(NAM_VOLUME_KNOB_BUILD_AUDIO_TEST "Build audio processing test using NeuralAmpModelerCore" OFF)
if(NAM_VOLUME_KNOB_BUILD_AUDIO_TEST)
    file(GLOB NAM_CORE_SOURCES
        "third_party/NeuralAmpModelerCore/NAM/*.cpp"
        "third_party/NeuralAmpModelerCore/NAM/wavenet/*.cpp")
    add_executable(audio_test tests/audio_test.cpp ${NAM_CORE_SOURCES})
    target_link_libraries(audio_test Eigen3::Eigen)
    target_include_directories(audio_test PRIVATE
        third_party
        third_party/NeuralAmpModelerCore
        third_party/NeuralAmpModelerCore/Dependencies
        third_party/NeuralAmpModelerCore/Dependencies/nlohmann)
    target_compile_options(audio_test PRIVATE -O2)
    target_compile_definitions(audio_test PRIVATE NAM_ENABLE_A2_FAST)
endif()

# Platform-specific flags
if(UNIX AND NOT APPLE)
    # Linux
    target_compile_options(nam-volume-knob PRIVATE -O2)
elseif(APPLE)
    # macOS
    target_compile_options(nam-volume-knob PRIVATE -O2)
elseif(WIN32)
    # Windows
    target_compile_options(nam-volume-knob PRIVATE /O2)
endif()