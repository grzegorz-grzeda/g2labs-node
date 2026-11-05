// The Node registry: every Node seen on the Broker, its latest Status and its
// Status history. Fed with the Nodes' MQTT messages; read by the admin API.

import type { Db } from './db.js';
import { parseStatus } from './contract.js';
import type { Status } from './contract.generated.js';
import { statusSchema, topics } from './contract.generated.js';
import type { Availability, NodeDetail, NodeSummary } from './api.js';

const NODE_ID = new RegExp(statusSchema.properties.node_id.pattern);

export const NODE_TOPICS = ['g2labs/+/availability', 'g2labs/+/status'];

interface NodeRow {
  node_id: string;
  first_seen: number;
  last_seen: number;
  availability: Availability;
  status: string | null;
}

export interface NodeRegistryDeps {
  db: Db;
  now: () => number;
  log: (msg: string) => void;
  /** How long Status history is kept. */
  retentionMs: number;
  /** Clears a retained message on the Broker. Must not reject. */
  clearRetained: (topic: string) => Promise<void>;
}

/** The retained topics a Node publishes, per the contract. */
function retainedTopicsOf(nodeId: string): string[] {
  return topics
    .filter((t) => t.publisher === 'node' && t.retained)
    .map((t) => t.pattern.replace('<node-id>', nodeId));
}

const PRUNE_INTERVAL_MS = 3600 * 1000;

export class NodeRegistry {
  private readonly db: Db;
  private readonly now: () => number;
  private readonly log: (msg: string) => void;
  private readonly retentionMs: number;
  private readonly clearRetained: (topic: string) => Promise<void>;
  private readonly pruneTimer: NodeJS.Timeout;
  private readonly listeners = new Set<(nodeId: string) => void>();
  /** Messages refused since the Hub started. */
  invalidMessages = 0;

  constructor({ db, now, log, retentionMs, clearRetained }: NodeRegistryDeps) {
    this.db = db;
    this.clearRetained = clearRetained;
    this.now = now;
    this.log = log;
    this.retentionMs = retentionMs;
    this.prune();
    // Reads leave out expired history anyway; this only frees the space.
    this.pruneTimer = setInterval(() => this.prune(), PRUNE_INTERVAL_MS).unref();
  }

  close(): void {
    clearInterval(this.pruneTimer);
  }

  /** Calls `listener` with the Node ID whenever a Node changes or is forgotten. Returns an unsubscribe. */
  onChange(listener: (nodeId: string) => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  private changed(nodeId: string): void {
    for (const listener of this.listeners) listener(nodeId);
  }

  private cutoff(): number {
    return this.now() - this.retentionMs;
  }

  /** Deletes Status history older than the retention. */
  private prune(): void {
    this.db.prepare('DELETE FROM status_history WHERE received_at < ?').run(this.cutoff());
  }

  /** Takes in one message from a topic in NODE_TOPICS. */
  handleMessage(topic: string, payload: Buffer, retained: boolean): void {
    const match = /^g2labs\/([^/]+)\/(availability|status)$/.exec(topic);
    if (!match) return;
    const [, nodeId, kind] = match;
    if (nodeId === 'hub') return; // the Hub's own availability
    const text = payload.toString();
    const error = !NODE_ID.test(nodeId)
      ? 'not a Node ID'
      : kind === 'status'
        ? this.handleStatus(nodeId, text)
        : this.handleAvailability(nodeId, text, retained);
    if (error !== undefined) {
      this.invalidMessages++;
      this.log(`ignored an invalid message on ${topic}: ${error}`);
    }
  }

  /** Returns what is wrong with the message, if anything. */
  private handleAvailability(nodeId: string, payload: string, retained: boolean): string | undefined {
    // Empty is the Broker clearing a retained message, as Forget does.
    if (payload === '') return undefined;
    if (payload !== 'online' && payload !== 'offline') return 'neither online nor offline';
    // Only a live `online` is the Node itself: `offline` is usually its last
    // will, published by the Broker after the Node went quiet, and a retained
    // message may be old. A Node first met that way can only be dated now.
    const seen = payload === 'online' && !retained;
    const at = this.now();
    this.db
      .prepare(
        `INSERT INTO nodes (node_id, first_seen, last_seen, availability) VALUES (?, ?, ?, ?)
         ON CONFLICT (node_id) DO UPDATE SET availability = excluded.availability,
           last_seen = CASE WHEN ? THEN last_seen ELSE excluded.last_seen END`,
      )
      .run(nodeId, at, at, payload, seen ? 0 : 1);
    this.changed(nodeId);
    return undefined;
  }

  private handleStatus(nodeId: string, payload: string): string | undefined {
    const parsed = parseStatus(payload);
    if (!parsed.ok) return parsed.error;
    if (parsed.value.node_id !== nodeId) return `node_id ${parsed.value.node_id} doesn't match the topic`;
    const at = this.now();
    const json = JSON.stringify(parsed.value);
    this.db.transaction(() => {
      this.db
        .prepare(
          // A Node publishes Status only while connected, so it is online
          // (a forgotten Node lost its retained `online`).
          `INSERT INTO nodes (node_id, first_seen, last_seen, availability, status) VALUES (?, ?, ?, 'online', ?)
           ON CONFLICT (node_id) DO UPDATE SET last_seen = excluded.last_seen, availability = 'online',
             status = excluded.status`,
        )
        .run(nodeId, at, at, json);
      this.db.prepare('INSERT INTO status_history (node_id, received_at, status) VALUES (?, ?, ?)').run(nodeId, at, json);
      this.prune();
    })();
    this.changed(nodeId);
    return undefined;
  }

  list(): NodeSummary[] {
    const rows = this.db.prepare('SELECT * FROM nodes ORDER BY node_id').all() as NodeRow[];
    return rows.map(summary);
  }

  /**
   * Forgets a Node: deletes it and its history and clears its retained
   * topics, so it isn't registered again from them. A Node still running
   * comes back with its next Status. False for a Node never seen.
   */
  async forget(nodeId: string): Promise<boolean> {
    const { changes } = this.db.prepare('DELETE FROM nodes WHERE node_id = ?').run(nodeId);
    if (changes === 0) return false;
    this.log(`forgot Node ${nodeId}`);
    this.changed(nodeId);
    await Promise.all(retainedTopicsOf(nodeId).map((topic) => this.clearRetained(topic)));
    return true;
  }

  /** One Node with its Status history, or undefined for a Node never seen. */
  get(nodeId: string): NodeDetail | undefined {
    const row = this.db.prepare('SELECT * FROM nodes WHERE node_id = ?').get(nodeId) as NodeRow | undefined;
    if (!row) return undefined;
    const history = (
      this.db
        .prepare(
          `SELECT received_at,
                  status ->> '$.uptime_s' AS uptime_s,
                  status ->> '$.rssi' AS rssi,
                  status ->> '$.free_heap' AS free_heap
           FROM status_history WHERE node_id = ? AND received_at >= ? ORDER BY received_at, rowid`,
        )
        .all(nodeId, this.cutoff()) as { received_at: number; uptime_s: number; rssi: number | null; free_heap: number }[]
    ).map(({ received_at, ...point }) => ({ at: new Date(received_at).toISOString(), ...point }));
    // Uptime going down means the Node restarted in between.
    const uptime_resets = history.filter((p, i) => i > 0 && p.uptime_s < history[i - 1].uptime_s).map((p) => p.at);
    return { ...summary(row), history, uptime_resets };
  }
}

function summary(row: NodeRow): NodeSummary {
  return {
    node_id: row.node_id,
    availability: row.availability,
    first_seen: new Date(row.first_seen).toISOString(),
    last_seen: new Date(row.last_seen).toISOString(),
    status: row.status === null ? null : (JSON.parse(row.status) as Status),
  };
}
