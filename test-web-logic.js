#!/usr/bin/env node

/**
 * Web Bindings Logic Tests
 *
 * These tests validate the logic in web_bindings.cpp by simulating
 * what the C++ processNam() function would do with various inputs.
 *
 * Usage: node test-web-logic.js
 */

const assert = require('assert');

// Constants from web_bindings.cpp
const MAX_GAIN_DB = 9.0;
const MAX_GAIN_LINEAR = 2.8183829312644537; // pow(10, 9/20)

class WebBindingsLogicTest {
    constructor() {
        this.passed = 0;
        this.failed = 0;
        this.tests = [];
    }

    test(name, fn) {
        this.tests.push({ name, fn });
    }

    run() {
        console.log('Running Web Bindings Logic Tests\n');
        console.log('='.repeat(60) + '\n');

        for (const { name, fn } of this.tests) {
            try {
                fn();
                console.log(`✓ ${name}`);
                this.passed++;
            } catch (e) {
                console.log(`✗ ${name}`);
                console.log(`  Error: ${e.message}\n`);
                this.failed++;
            }
        }

        console.log('\n' + '='.repeat(60));
        console.log(`Results: ${this.passed} passed, ${this.failed} failed`);
        console.log(`Total: ${this.tests.length} tests\n`);

        return this.failed === 0;
    }
}

const test = new WebBindingsLogicTest();

// ============================================================================
// INPUT VALIDATION TESTS
// ============================================================================

test.test('Rejects invalid JSON syntax', () => {
    const input = '{"invalid": json}';
    const error = validateAndParse(input);
    assert(error.includes('Error'), `Expected error message, got: ${error}`);
});

test.test('Accepts valid model JSON', () => {
    const input = JSON.stringify({
        version: "0.5.0",
        architecture: "WaveNet",
        config: {},
        weights: [1.0, 2.0, 0.02],
        metadata: { loudness: -12.0 }
    });
    const error = validateAndParse(input);
    assert(!error, `Should not error on valid JSON, got: ${error}`);
});

// ============================================================================
// VALIDATOR TESTS
// ============================================================================

test.test('Rejects missing version field', () => {
    const input = JSON.stringify({
        architecture: "WaveNet",
        config: {},
        weights: [1.0, 2.0, 0.02]
    });
    assert(!Validator.validateNam(JSON.parse(input)), 'Should reject missing version');
});

test.test('Rejects version below 0.5.0', () => {
    const input = JSON.stringify({
        version: "0.4.9",
        architecture: "WaveNet",
        config: {},
        weights: [1.0, 2.0, 0.02]
    });
    assert(!Validator.validateNam(JSON.parse(input)), 'Should reject version < 0.5.0');
});

test.test('Accepts version 0.5.0 and above', () => {
    for (const version of ["0.5.0", "0.6.0", "0.10.5"]) {
        const input = {
            version,
            architecture: "WaveNet",
            config: {},
            weights: [1.0, 2.0, 0.02]
        };
        assert(Validator.validateNam(input), `Should accept version ${version}`);
    }
});

test.test('Rejects invalid version format', () => {
    for (const version of ["0.5", "0.5.0a", "1.0.0", "invalid"]) {
        const input = {
            version,
            architecture: "WaveNet",
            config: {},
            weights: [1.0, 2.0, 0.02]
        };
        assert(!Validator.validateNam(input), `Should reject version ${version}`);
    }
});

test.test('Rejects empty weights array', () => {
    const input = {
        version: "0.5.0",
        architecture: "WaveNet",
        config: {},
        weights: []
    };
    assert(!Validator.validateNam(input), 'Should reject empty weights');
});

test.test('Rejects non-numeric weights', () => {
    const input = {
        version: "0.5.0",
        architecture: "WaveNet",
        config: {},
        weights: [1.0, "not a number", 0.02]
    };
    assert(!Validator.validateNam(input), 'Should reject non-numeric weights');
});

test.test('Rejects LSTM without hidden_size', () => {
    const input = {
        version: "0.5.0",
        architecture: "LSTM",
        config: { num_layers: 1 },
        weights: [1.0, 2.0, 0.02]
    };
    assert(!Validator.validateNam(input), 'Should reject LSTM without hidden_size');
});

test.test('Rejects ConvNet without channels', () => {
    const input = {
        version: "0.5.0",
        architecture: "ConvNet",
        config: { out_channels: 16 },
        weights: [1.0, 2.0, 0.02]
    };
    assert(!Validator.validateNam(input), 'Should reject ConvNet without channels');
});

test.test('Rejects unknown architecture', () => {
    const input = {
        version: "0.5.0",
        architecture: "UnknownArch",
        config: {},
        weights: [1.0, 2.0, 0.02]
    };
    assert(!Validator.validateNam(input), 'Should reject unknown architecture');
});

// ============================================================================
// GAIN VALIDATION TESTS
// ============================================================================

test.test('Rejects extreme gain (factor > max)', () => {
    const factor = MAX_GAIN_LINEAR + 0.1;
    assert(!validateGain(factor, 6.0), `Should reject factor ${factor}`);
});

test.test('Rejects extreme gain (dB > max)', () => {
    const gainDb = MAX_GAIN_DB + 1.0;
    assert(!validateGain(2.0, gainDb), `Should reject gain ${gainDb} dB`);
});

test.test('Rejects zero or negative factor', () => {
    assert(!validateGain(0.0, 0.0), 'Should reject factor 0');
    assert(!validateGain(-1.0, -20.0), 'Should reject negative factor');
});

test.test('Accepts valid gain values', () => {
    assert(validateGain(1.0, 0.0), 'Should accept unity gain');
    assert(validateGain(2.0, 6.0), 'Should accept +6dB');
    assert(validateGain(MAX_GAIN_LINEAR, MAX_GAIN_DB), 'Should accept max gain');
});

test.test('Rejects non-finite gain values', () => {
    assert(!validateGain(Infinity, 0.0), 'Should reject Infinity factor');
    assert(!validateGain(NaN, 0.0), 'Should reject NaN factor');
    assert(!validateGain(2.0, Infinity), 'Should reject Infinity dB');
});

// ============================================================================
// METADATA UPDATE TESTS
// ============================================================================

test.test('Updates loudness metadata', () => {
    const model = {
        version: "0.5.0",
        architecture: "WaveNet",
        config: {},
        weights: [1.0, 2.0, 0.02],
        metadata: { loudness: -12.0 }
    };
    const gainDb = 6.0;
    updateMetadata(model, gainDb);
    assert.strictEqual(model.metadata.loudness, -6.0, 'Loudness should be updated');
});

test.test('Updates gain metadata field', () => {
    const model = {
        version: "0.5.0",
        architecture: "WaveNet",
        config: {},
        weights: [1.0, 2.0, 0.02],
        metadata: { gain: 0.0 }
    };
    const gainDb = 6.0;
    updateMetadata(model, gainDb);
    assert.strictEqual(model.metadata.gain, 6.0, 'Gain should be updated');
});

test.test('Updates output_level config field', () => {
    const model = {
        version: "0.5.0",
        architecture: "WaveNet",
        config: { output_level: 0.0 },
        weights: [1.0, 2.0, 0.02]
    };
    const gainDb = 6.0;
    updateMetadata(model, gainDb);
    assert.strictEqual(model.config.output_level, 6.0, 'Output level should be updated');
});

test.test('Handles missing metadata safely', () => {
    const model = {
        version: "0.5.0",
        architecture: "WaveNet",
        config: {},
        weights: [1.0, 2.0, 0.02]
    };
    assert.doesNotThrow(() => {
        updateMetadata(model, 6.0);
    }, 'Should not throw on missing metadata');
});

test.test('Handles missing config safely', () => {
    const model = {
        version: "0.5.0",
        architecture: "WaveNet",
        metadata: { loudness: -12.0 },
        weights: [1.0, 2.0, 0.02]
    };
    assert.doesNotThrow(() => {
        updateMetadata(model, 6.0);
    }, 'Should not throw on missing config');
});

// ============================================================================
// HELPER FUNCTIONS (Simulating web_bindings.cpp logic)
// ============================================================================

function validateAndParse(jsonStr) {
    try {
        const j = JSON.parse(jsonStr);
        if (!Validator.validateNam(j)) {
            return "Error: Invalid .nam file format (missing required fields or corrupted).";
        }
        return null; // Success
    } catch (e) {
        return "Error: Failed to parse JSON.";
    }
}

function validateGain(factor, gainDb) {
    if (!isFinite(factor) || factor <= 0 || factor > MAX_GAIN_LINEAR) {
        return false;
    }
    if (!isFinite(gainDb) || gainDb > MAX_GAIN_DB) {
        return false;
    }
    return true;
}

function updateMetadata(model, dbGain) {
    if (model.metadata && typeof model.metadata === 'object') {
        if ('loudness' in model.metadata && typeof model.metadata.loudness === 'number') {
            model.metadata.loudness += dbGain;
        }
        if ('gain' in model.metadata && typeof model.metadata.gain === 'number') {
            model.metadata.gain += dbGain;
        }
    }
    if (model.config && typeof model.config === 'object' && 'output_level' in model.config) {
        if (typeof model.config.output_level === 'number') {
            model.config.output_level += dbGain;
        }
    }
}

const Validator = {
    validateNam(j) {
        // Version validation
        if (!j.version || typeof j.version !== 'string') return false;

        const versionRegex = /^0\.\d+\.\d+$/;
        if (!versionRegex.test(j.version)) return false;

        const minorStr = j.version.split('.')[1];
        const minor = parseInt(minorStr, 10);
        if (minor < 5) return false;

        // Architecture validation
        if (!j.architecture || typeof j.architecture !== 'string') return false;

        // Config validation
        if (!j.config || typeof j.config !== 'object') return false;

        // Weights validation
        if (!Array.isArray(j.weights) || j.weights.length === 0) return false;

        for (const w of j.weights) {
            if (typeof w !== 'number' || !isFinite(w)) return false;
        }

        // Architecture-specific config
        if (j.architecture === 'LSTM') {
            if (!('hidden_size' in j.config) || typeof j.config.hidden_size !== 'number') {
                return false;
            }
        } else if (j.architecture === 'ConvNet') {
            if (!('channels' in j.config) || typeof j.config.channels !== 'number') {
                return false;
            }
            if (!('out_channels' in j.config) || typeof j.config.out_channels !== 'number') {
                return false;
            }
        } else if (j.architecture === 'WaveNet' || j.architecture === 'Linear') {
            // No specific requirements
        } else if (j.architecture === 'SlimmableContainer') {
            // A2 model - requires submodels
            if (!j.config.submodels || !Array.isArray(j.config.submodels)) {
                return false;
            }
        } else {
            // Unknown architecture
            return false;
        }

        return true;
    }
};

// ============================================================================
// RUN TESTS
// ============================================================================

const success = test.run();
process.exit(success ? 0 : 1);
