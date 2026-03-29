'use strict';
const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

let gitHash = 'unknown';
try {
  gitHash = execSync('git rev-parse --short HEAD', { encoding: 'utf8' }).trim();
} catch (_) {}

const now = new Date();
const date = (now.getMonth() + 1).toString().padStart(2, '0') + now.getDate().toString().padStart(2, '0');
const time = now.getHours().toString().padStart(2, '0') + now.getMinutes().toString().padStart(2, '0');
const buildId = `${gitHash}.${date}.${time}`;
const outPath = path.join(__dirname, '..', 'static', 'buildId.json');
fs.writeFileSync(outPath, JSON.stringify({ buildId }));
console.log('generate-build-id:', buildId);
