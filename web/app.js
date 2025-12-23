const dropZone = document.getElementById('drop-zone');
const gainDbInput = document.getElementById('gain-db');
const gainLinearInput = document.getElementById('gain-linear');
const processBtn = document.getElementById('process');
const results = document.getElementById('results');
const gainTypeRadios = document.querySelectorAll('input[name="gainType"]');

let files = [];

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
        const gainDb = parseFloat(gainDbInput.value);
        const gainLinear = parseFloat(gainLinearInput.value);
        let outputList = '';
        for (const file of files) {
            let base = file.name.replace('.nam', '');
            let gainValue = selectedType === 'db' ? gainDb : gainLinear;
            let gainStr = formatGain(gainValue, selectedType === 'db');
            let suffix = selectedType === 'db' ? 'db' : 'lin';
            let outputName = base + '_' + gainStr + suffix + '.nam';
            outputList += outputName + '<br>';
        }
        dropZone.innerHTML = outputList;
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
    files = Array.from(e.dataTransfer.files).filter(f => f.name.endsWith('.nam'));
    updateDropZoneDisplay();
});

processBtn.addEventListener('click', async () => {
    const selectedType = document.querySelector('input[name="gainType"]:checked').value;
    const gainDb = parseFloat(gainDbInput.value);
    const gainLinear = parseFloat(gainLinearInput.value);
    let factor, isDb;
    if (selectedType === 'db') {
        factor = Math.pow(10, gainDb / 20);
        isDb = true;
    } else {
        factor = gainLinear;
        isDb = false;
    }
    results.innerHTML = '';
    results.classList.remove('error');
    if (!files.length) {
        showStatus('Drop one or more .nam files first.');
        return;
    }
    showStatus(`Processing ${files.length} file(s)…`);

    const gainValueForName = isDb ? gainDb : gainLinear;
    const gainStrForName = formatGain(gainValueForName, isDb);
    const suffixForName = isDb ? 'db' : 'lin';

    // If multiple files, prefer a single zip to avoid browser multi-download blocking.
    const shouldZip = files.length > 1 && typeof window.fflate !== 'undefined';
    const zipEntries = shouldZip ? {} : null;
    let successCount = 0;

    for (const file of files) {
        let outputName = '';
        try {
            const text = await file.text();
            const json = JSON.parse(text);
            const modified = Module.processNam(
                JSON.stringify(json),
                factor,
                isDb ? gainDb : 20 * Math.log10(gainLinear)
            );

            if (typeof modified === 'string' && modified.startsWith('Error:')) {
                throw new Error(modified.replace(/^Error:\s*/, ''));
            }

            const base = file.name.replace('.nam', '');
            outputName = base + '_' + gainStrForName + suffixForName + '.nam';

            if (shouldZip) {
                // fflate helpers: strToU8 + zipSync
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
        } catch (e) {
            const nameForError = outputName || file.name;
            showError(`Error processing ${file.name} (${nameForError}): ${e.message}`);
        }
    }

    if (shouldZip && successCount > 0) {
        const zipBytes = window.fflate.zipSync(zipEntries, { level: 0 });
        const zipBlob = new Blob([zipBytes], { type: 'application/zip' });
        const zipUrl = URL.createObjectURL(zipBlob);
        const a = document.createElement('a');
        a.href = zipUrl;
        a.download = `nam_volumifier_${gainStrForName}${suffixForName}.zip`;
        document.body.appendChild(a);
        a.click();
        a.remove();
        setTimeout(() => URL.revokeObjectURL(zipUrl), 2000);
    }

    // Clear status text if there were no errors appended.
    if (results.textContent === `Processing ${files.length} file(s)…`) {
        results.textContent = '';
    }
});