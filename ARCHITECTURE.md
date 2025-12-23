# Architecture

This repository contains **NAM Volume Knob**: a small tool that applies a gain change to Neural Amp Modeler `.nam` model files.

It provides two front-ends backed by the same C++ processing code:

- **CLI** (native executable) for batch/offline processing.
- **Web UI** (HTML/JS + WebAssembly) for in-browser drag-and-drop processing.

## Repository Layout

- `src/`: C++ implementation
  - `nam_parser.cpp`: parse `.nam` JSON
  - `validator.cpp`: validate expected shape/version
  - `weight_scaler.cpp`: apply gain factor to the model output/head weights
  - `metadata_updater.cpp`: update metadata (loudness/output level) to reflect gain
  - `cli.cpp`, `main.cpp`: CLI argument parsing + filesystem I/O
  - `web_bindings.cpp`: Emscripten/Embind exports used by the browser
- `include/`: public/internal headers
- `tests/`: Catch2 unit tests
- `web/`: static web app
  - `index.html`, `styles.css`, `app.js`
  - `nam-volume-knob-web.js/.wasm`: Emscripten build output
  - `serve_local.py`: no-cache IPv4 dev server
  - `vendor/fflate-0.8.2-umd.js`: vendored zip library for multi-file downloads

## Core Data Model

A `.nam` file is JSON. The tool treats it as an immutable input and produces a new JSON document:

1. Parse JSON using `nlohmann::json`.
2. Compute gain factor:
   - dB mode: $\text{factor} = 10^{\frac{\text{dB}}{20}}$
   - linear mode: $\text{factor} = \text{linear}$
3. Scale the appropriate weight arrays (the model “head” / output stage) by `factor`.
4. Update metadata fields so downstream hosts have correct loudness/level info.
5. Serialize JSON back to text.

## CLI Flow

- Inputs: one `.nam` file path + either `--gain-db` or `--gain-linear`.
- Steps:
  - Read file from disk.
  - Parse + validate JSON.
  - Transform weights + metadata.
  - Write output `.nam`.
  - Prevent overwrites by versioning output names when needed.

## Web Flow

The web UI is fully client-side.

- `web/index.html` loads:
  - the Emscripten module (`nam-volume-knob-web.js` + `.wasm`)
  - UI code (`web/app.js`)
  - zip support (`web/vendor/fflate-0.8.2-umd.js`)

- `web/app.js`:
  - handles drag-and-drop of `.nam` files
  - shows output filename previews immediately on drop
  - calls the Wasm-exposed function (via Embind) to process each file’s JSON
  - triggers downloads:
    - 1 file: downloads `.nam` directly
    - many files: creates one `.zip` and downloads it

## Local Serving

WebAssembly and module loading works best when served over HTTP.

- Recommended: `python3 web/serve_local.py`
  - serves the correct directory regardless of current working directory
  - binds to `127.0.0.1` (helps with Safari/localhost quirks)
  - sets no-cache headers to avoid stale JS/CSS while iterating

## Build Notes

- Native build: CMake generates the `nam-volume-knob` executable.
- Web build: when `EMSCRIPTEN` is enabled, CMake builds `nam-volume-knob-web` (emits `.js` + `.wasm`) for the `web/` UI to load.
