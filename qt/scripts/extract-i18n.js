'use strict';

const fs = require('node:fs');
const path = require('node:path');

const repoRoot = path.resolve(__dirname, '..', '..');
const i18n = require(path.join(repoRoot, 'src', 'electron', 'renderer', 'i18n.js'));
const outDir = path.join(__dirname, '..', 'resources');
fs.mkdirSync(outDir, { recursive: true });
const payload = {
    languages: Object.keys(i18n.MESSAGES || {}),
    messages: i18n.MESSAGES || {}
};
fs.writeFileSync(path.join(outDir, 'i18n.json'), JSON.stringify(payload), 'utf8');
console.log(`wrote ${path.join(outDir, 'i18n.json')} (${payload.languages.length} locales)`);
