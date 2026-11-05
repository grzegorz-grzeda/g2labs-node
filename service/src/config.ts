// The Hub's configuration, read from environment variables.

export interface Config {
  /** Broker URL, e.g. mqtt://localhost:1883. */
  mqttUrl: string;
  mqttUsername?: string;
  mqttPassword?: string;
  /** Delay before reconnecting after a failed connect or a lost connection. */
  mqttReconnectMs: number;
  /** The one password that opens the admin page. */
  adminPassword: string;
  /** Directory holding the SQLite database. */
  dataDir: string;
  httpHost: string;
  /** 0 picks a free port. */
  httpPort: number;
  /** How long Status history is kept. */
  statusRetentionMs: number;
}

export type Env = Record<string, string | undefined>;

export class ConfigError extends Error {}

function nonEmpty(value: string | undefined): string | undefined {
  return value === undefined || value === '' ? undefined : value;
}

function number(env: Env, name: string, fallback: number): number {
  const raw = nonEmpty(env[name]);
  if (raw === undefined) return fallback;
  const value = Number(raw);
  if (!Number.isFinite(value) || value < 0) {
    throw new ConfigError(`${name} must be a non-negative number, got "${raw}"`);
  }
  return value;
}

export function loadConfig(env: Env): Config {
  const adminPassword = nonEmpty(env.HUB_ADMIN_PASSWORD);
  if (adminPassword === undefined) {
    throw new ConfigError('HUB_ADMIN_PASSWORD must be set');
  }
  const mqttUrl = nonEmpty(env.HUB_MQTT_URL) ?? 'mqtt://localhost:1883';
  if (!/^(mqtts?|wss?|tcp):\/\//.test(mqttUrl)) {
    throw new ConfigError(`HUB_MQTT_URL must be an mqtt:// URL, got "${mqttUrl}"`);
  }
  const httpPort = number(env, 'HUB_HTTP_PORT', 8080);
  if (!Number.isInteger(httpPort) || httpPort > 65535) {
    throw new ConfigError(`HUB_HTTP_PORT must be a TCP port, got "${env.HUB_HTTP_PORT}"`);
  }
  const retentionDays = number(env, 'HUB_STATUS_RETENTION_DAYS', 30);
  if (retentionDays <= 0) {
    throw new ConfigError(`HUB_STATUS_RETENTION_DAYS must be more than 0, got "${env.HUB_STATUS_RETENTION_DAYS}"`);
  }
  return {
    mqttUrl,
    mqttUsername: nonEmpty(env.HUB_MQTT_USERNAME),
    mqttPassword: nonEmpty(env.HUB_MQTT_PASSWORD),
    mqttReconnectMs: Math.round(number(env, 'HUB_MQTT_RECONNECT_S', 5) * 1000),
    adminPassword,
    dataDir: nonEmpty(env.HUB_DATA_DIR) ?? '/data',
    httpHost: nonEmpty(env.HUB_HTTP_HOST) ?? '0.0.0.0',
    httpPort,
    statusRetentionMs: Math.round(retentionDays * 24 * 3600 * 1000),
  };
}
