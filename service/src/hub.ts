// The Hub: wires the database, the Broker connection and the admin page
// together. `main.ts` runs it from env vars; tests start it directly.

import { fileURLToPath } from 'node:url';
import { loadConfig, type Env } from './config.js';
import { openDb } from './db.js';
import { connectBroker } from './broker.js';
import { createHttp } from './http.js';
import { NODE_TOPICS, NodeRegistry } from './nodes.js';

export interface Hub {
  /** Base URL of the admin page, e.g. http://127.0.0.1:8080. */
  url: string;
  /** Says `offline` on the Broker, then stops everything. */
  close(): Promise<void>;
}

export interface HubOptions {
  log?: (msg: string) => void;
  /** The clock, in ms since the epoch. Tests pass their own. */
  now?: () => number;
}

/** The built admin page, next to the built server (dist/public). */
const PUBLIC_DIR = fileURLToPath(new URL('../public/', import.meta.url));

export async function startHub(env: Env, options: HubOptions = {}): Promise<Hub> {
  const log = options.log ?? ((msg: string) => console.log(msg));
  const config = loadConfig(env);
  const now = options.now ?? Date.now;
  const db = openDb(config.dataDir);
  const nodes = new NodeRegistry({
    db,
    now,
    log,
    retentionMs: config.statusRetentionMs,
    // `broker` is created below, because it feeds the registry; this runs later.
    clearRetained: (topic) =>
      broker.publish(topic, '', { retain: true }).catch((err) => log(`clearing ${topic} failed: ${err.message}`)),
  });
  const broker = connectBroker(config, log, {
    topics: NODE_TOPICS,
    onMessage: (topic, payload, retained) => nodes.handleMessage(topic, payload, retained),
  });
  const http = createHttp({ config, broker, nodes, publicDir: PUBLIC_DIR });

  let address: string;
  try {
    address = await http.listen({ host: config.httpHost, port: config.httpPort });
  } catch (err) {
    await broker.close();
    nodes.close();
    db.close();
    throw err;
  }
  log(`admin page on ${address}`);

  return {
    url: address,
    async close() {
      await http.close();
      await broker.close();
      nodes.close();
      db.close();
    },
  };
}
