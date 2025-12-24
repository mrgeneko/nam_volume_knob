const dropZone = document.getElementById('drop-zone');
const fileInput = document.getElementById('file-input');
const chooseFilesBtn = document.getElementById('choose-files');
const dropZoneInstructions = document.getElementById('drop-zone-instructions');
const dropZoneOutput = document.getElementById('drop-zone-output');
const gainDbInput = document.getElementById('gain-db');
const gainLinearInput = document.getElementById('gain-linear');
const processBtn = document.getElementById('process');
const results = document.getElementById('results');
const gainTypeRadios = document.querySelectorAll('input[name="gainType"]');

const MAX_GAIN_DB = 9;
const MAX_GAIN_LINEAR = Math.pow(10, MAX_GAIN_DB / 20);

let files = [];

function setFilesFromList(fileList) {
    files = Array.from(fileList || []).filter(f => f.name.toLowerCase().endsWith('.nam'));
    updateDropZoneDisplay();
}

function showStatus(message) {
    results.classList.remove('error');
    results.textContent = message;
}

function showError(message) {
    results.classList.add('error');
    const line = document.createElement('div');
    line.textContent = message;
    results.appendChild(line);
}

function formatGain(gain, isDb) {
    let gainStr = gain.toFixed(7);
    let dotPos = gainStr.indexOf('.');
    if (dotPos !== -1) {
        while (gainStr.length > dotPos + 1 && gainStr.endsWith('0')) {
            gainStr = gainStr.slice(0, -1);
        }
        if (gainStr.endsWith('.')) {
            gainStr += '0';
        }
    }
    gainStr = gainStr.replace('.', '_');
    if (isDb && gain >= 0) gainStr = '+' + gainStr;
    return gainStr;
}

function parseGainList(raw) {
    const parts = String(raw || '')
        .split(',')
        .map(s => s.trim())
        .filter(Boolean);
    if (!parts.length) return [];
    const values = parts.map(p => Number(p));
    if (values.some(v => Number.isNaN(v))) {
        throw new Error('Gain list contains non-numeric value(s).');
    }
    return values;
}

function validateGains(gains, isDb) {
    if (!Array.isArray(gains) || !gains.length) return 'Enter one or more gains (comma-separated).';
    if (gains.some(v => !Number.isFinite(v))) return 'Gain list contains non-finite value(s).';
    if (isDb) {
        const tooHigh = gains.find(g => g > MAX_GAIN_DB);
        if (tooHigh !== undefined) return `Max gain is +${MAX_GAIN_DB} dB.`;
        return null;
    }

    const bad = gains.find(g => g <= 0);
    if (bad !== undefined) return 'Linear gains must be > 0.';

    const tooHigh = gains.find(g => g > MAX_GAIN_LINEAR);
    if (tooHigh !== undefined) return `Max linear gain is ${MAX_GAIN_LINEAR.toFixed(5)} (equivalent to +${MAX_GAIN_DB} dB).`;

    return null;
}

// Handle gain type change
gainTypeRadios.forEach(radio => {
    radio.addEventListener('change', () => {
        if (radio.value === 'db') {
            gainDbInput.disabled = false;
            gainLinearInput.disabled = true;
        } else {
            gainDbInput.disabled = true;
            gainLinearInput.disabled = false;
        }
        updateDropZoneDisplay();
    });
});

// Update drop zone display when gain changes
gainDbInput.addEventListener('input', updateDropZoneDisplay);
gainLinearInput.addEventListener('input', updateDropZoneDisplay);

function updateDropZoneDisplay() {
    if (files.length > 0) {
        const selectedType = document.querySelector('input[name="gainType"]:checked').value;
        let gains;
        let isDb;
        try {
            if (selectedType === 'db') {
                gains = parseGainList(gainDbInput.value);
                isDb = true;
            } else {
                gains = parseGainList(gainLinearInput.value);
                isDb = false;
            }
        } catch {
            dropZoneInstructions.textContent = 'Drop .nam files here';
            dropZoneOutput.textContent = 'Invalid gain list.';
            return;
        }

        const validationError = validateGains(gains, isDb);
        if (validationError) {
            dropZoneInstructions.textContent = 'Drop .nam files here';
            dropZoneOutput.textContent = validationError;
            return;
        }
        let outputList = '';
        for (const file of files) {
            let base = file.name.replace('.nam', '');
            let suffix = selectedType === 'db' ? 'db' : 'lin';
            for (const gainValue of gains) {
                let gainStr = formatGain(gainValue, selectedType === 'db');
                let outputName = base + '_' + gainStr + suffix + '.nam';
                outputList += outputName + '<br>';
            }
        }
        dropZoneInstructions.textContent = 'Drop .nam files here';
        dropZoneOutput.innerHTML = outputList;
    } else {
        dropZoneInstructions.textContent = 'Drop .nam files here';
        dropZoneOutput.innerHTML = '';
    }
}

dropZone.addEventListener('dragover', (e) => {
    e.preventDefault();
    dropZone.classList.add('dragover');
});

dropZone.addEventListener('dragleave', () => {
    dropZone.classList.remove('dragover');
});

dropZone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropZone.classList.remove('dragover');
    setFilesFromList(e.dataTransfer.files);
});

chooseFilesBtn.addEventListener('click', () => {
    // Reset so selecting the same file again still triggers a change event.
    fileInput.value = '';
    fileInput.click();
});

fileInput.addEventListener('change', (e) => {
    setFilesFromList(e.target.files);
});

processBtn.addEventListener('click', async () => {
    const selectedType = document.querySelector('input[name="gainType"]:checked').value;
    let gains;
    let isDb;
    try {
        if (selectedType === 'db') {
            gains = parseGainList(gainDbInput.value);
            isDb = true;
        } else {
            gains = parseGainList(gainLinearInput.value);
            isDb = false;
        }
    } catch (e) {
        results.innerHTML = '';
        results.classList.add('error');
        showError(e.message);
        // Track parse errors
        if (typeof gtag === 'function') {
            gtag('event', 'nam_error', {
                'error_type': 'gain_parse_error',
                'error_message': e.message
            });
        }
        return;
    }

    {
        const validationError = validateGains(gains, isDb);
        if (validationError) {
            results.innerHTML = '';
            results.classList.add('error');
            showError(validationError);
            // Track validation errors
            if (typeof gtag === 'function') {
                gtag('event', 'nam_error', {
                    'error_type': 'validation_error',
                    'error_message': validationError
                });
            }
            return;
        }
    }
    results.innerHTML = '';
    results.classList.remove('error');
    if (!files.length) {
        showStatus('Drop one or more .nam files first.');
        return;
    }
    showStatus(`Processing ${files.length} file(s) × ${gains.length} gain(s)…`);

    const suffixForName = isDb ? 'db' : 'lin';

    // If multiple files, prefer a single zip to avoid browser multi-download blocking.
    const totalOutputs = files.length * gains.length;
    const shouldZip = totalOutputs > 1 && typeof window.fflate !== 'undefined';
    const zipEntries = shouldZip ? {} : null;
    let successCount = 0;

    for (const file of files) {
        try {
            const text = await file.text();
            const json = JSON.parse(text);
            const base = file.name.replace('.nam', '');
            for (const gainValue of gains) {
                const factor = isDb ? Math.pow(10, gainValue / 20) : gainValue;
                const gainDbForMetadata = isDb ? gainValue : 20 * Math.log10(gainValue);

                const modified = Module.processNam(
                    JSON.stringify(json),
                    factor,
                    gainDbForMetadata
                );

                if (typeof modified === 'string' && modified.startsWith('Error:')) {
                    throw new Error(modified.replace(/^Error:\s*/, ''));
                }

                const gainStrForName = formatGain(gainValue, isDb);
                const outputName = base + '_' + gainStrForName + suffixForName + '.nam';

                if (shouldZip) {
                    zipEntries[outputName] = window.fflate.strToU8(modified);
                } else {
                    const blob = new Blob([modified], { type: 'application/json' });
                    const url = URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    a.download = outputName;
                    document.body.appendChild(a);
                    a.click();
                    a.remove();
                    setTimeout(() => URL.revokeObjectURL(url), 1000);
                }

                successCount++;

                // Track each export with gain level
                if (typeof gtag === 'function') {
                    gtag('event', 'nam_export', {
                        'gain_value': gainValue,
                        'gain_type': isDb ? 'db' : 'linear',
                        'file_name': file.name
                    });
                }
            }
        } catch (e) {
            showError(`Error processing ${file.name}: ${e.message}`);
            // Track processing errors
            if (typeof gtag === 'function') {
                gtag('event', 'nam_error', {
                    'error_type': 'processing_error',
                    'error_message': e.message,
                    'file_name': file.name
                });
            }
        }
    }

    if (shouldZip && successCount > 0) {
        const zipBytes = window.fflate.zipSync(zipEntries, { level: 0 });
        const zipBlob = new Blob([zipBytes], { type: 'application/zip' });
        const zipUrl = URL.createObjectURL(zipBlob);
        const a = document.createElement('a');
        a.href = zipUrl;
        a.download = `nam_volume_knob_${suffixForName}.zip`;
        document.body.appendChild(a);
        a.click();
        a.remove();
        setTimeout(() => URL.revokeObjectURL(zipUrl), 2000);
    }

    // Track batch summary
    if (successCount > 0 && typeof gtag === 'function') {
        gtag('event', 'nam_export_batch', {
            'file_count': files.length,
            'gain_count': gains.length,
            'total_exports': successCount,
            'gain_type': isDb ? 'db' : 'linear',
            'gain_levels': gains.join(',')
        });
    }

    // Clear status text if there were no errors appended.
    if (results.textContent === `Processing ${files.length} file(s) × ${gains.length} gain(s)…`) {
        results.textContent = '';
    }
});