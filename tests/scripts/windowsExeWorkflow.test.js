'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const workflow = fs.readFileSync(
  path.join(__dirname, '..', '..', '.github', 'workflows', 'windows-exe.yml'),
  'utf8'
);

test('Windows exe packaging is unsigned, fork-only, and not a pull-request check', () => {
  assert.match(workflow, /workflow_dispatch:/);
  assert.match(workflow, /if: github\.repository != 'Javis603\/token-monitor'/);
  assert.match(workflow, /npm run dist:win/);
  assert.match(workflow, /CSC_IDENTITY_AUTO_DISCOVERY: 'false'/);
  assert.match(workflow, /dist\/\*\.exe/);
  assert.doesNotMatch(workflow, /\bpull_request:/);
  assert.doesNotMatch(workflow, /SignPath|SIGNPATH_/);
  assert.doesNotMatch(workflow, /npm run dist:win:prepackaged/);
});
