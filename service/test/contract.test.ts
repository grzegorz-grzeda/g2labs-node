import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import { generateContract } from '../scripts/gen-contract.js';

const CONTRACT_DIR = fileURLToPath(new URL('../../contract/', import.meta.url));
const GENERATED = fileURLToPath(new URL('../src/contract.generated.ts', import.meta.url));

describe('Generated contract module', () => {
  it('is up to date with contract/ (run `npm run contract` to regenerate)', () => {
    expect(readFileSync(GENERATED, 'utf8')).toBe(generateContract(CONTRACT_DIR));
  });
});

describe('Contract generator', () => {
  it('refuses two text topics that would get the same type name, naming both', () => {
    const dir = mkdtempSync(join(tmpdir(), 'g2labs-contract-'));
    try {
      mkdirSync(join(dir, 'schemas'));
      const topic = (pattern: string) => ({ pattern, publisher: 'node', retained: true, encoding: 'text', values: ['on', 'off'] });
      writeFileSync(
        join(dir, 'topics.json'),
        JSON.stringify({ topics: [topic('g2labs/<node-id>/power'), topic('g2labs/<node-id>/relay/power')] }),
      );
      expect(() => generateContract(dir)).toThrow(/NodePower.*g2labs\/<node-id>\/power.*g2labs\/<node-id>\/relay\/power/);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });
});

