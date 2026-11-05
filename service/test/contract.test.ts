import { readFileSync } from 'node:fs';
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
