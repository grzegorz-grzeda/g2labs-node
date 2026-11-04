// Test harness: an in-process MQTT Broker, an observer client on it, and a
// Hub started from env vars. Tests talk to the Hub only over MQTT and HTTP.

import { Aedes } from 'aedes';
import mqtt, { type MqttClient } from 'mqtt';
import { mkdtempSync, rmSync } from 'node:fs';
import { createServer, type Server, type Socket } from 'node:net';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { startHub, type Hub } from '../src/hub.js';

export interface BrokerOptions {
  port?: number;
  credentials?: { username: string; password: string };
}

export class TestBroker {
  private sockets = new Set<Socket>();
  private clients = new Map<string, Socket>();

  private constructor(
    private aedes: Aedes,
    private server: Server,
    readonly port: number,
  ) {
    aedes.on('clientReady', (client) => this.clients.set(client.id, client.conn as Socket));
    server.on('connection', (socket) => {
      this.sockets.add(socket);
      socket.on('close', () => this.sockets.delete(socket));
    });
  }

  static async start(options: BrokerOptions = {}): Promise<TestBroker> {
    const aedes = await Aedes.createBroker();
    if (options.credentials) {
      const { username, password } = options.credentials;
      aedes.authenticate = (_client, user, pass, done) => {
        const ok = user === username && pass?.toString() === password;
        done(ok ? null : Object.assign(new Error('bad credentials'), { returnCode: 4 }), ok);
      };
    }
    const server = createServer(aedes.handle);
    await new Promise<void>((resolve, reject) => {
      server.once('error', reject);
      server.listen(options.port ?? 0, '127.0.0.1', () => resolve());
    });
    const port = (server.address() as { port: number }).port;
    return new TestBroker(aedes, server, port);
  }

  get url(): string {
    return `mqtt://127.0.0.1:${this.port}`;
  }

  /** Cuts one client's TCP connection without an MQTT DISCONNECT. */
  dropClient(clientId: string): void {
    const conn = this.clients.get(clientId);
    if (!conn) throw new Error(`no client ${clientId} on the Broker`);
    conn.destroy();
  }

  /** Cuts every client's TCP connection without an MQTT DISCONNECT. */
  dropConnections(): void {
    for (const socket of this.sockets) socket.destroy();
  }

  async stop(): Promise<void> {
    this.dropConnections();
    await new Promise<void>((resolve) => this.server.close(() => resolve()));
    await new Promise<void>((resolve) => this.aedes.close(() => resolve()));
  }
}

export interface Received {
  topic: string;
  payload: string;
  retain: boolean;
}

/** An MQTT client that records what it receives on the topics it watches. */
export class Observer {
  readonly messages: Received[] = [];

  private constructor(private client: MqttClient) {
    client.on('message', (topic, payload, packet) => {
      this.messages.push({ topic, payload: payload.toString(), retain: packet.retain });
    });
  }

  static async connect(broker: TestBroker, topic: string, options: mqtt.IClientOptions = {}): Promise<Observer> {
    const client = await mqtt.connectAsync(broker.url, { reconnectPeriod: 0, ...options });
    const observer = new Observer(client);
    await client.subscribeAsync(topic, { qos: 1 });
    return observer;
  }

  /** Waits for a message matching `pred` among those received after `from`. */
  async waitFor(pred: (m: Received) => boolean, from = 0, timeoutMs = 5000): Promise<Received> {
    const deadline = Date.now() + timeoutMs;
    for (;;) {
      const found = this.messages.slice(from).find(pred);
      if (found) return found;
      if (Date.now() > deadline) {
        throw new Error(`no matching message; got ${JSON.stringify(this.messages)}`);
      }
      await sleep(20);
    }
  }

  async close(): Promise<void> {
    await this.client.endAsync(true);
  }
}

export function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

export async function eventually<T>(fn: () => Promise<T>, check: (v: T) => boolean, timeoutMs = 5000): Promise<T> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const value = await fn();
    if (check(value)) return value;
    if (Date.now() > deadline) throw new Error(`condition not met; last value ${JSON.stringify(value)}`);
    await sleep(50);
  }
}

export function tempDataDir(): { dir: string; remove: () => void } {
  const dir = mkdtempSync(join(tmpdir(), 'g2labs-hub-test-'));
  return { dir, remove: () => rmSync(dir, { recursive: true, force: true }) };
}

export const ADMIN_PASSWORD = 'correct horse battery staple';

export function hubEnv(broker: TestBroker, dataDir: string, extra: Record<string, string> = {}) {
  return {
    HUB_MQTT_URL: broker.url,
    HUB_ADMIN_PASSWORD: ADMIN_PASSWORD,
    HUB_DATA_DIR: dataDir,
    HUB_HTTP_HOST: '127.0.0.1',
    HUB_HTTP_PORT: '0',
    HUB_MQTT_RECONNECT_S: '0.2',
    ...extra,
  };
}

export function quietHub(env: Record<string, string>): Promise<Hub> {
  return startHub(env, { log: () => {} });
}

/** A tiny HTTP client for the admin API that keeps the session cookie. */
export class AdminClient {
  private cookie?: string;

  constructor(private baseUrl: string) {}

  async request(method: string, path: string, body?: unknown): Promise<{ status: number; json: any }> {
    const headers: Record<string, string> = {};
    if (this.cookie) headers.cookie = this.cookie;
    if (body !== undefined) headers['content-type'] = 'application/json';
    const res = await fetch(this.baseUrl + path, {
      method,
      headers,
      body: body === undefined ? undefined : JSON.stringify(body),
    });
    const setCookie = res.headers.get('set-cookie');
    if (setCookie) this.cookie = setCookie.split(';')[0];
    const text = await res.text();
    return { status: res.status, json: text ? JSON.parse(text) : undefined };
  }

  login(password = ADMIN_PASSWORD) {
    return this.request('POST', '/api/login', { password });
  }

  status() {
    return this.request('GET', '/api/status');
  }
}
