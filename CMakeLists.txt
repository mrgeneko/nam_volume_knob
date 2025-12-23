cmake_minimum_required(VERSION 3.10)
project(nam-volume-knob VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(include)
include_directories(third_party)

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
    add_executable(nam-volume-knob-web src/main.cpp ${SOURCES} src/web_bindings.cpp)
    set_target_properties(nam-volume-knob-web PROPERTIES SUFFIX ".js")
    target_link_libraries(nam-volume-knob-web --bind)
endif()

# Tests
option(NAM_VOLUME_KNOB_BUILD_TESTS "Build Catch2 unit tests" OFF)
if(NAM_VOLUME_KNOB_BUILD_TESTS AND NOT EMSCRIPTEN)
    enable_testing()
    find_package(Catch2 QUIET)
    if(Catch2_FOUND)
        add_executable(tests tests/test_main.cpp ${SOURCES})
        target_link_libraries(tests Catch2::Catch2)
        target_include_directories(tests PRIVATE third_party)
    else()
        message(WARNING "Catch2 not found; tests target will not be built. Set Catch2_DIR or install Catch2.")
    endif()
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