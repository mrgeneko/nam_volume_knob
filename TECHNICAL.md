# NAM Volume Knob — Technical Documentation

## Architecture Overview

This tool scales the output volume of Neural Amp Modeler (NAM) files by modifying model weights and associated metadata. The approach differs by model architecture because NAM architectures serialize weights in different orders and use different mechanisms for output control.

## Model Architectures & Weight Scaling

### A1 Models: WaveNet, LSTM, ConvNet, Linear

Each A1 architecture stores weights in a flat array with a specific serialization order. Output scaling targets a subset of these weights depending on the architecture:

#### WaveNet
- **Scaling target:** Last weight in the array (index `weightsSize - 1`)
- **Why:** The last weight is `head_scale`, a scalar multiplier applied to the model's final output
- **Implementation:** `src/weight_scaler.cpp`, lines 30-34

#### LSTM
- **Scaling target:** Last `hidden_size` weights (indices `weightsSize - hidden_size` through `weightsSize - 1`)
- **Why:** These are the output layer weights; scaling them affects output amplitude
- **Config dependency:** Reads `hidden_size` from model config to determine the range
- **Implementation:** `src/weight_scaler.cpp`, lines 11-28

#### ConvNet
- **Scaling target:** Last `(channels × out_channels) + out_channels` weights
- **Why:** These form the output convolution layer
- **Config dependencies:** Reads `channels` and `out_channels` from model config
- **Implementation:** `src/weight_scaler.cpp`, lines 36-59
- **Formula derivation:** Based on NeuralAmpModelerCore's internal weight ordering

#### Linear
- **Scaling target:** All weights (index 0 through `weightsSize - 1`)
- **Why:** Linear models have no internal structure; every weight contributes equally to output
- **Implementation:** `src/weight_scaler.cpp`, lines 61-65

### A2 Models: SlimmableContainer

SlimmableContainer is a wrapper that holds multiple A1 submodels (typically 2-4) at different complexity levels for efficient inference. When scaling a SlimmableContainer:

1. **Recursive descent:** `tryScaleA2Model()` iterates over all submodels and calls itself recursively on each
2. **Submodel scaling:** Each submodel (e.g., WaveNet) is scaled using the appropriate A1 logic
3. **Metadata at two levels:**
   - Submodel metadata (loudness, gain) updated by the recursive call
   - Container-level metadata (if present) also updated
4. **Implementation:** `src/weight_scaler.cpp`, lines 100-137

**Why multiple submodels?** SlimmableContainer allows hosts to trade quality for latency. All submodels should produce the same output (at different precision/quality levels), so all must be scaled consistently.

## Metadata Updates: Loudness and Gain

### Why Metadata Matters

NAM hosts (like Chainsmith FX) support loudness-based normalization. When loading a model, the host reads the `loudness` metadata field and calculates a gain compensation factor:

```
host_gain = 10^(target_loudness - model_loudness) / 20
final_output = model_output × host_gain
```

**Problem:** If weights are scaled +6dB but metadata is not updated, the host's normalization will apply a -6dB compensation, **canceling out the weight scaling entirely**. The user hears no volume change.

**Solution:** Update both weights and metadata together:
- Scale weights by factor (e.g., 1.995× for +6dB)
- Add `20 × log10(factor)` to loudness metadata
- This ensures the host's normalization won't undo the scaling

### Fields Updated

For all A1 models (lines 167-180 in `src/weight_scaler.cpp`):
- `metadata.loudness` — increased by dB gain
- `config.output_level` — increased by dB gain (if present)

For A2 SlimmableContainer (lines 121-136):
- Container-level metadata and config updated
- Submodel metadata updated recursively

**Not updated:**
- `config.input_level` — input scaling is independent of output scaling; modifying it would change input gain behavior, not output volume

## Weight Scaling Mechanics

### Gain Conversion

**dB to linear factor:**
```
factor = 10^(gain_db / 20)
```
Example: +6dB = 10^(6/20) ≈ 1.995×

**Linear factor to dB:**
```
gain_db = 20 × log10(factor)
```
Example: 2.0× = 20 × log10(2) ≈ +6.02dB

### The Double-Scaling Regression

**Historical issue (fixed in commit 135e7fe):**

WaveNet has `head_scale` stored in two places:
1. In the weights array as the last element
2. In the config as a separate `head_scale` field

Earlier code scaled both:
```cpp
// OLD CODE (INCORRECT)
scaleWeights(weights, start, end, factor);  // scales weights[last] 
model["weights"] = weights;
model["config"]["head_scale"] = model["config"]["head_scale"] * factor;  // scales again!
```

This caused +6dB requests to produce +12dB in audio output (gain applied twice).

**Fix:** Remove the config scaling; rely only on the weights array:
```cpp
// NEW CODE (CORRECT)
scaleWeights(weights, start, end, factor);  // scales weights[last] only
model["weights"] = weights;
// head_scale config left unchanged; it's already in the weights array
```

**Lesson:** When the same parameter exists in multiple places, verify which one is used at inference time. NeuralAmpModelerCore uses the weights array; the config field is informational.

## Testing

### Audio Processing Test

`tests/audio_test.cpp` validates scaling by processing actual audio through models:

1. **Generate test signal:** 1 kHz sine wave, 2 seconds, 48 kHz
2. **Load three models:** original, +6dB scaled, +9dB scaled
3. **Process audio:** Feed through each model's DSP pipeline
4. **Measure output:** Peak and RMS levels
5. **Compare to expected:** Check that +6dB model produces +6dB gain, ±1dB tolerance

**Why 1dB tolerance?** Neural network models aren't perfectly linear. A +6dB weight scaling typically produces +5.5dB to +6.5dB audio gain depending on signal characteristics and model dynamics.

**Running the test:**
```bash
cd build
cmake -DNAM_VOLUME_KNOB_BUILD_AUDIO_TEST=ON ..
make audio_test
NAM_TEST_MODELS_DIR=/path/to/models ./audio_test
```

**Expected output:**
```
+6dB vs Original:
   Peak: X.XXX (+6 dB, expected 6 dB)
   RMS:  X.XXX (+6 dB)
   ✓ PASS (within tolerance)
```

### Verifying a Fix

When modifying weight scaling logic:
1. Regenerate scaled models with the fixed code
2. Run audio_test on both A1 (WaveNet) and A2 (SlimmableContainer) models
3. Both must produce correct gains within 1dB tolerance
4. Check that metadata fields were updated correctly (use Python: `json.load()` and inspect `loudness`, `gain`, `output_level` fields)

## Limitations

### Gain Range
- Maximum boost: +9dB (linear ≈ 2.82×)
- Rationale: Prevent user from creating models that distort/clip in most DAWs
- Config: `src/cli.cpp`, lines 18-19

### Output Filename Format
- Decimal places limited to 7 to avoid OS filename length limits
- Decimals replaced with underscores (e.g., `+6_5db` for 6.5dB)
- Trailing zeros removed (e.g., `+6db` not `+6_0db`)

### Float Precision
- Weights stored as 32-bit float in NAM format
- Gain values also limited to ~7 significant digits
- Sufficient for typical use; scientific applications requiring higher precision would need architecture changes

## Design Decisions

### Why Scale Weights, Not Config Parameters?

WaveNet's `head_scale` could theoretically be scaled in the config instead:
```cpp
// Alternative: modify config instead of weights
model["config"]["head_scale"] *= factor;
```

We don't do this because:
1. **Consistency:** All architectures scale weights; using config would be architecture-specific
2. **Correctness:** NeuralAmpModelerCore reads weights for inference; config fields are largely informational
3. **Metadata coupling:** We update loudness metadata to match weight scaling. If config field were used, the same loudness update would apply differently at inference time, causing confusion

### Why Update Loudness Separately from Weight Scaling?

Couldn't the host just detect that weights were modified and compensate automatically?

No, because:
1. **Host assumption:** Hosts assume the loudness field is accurate. They read it and trust it.
2. **Serialization:** Once saved, there's no record of "these weights were scaled; apply compensation." The file stands alone.
3. **Portability:** A scaled model should sound the same in any host (with or without loudness normalization).

### Why Not Modify Input Level?

Input level affects how loudly the signal is fed into the model. Scaling output weights doesn't change how the model processes input; it only scales the final output. Modifying input level would change the model's internal signal levels and potentially affect distortion/saturation characteristics.

## File Formats

### NAM File Structure

NAM files are ZIP archives (despite `.nam` extension) containing:
- `model.json` — model configuration, weights, metadata
- Optional architecture-specific auxiliary data

When reading: `.nam` → unzip → parse `model.json`

The JSON structure:
```json
{
  "version": "0.5.4",
  "architecture": "WaveNet" or "SlimmableContainer" or ...,
  "config": {
    "hidden_size": 16,
    "head_scale": 0.02,
    ...
  },
  "metadata": {
    "loudness": -16.5,
    "gain": 2.3,
    "date": {...},
    ...
  },
  "weights": [0.123, -0.456, ...]
}
```

### Metadata Fields

- `loudness` — perceived loudness in dB (LUFS-like)
- `gain` — gain applied at training time (informational)
- `date` — training date
- `source_file` — input audio filename
- Other fields are architecture-specific

## Future Work

### Potential Improvements

1. **Per-submodel gain:** Allow different gains for different complexity levels in SlimmableContainer
2. **Frequency-dependent scaling:** Scale treble/bass independently (would require architecture-specific DSP)
3. **Batch processing:** Faster CLI for scaling many files at once
4. **Validation:** Verify scaled models via audio_test before saving (catch errors early)

### Testing Infrastructure

Current test only validates A1 and A2 models with +6dB and +9dB. Future test coverage could include:
- Edge cases: +0dB, +0.1dB, +9dB boundary
- All architectures: ensure ConvNet and LSTM scale correctly
- Metadata consistency: verify loudness/gain are always updated together

## References

- [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore) — NAM DSP implementation
- NAM file format: defined in NeuralAmpModelerCore's model loaders
- LUFS/loudness standard: ITU-R BS.1770-4 (referenced by NAM loudness field, though exact calculation is model-specific)
