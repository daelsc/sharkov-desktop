'use strict';
const fs = require('fs');
const path = require('path');

const counterPath = path.join(__dirname, '..', '.buildcount');
const outPath = path.join(__dirname, '..', 'static', 'buildId.json');

let count = 0;
try {
  count = parseInt(fs.readFileSync(counterPath, 'utf8').trim(), 10) || 0;
} catch (_) {}

count++;

fs.writeFileSync(counterPath, String(count));

const buildId = String(count);
fs.writeFileSync(outPath, JSON.stringify({ buildId }));
console.log('generate-build-id:', buildId);
