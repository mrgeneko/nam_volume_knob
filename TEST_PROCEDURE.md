# NAM Volume Knob Test Procedure

## Objective
Verify that the volume scaling fix for WaveNet models works correctly by processing audio through the original and scaled NAM files.

## Setup Requirements
- DAW with NAM plugin support (e.g., Studio One, Cubase, etc.)
- Spectrum analyzer plugin with peak level display (decibels)
- Test files in `/Users/gene/Downloads/`:
  - `Deluxe Reverb.nam` (original)
  - `Deluxe Reverb_+6_0db.nam` (scaled version)
  - `Deluxe Reverb_+9_0db.nam` (scaled version)

## Test Configuration
1. Open your DAW
2. Create a new track with the NAM plugin inserted
3. Disable normalization/calibration on the Spectrum plugin
4. Ensure your test audio is at a consistent level

## Test Procedure

### Test 1: +9dB Verification

1. **Load original model**: Insert `Deluxe Reverb.nam`
2. **Play test audio**: Play a sine wave (1kHz) or your standard test audio
3. **Record peak level**: Note the peak value shown in Spectrum plugin (e.g., -10 dB)
4. **Switch to scaled model**: Replace with `Deluxe Reverb_+9_0db.nam`
5. **Replay same audio**: Play the identical audio at the same input level
6. **Record new peak level**: Note the peak value (should be ~9dB higher)

**Expected Result**: 
- If original peak = -10 dB
- Then +9dB version peak ≈ -1 dB
- Tolerance: ±1 dB (model dynamics may compress slightly)

**Pass Criteria**: Peak level increase is within 8-10 dB

---

### Test 2: +6dB Verification

Repeat the same procedure with `Deluxe Reverb_+6_0db.nam`

**Expected Result**:
- Peak level increase: 5-7 dB (±1 dB tolerance)

**Pass Criteria**: Peak level increase is within 5-7 dB

---

## What to Look For

✅ **PASS**: Output levels increase by approximately the expected dB amount
❌ **FAIL**: Output levels remain unchanged or change unpredictably

If tests FAIL:
- The fix may not have taken effect
- The NAM plugin implementation may use a different code path
- Check that the updated binary is being used
- Verify the +6dB and +9dB files were generated with the fixed tool

## Notes

- Models with compression/saturation may not show exact dB increases
- The 1 dB tolerance accounts for neural network non-linearity
- Test with multiple audio types if possible (sine, music, noise)
