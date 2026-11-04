// The Hub: wires the database, the Broker connection and the admin page
// together. `main.ts` runs it from env vars; tests start it directly.

import { fileURLToPath } from 'node:url';
import { loadConfig, type Env } from './config.js';
import { openDb } from './db.js';
import { connectBroker } from './broker.js';
import { createHttp } from './http.js';

export interface Hub {
  /** Base URL of the admin page, e.g. http://127.0.0.1:8080. */
  url: string;
  /** Says `offline` on the Broker, then stops everything. */
  close(): Promise<void>;
}

export interface HubOptions {
  log?: (msg: string) => void;
}

/** The built admin page, next to the built server (dist/public). */
const PUBLIC_DIR = fileURLToPath(new URL('../public/', import.meta.url));

export async function startHub(env: Env, options: HubOptions = {}): Promise<Hub> {
  const log = options.log ?? ((msg: string) => console.log(msg));
  const config = loadConfig(env);
  const db = openDb(config.dataDir);
  const broker = connectBroker(config, log);
  const http = createHttp({ config, broker, publicDir: PUBLIC_DIR });

  let address: string;
  try {
    address = await http.listen({ host: config.httpHost, port: config.httpPort });
  } catch (err) {
    await broker.close();
    db.close();
    throw err;
  }
  log(`admin page on ${address}`);

  return {
    url: address,
    async close() {
      await http.close();
      await broker.close();
      db.close();
    },
  };
}
