# NAM Volume Knob

A cross-platform tool for adjusting the output volume of NeuralAmpModeler (.nam) files by scaling head weights. Preserves gain structure, distortion characteristics, and neural network behavior while boosting or attenuating output levels.

Note: This project was created with the help of AI-assisted coding tools.

Try it here: [https://mrgeneko.github.io/nam_volume_knob/](https://mrgeneko.github.io/nam_volume_knob/)

## Features

- **CLI Tool**: Command-line interface for batch processing with dB or linear gain options.
- **Web Interface**: Browser-based drag-and-drop tool (built with Emscripten) with single-click download.
- **Architecture Support**: Compatible with all NAM architectures (LSTM, WaveNet A2, ConvNet, Linear, and variants including SlimmableContainer).
- **Verified Scaling**: Audio processing test verifies output levels match expected dB gains.
- **Overwrite Prevention**: Automatic versioning (_v2, _v3, etc.) to avoid overwriting existing files.
- **Metadata Updates**: Adjusts `loudness` and `gain` metadata to reflect the new output level, ensuring the scaled output is accurately represented in the model.
- **Cross-Platform**: Works on macOS, Windows, and Linux.

## Installation

### Prerequisites

- CMake 3.10+
- C++20 compiler
- nlohmann/json (header-only, included)
- Catch2 (for tests, optional)

### Building the CLI

```bash
git clone <repo-url>
cd <repo-dir>
mkdir build && cd build
cmake ..
make
```

The executable `nam-volume-knob` will be in the `build/` directory.

### Web Version

Requires Emscripten SDK.

Note: you must serve the `web/` folder via a web server (opening `web/index.html` directly as a file often fails due to browser security restrictions around WASM/module loading).

#### Publish on GitHub Pages

This repo includes a GitHub Actions workflow that deploys the contents of `web/` to GitHub Pages.

1. In GitHub: **Settings → Pages → Source → GitHub Actions**
2. Push to `main` (or re-run the workflow in the **Actions** tab)

Your Pages URL will be:

`https://<username>.github.io/<repo>/`

After building with Emscripten, run:

```bash
python3 web/serve_local.py
```

Then open the printed URL in a modern browser.

## Usage

### CLI

```bash
# Boost by +3.5 dB
./nam-volume-knob --input model.nam --gain-db 3.5

# Attenuate by factor of 0.5
./nam-volume-knob --input model.nam --gain-linear 0.5

# Specify output file
./nam-volume-knob --input model.nam --gain-db -6.0 --output quieter.nam
```

#### Options

- `--input <file>`: Path to input .nam file (required).
- `--output <file>`: Path to output .nam file (optional; auto-generated if omitted).
- `--gain-db <float>`: Gain in dB (e.g., 3.5 for boost, -6.0 for cut; mutually exclusive with --gain-linear).
- `--gain-linear <float>`: Linear gain multiplier (e.g., 1.5 for 50% boost, 0.5 for 50% cut).

Filenames are auto-generated as `<basename>_+<gain>db.<ext>` or `<basename>_<gain>lin.<ext>`, with decimals replaced by underscores and trailing zeros removed.

### Web Interface

The most reliable way to run locally (correct directory, IPv4 bind for Safari, no-cache headers):

```bash
python3 web/serve_local.py
```

Note: the web UI vendors its dependencies (including `fflate`) under `web/vendor/` so it can run offline.

Fallback (works, but may cache assets and it’s easy to run from the wrong directory):

```bash
python3 -m http.server --directory web --bind 127.0.0.1 8000
```

1. Open the printed URL in a browser.
2. Select gain type: dB or Linear.
3. Enter gain value in the corresponding input field.
4. Drag and drop one or more .nam files.
5. The drop area shows the output filenames that will be created.
6. Click "Process and Download".
	- Single file: downloads the modified `.nam` directly.
	- Multiple files: downloads a single `.zip` containing all modified `.nam` files.

Filenames include the gain value and type suffix (e.g., `model_+3_0db.nam` or `model_1_5lin.nam`).

## Examples

- Original: `lstm.nam`
- +3.0 dB boost: `lstm_+3_0db.nam`
- Linear 1.5x gain: `lstm_1_5lin.nam`

## How It Works

The tool identifies the head/output scaling parameters for each architecture and scales them by the gain factor. This amplifies the final output without altering internal processing, preserving distortion and dynamics.

**Architecture-specific scaling:**
- **LSTM**: Scales last `hidden_size` weights (output layer)
- **WaveNet**: Scales `head_scale` parameter and output weights
- **ConvNet**: Scales output layer weights (`channels * out_channels + out_channels`)
- **Linear**: Scales all weights
- **SlimmableContainer**: Recursively scales each submodel

**Gain conversion:**
- **dB Mode**: Factor = 10^(gain/20)
- **Linear Mode**: Factor = gain value

**Metadata updates:**
- Updates `loudness` and `gain` metadata to reflect the new output level
- Critical for hosts/DAWs that use loudness metadata for output normalization—ensures the scaled output is accurately represented
- Maintains original model behavior while shifting output amplitude

## Limitations

- Input gain values are limited to float precision (~7 significant digits).
- Filenames capped at 7 decimal places to avoid OS limits.

## Testing

### Audio Processing Test

Verifies that volume scaling produces correct output levels by processing actual audio through NAM models using NeuralAmpModelerCore.

**Prerequisites:**
- Eigen3 library: `brew install eigen` (macOS) or `apt-get install libeigen3-dev` (Linux)

**Build and run:**

```bash
cd build
cmake -DNAM_VOLUME_KNOB_BUILD_AUDIO_TEST=ON ..
make audio_test
./audio_test
```

**Configuration:**

The test looks for model files in `$NAM_TEST_MODELS_DIR` or defaults to `$HOME/Downloads`:

```bash
./audio_test                    # Uses ~/Downloads
NAM_TEST_MODELS_DIR=/path ./audio_test  # Use custom path
```

**What it does:**
- Generates a 1kHz sine wave test signal
- Loads original and scaled NAM models
- Processes audio through each model
- Measures peak/RMS output levels
- Verifies that +6dB and +9dB files produce expected dB gains

**Example output:**
```
+6dB vs Original:
   Peak: 0.274909 (+6 dB, expected 6 dB)
   RMS:  0.137006 (+6 dB)
   ✓ PASS (within tolerance)
```

The test validates that the tool correctly scales model weights and metadata to produce audible volume changes.

### Unit Tests

Run unit tests with Catch2:

```bash
cd build
ctest
```

To build the tests target, configure with:

```bash
cmake -DNAM_VOLUME_KNOB_BUILD_TESTS=ON ..
```

## Contributing

Contributions welcome! Please test with various .nam files and architectures.

## License

MIT License. See LICENSE file.

## Related

Based on NeuralAmpModelerCore: https://github.com/sdatkinson/NeuralAmpModelerCore