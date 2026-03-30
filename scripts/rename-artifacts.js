'use strict';
const fs = require('fs');
const path = require('path');

const outDir = path.join(__dirname, '..', 'out');
const buildIdPath = path.join(__dirname, '..', 'static', 'buildId.json');

let buildId = '';
try {
  buildId = JSON.parse(fs.readFileSync(buildIdPath, 'utf8')).buildId;
} catch (_) {}

if (!buildId) {
  console.log('rename-artifacts: no buildId found, skipping');
  process.exit(0);
}

const pkg = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'package.json'), 'utf8'));
const ver = pkg.version;

const renames = [
  [`Sharkov ${ver}.exe`, `Sharkov ${ver}-${buildId}.exe`],
  [`Sharkov Setup ${ver}.exe`, `Sharkov Setup ${ver}-${buildId}.exe`],
];

for (const [from, to] of renames) {
  const src = path.join(outDir, from);
  const dst = path.join(outDir, to);
  if (fs.existsSync(src)) {
    try {
      if (fs.existsSync(dst)) fs.unlinkSync(dst);
      fs.renameSync(src, dst);
      console.log(`rename-artifacts: ${from} -> ${to}`);
    } catch (err) {
      console.error(`rename-artifacts: failed to rename ${from}:`, err.message);
    }
  }
}
