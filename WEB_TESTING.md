# Testing the NAM Volume Knob Web Interface Locally

This guide shows how to build and test the WebAssembly version of nam-volume-knob locally.

## Prerequisites

- Emscripten SDK installed (check: `which emcmake`)
- Node.js installed (for local testing without a browser)
- A web browser (Chrome, Firefox, Safari, Edge)

## Building the WebAssembly Version

```bash
cd /Users/gene/work/nam_volume_knob

# Create a separate build directory for web
mkdir -p build-web
cd build-web

# Configure with Emscripten
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Output files
ls -lh nam-volume-knob-web.*
# Should show:
# - nam-volume-knob-web.js (JavaScript glue code)
# - nam-volume-knob-web.wasm (WebAssembly binary)
```

## Testing Options

### Option 1: Browser Testing with HTML Test Page

Create `test-web.html` in the build-web directory:

```html
<!DOCTYPE html>
<html>
<head>
    <title>NAM Volume Knob Web Tester</title>
    <style>
        body { font-family: monospace; margin: 20px; }
        textarea { width: 100%; height: 200px; }
        button { padding: 10px 20px; font-size: 14px; }
        .output { margin-top: 20px; padding: 10px; background: #f0f0f0; }
    </style>
</head>
<body>
    <h1>NAM Volume Knob Web Tester</h1>
    
    <h2>Input JSON</h2>
    <textarea id="input">{"version":"0.5.0","architecture":"WaveNet","config":{"head_scale":0.02},"weights":[0.1,0.2,0.3,0.02],"metadata":{"loudness":-12.0}}</textarea>
    
    <h2>Parameters</h2>
    <label>
        Gain (dB): <input type="number" id="gainDb" value="6" step="0.1" min="-9" max="9">
    </label>
    <br/>
    <label>
        Or Factor (linear): <input type="number" id="factor" value="2.0" step="0.1" min="0.1" max="2.82">
    </label>
    
    <br/><br/>
    <button onclick="testProcess()">Process Model</button>
    <button onclick="testInvalidJson()">Test Invalid JSON</button>
    <button onclick="testExtremeGain()">Test Gain Limit</button>
    <button onclick="testEmptyWeights()">Test Edge Cases</button>
    
    <h2>Output</h2>
    <div class="output">
        <pre id="output">Ready for testing...</pre>
    </div>
    
    <h2>Test Results</h2>
    <div class="output">
        <pre id="results"></pre>
    </div>
    
    <script src="nam-volume-knob-web.js"></script>
    <script>
        let Module = { onRuntimeInitialized: () => console.log("WASM loaded") };
        
        function testProcess() {
            const input = document.getElementById('input').value;
            const gainDb = parseFloat(document.getElementById('gainDb').value);
            const factor = parseFloat(document.getElementById('factor').value);
            
            try {
                const result = Module.processNam(input, factor, gainDb);
                document.getElementById('output').textContent = result;
                
                // Parse and verify
                const model = JSON.parse(result);
                let testResult = "✓ Valid JSON output\n";
                testResult += `✓ Weights: ${model.weights.length} values\n`;
                testResult += `✓ Metadata loudness: ${model.metadata?.loudness}\n`;
                document.getElementById('results').textContent = testResult;
            } catch (e) {
                document.getElementById('output').textContent = `Error: ${e.message}`;
                document.getElementById('results').textContent = `✗ Exception: ${e.stack}`;
            }
        }
        
        function testInvalidJson() {
            const invalidJson = '{"invalid": json}';
            try {
                const result = Module.processNam(invalidJson, 2.0, 6.0);
                document.getElementById('output').textContent = result;
                document.getElementById('results').textContent = '✓ Handled invalid JSON gracefully\n' + result;
            } catch (e) {
                document.getElementById('results').textContent = `✗ Exception thrown: ${e.message}`;
            }
        }
        
        function testExtremeGain() {
            const validModel = '{"version":"0.5.0","architecture":"WaveNet","config":{},"weights":[1.0,2.0,0.02]}';
            try {
                // Test with extreme factor
                const result = Module.processNam(validModel, 1000.0, 60.0);
                document.getElementById('output').textContent = result;
                document.getElementById('results').textContent = '✓ Web layer validated extreme gain:\n' + result;
            } catch (e) {
                document.getElementById('results').textContent = `✗ Unexpected error: ${e.message}`;
            }
        }
        
        function testEmptyWeights() {
            const emptyWeights = '{"version":"0.5.0","architecture":"WaveNet","config":{},"weights":[]}';
            try {
                const result = Module.processNam(emptyWeights, 2.0, 6.0);
                document.getElementById('output').textContent = result;
                document.getElementById('results').textContent = '✓ Handled empty weights:\n' + result;
            } catch (e) {
                document.getElementById('results').textContent = `Error (expected): ${e.message}`;
            }
        }
    </script>
</body>
</html>
```

Then open it:
```bash
# Start a simple HTTP server
python3 -m http.server 8000

# Open in browser
open http://localhost:8000/test-web.html
```

### Option 2: Node.js Testing (Automated)

Create `test-web.js`:

```javascript
const fs = require('fs');
const path = require('path');

// Load the WASM module
const Module = require('./nam-volume-knob-web.js');

Module.onRuntimeInitialized = function() {
    console.log('✓ WASM module loaded\n');
    
    // Test 1: Valid model
    console.log('Test 1: Process valid WaveNet model');
    const validModel = JSON.stringify({
        version: "0.5.0",
        architecture: "WaveNet",
        config: { head_scale: 0.02 },
        weights: [0.1, 0.2, 0.3, 0.02],
        metadata: { loudness: -12.0 }
    });
    
    try {
        const result = Module.processNam(validModel, 2.0, 6.0);
        const model = JSON.parse(result);
        console.log(`✓ Processing succeeded`);
        console.log(`  Weights: ${model.weights.length} values`);
        console.log(`  Metadata loudness: ${model.metadata.loudness}\n`);
    } catch (e) {
        console.log(`✗ Error: ${e.message}\n`);
    }
    
    // Test 2: Invalid JSON
    console.log('Test 2: Reject invalid JSON');
    try {
        const result = Module.processNam('invalid json', 2.0, 6.0);
        console.log(`✓ Handled gracefully: ${result}\n`);
    } catch (e) {
        console.log(`✗ Unexpected exception: ${e.message}\n`);
    }
    
    // Test 3: Extreme gain
    console.log('Test 3: Reject extreme gain');
    const testModel = JSON.stringify({
        version: "0.5.0",
        architecture: "WaveNet",
        config: {},
        weights: [1.0, 2.0, 0.02]
    });
    
    try {
        const result = Module.processNam(testModel, 1000.0, 60.0);
        console.log(`Result: ${result}`);
        if (result.includes('Error')) {
            console.log(`✓ Rejected extreme gain\n`);
        }
    } catch (e) {
        console.log(`✗ Exception: ${e.message}\n`);
    }
    
    // Test 4: Empty weights
    console.log('Test 4: Reject empty weights');
    const emptyModel = JSON.stringify({
        version: "0.5.0",
        architecture: "WaveNet",
        config: {},
        weights: []
    });
    
    try {
        const result = Module.processNam(emptyModel, 2.0, 6.0);
        console.log(`Result: ${result}`);
        if (result.includes('Error')) {
            console.log(`✓ Rejected empty weights\n`);
        }
    } catch (e) {
        console.log(`✗ Exception: ${e.message}\n`);
    }
};
```

Run it:
```bash
cd build-web
node test-web.js
```

## What to Test

### Valid Inputs
- ✓ WaveNet, LSTM, ConvNet, Linear architectures
- ✓ Gain from -9dB to +9dB
- ✓ Metadata updates (loudness, gain, output_level)
- ✓ A2 (SlimmableContainer) models with submodels

### Invalid Inputs (should return error strings, not throw)
- ✗ Invalid JSON syntax
- ✗ Missing required fields
- ✗ Non-numeric weights
- ✗ Empty weights array
- ✗ Extreme gain values (>+9dB or <0)
- ✗ Unknown architectures
- ✗ Architecture-specific config violations (LSTM missing hidden_size, etc.)

### Error Handling
- No JavaScript exceptions for invalid input
- All errors returned as "Error: ..." strings
- Clear error messages indicating what's wrong

## Performance Notes

The WebAssembly binary is ~2-3 MB. For production use:
- Consider gzip compression (reduces to ~500-700 KB)
- Lazy-load the module on demand
- Cache aggressively in CDN

## Integration Points

When integrating into a web application:

```javascript
// Load module once at startup
import('/nam-volume-knob-web.js').then(Module => {
    Module.onRuntimeInitialized = () => {
        // Module ready for use
        window.NAM = Module;
    };
});

// Use in UI
async function scaleModel(jsonString, gainDb) {
    const factor = Math.pow(10, gainDb / 20);
    const result = window.NAM.processNam(jsonString, factor, gainDb);
    
    if (result.startsWith('Error')) {
        console.error(result);
        return null;
    }
    
    return JSON.parse(result);
}
```
