// The admin API's response types, shared with the admin page (web/), so
// types only: nothing here may need Node.js.

import type { NodeAvailability, Status } from './contract.generated.js';

export type { Status };

/** GET /api/status */
export interface HubStatus {
  broker: { url: string; connected: boolean };
  mqtt: {
    /** Messages from Nodes refused as invalid since the Hub started. */
    invalid_messages: number;
  };
}

/** A Node's last word on its availability topic; null until it says one. */
export type Availability = NodeAvailability | null;

/** One entry of GET /api/nodes */
export interface NodeSummary {
  node_id: string;
  availability: Availability;
  first_seen: string;
  last_seen: string;
  /** The latest Status, null until the first one arrives. */
  status: Status | null;
}

export interface HistoryPoint {
  at: string;
  uptime_s: number;
  rssi: number | null;
  free_heap: number;
}

/** GET /api/nodes/:id */
export interface NodeDetail extends NodeSummary {
  /** Status history within the retention, oldest first. */
  history: HistoryPoint[];
  /** When the Node's uptime went down: it restarted since the Status before. */
  uptime_resets: string[];
}

/** GET /api/nodes */
export interface NodeList {
  nodes: NodeSummary[];
}
