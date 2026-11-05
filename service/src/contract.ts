// Runtime checks of incoming MQTT payloads against the contract (contract/,
// generated into contract.generated.ts). Unknown fields are dropped, not
// refused: the contract only grows (ADR-0005), so a newer Node may send
// fields this Hub doesn't know yet.

import { Ajv2020 } from 'ajv/dist/2020.js';
import { statusSchema, type Status } from './contract.generated.js';

export type { Status } from './contract.generated.js';

// removeAdditional: true drops properties where a schema says
// `additionalProperties: false`, instead of failing on them.
const ajv = new Ajv2020({ removeAdditional: true, allErrors: true });
const validateStatus = ajv.compile<Status>(statusSchema);

export type Parsed<T> = { ok: true; value: T } | { ok: false; error: string };

export function parseStatus(payload: string): Parsed<Status> {
  let value: unknown;
  try {
    value = JSON.parse(payload);
  } catch {
    return { ok: false, error: 'not JSON' };
  }
  if (!validateStatus(value)) {
    return { ok: false, error: ajv.errorsText(validateStatus.errors) };
  }
  return { ok: true, value };
}
