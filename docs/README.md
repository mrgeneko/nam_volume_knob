# NAM Volume Knob

## CLI Usage

Build the project with CMake, then run:

```
nam-volume-knob --input model.nam --gain-db 3.5
```

Options:
- `--input <path>`: Input .nam file
- `--output <path>`: Output file (optional, auto-generated)
- `--gain-db <float>`: Gain in dB
- `--gain-linear <float>`: Linear gain factor

## Web Usage

The easiest/most reliable way to run the Web UI locally:

```
python3 web/serve_local.py
```

Then:

1. Open the printed URL.
2. Choose gain type (dB or Linear) and enter a value.
3. Drop one or more `.nam` files.
4. Click “Process and Download”.
	- Single file: downloads the modified `.nam`.
	- Multiple files: downloads a single `.zip` containing all modified `.nam` files.

## Installation
Browser build artifacts: `nam-volume-knob-web.js` + `nam-volume-knob-web.wasm`
For CLI: Build with CMake and make.

For Web: Use Emscripten to build Wasm.