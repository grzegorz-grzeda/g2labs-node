import Database from 'better-sqlite3';
import { existsSync } from 'node:fs';
import { join } from 'node:path';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import type { Hub } from '../src/hub.js';
import { AdminClient, eventually, hubEnv, Observer, quietHub, tempDataDir, TestBroker } from './harness.js';

const AVAILABILITY = 'g2labs/hub/availability';

let broker: TestBroker;
let data: ReturnType<typeof tempDataDir>;
let hub: Hub | undefined;
const observers: Observer[] = [];

async function observe(b = broker, options = {}): Promise<Observer> {
  const o = await Observer.connect(b, AVAILABILITY, options);
  observers.push(o);
  return o;
}

beforeEach(async () => {
  broker = await TestBroker.start();
  data = tempDataDir();
  hub = undefined;
});

afterEach(async () => {
  await Promise.all(observers.splice(0).map((o) => o.close()));
  await hub?.close();
  await broker.stop();
  data.remove();
});

describe('Hub availability on the Broker', () => {
  it('publishes retained online after connecting', async () => {
    hub = await quietHub(hubEnv(broker, data.dir));
    const live = await observe();
    await live.waitFor((m) => m.payload === 'online');

    // A client subscribing later still gets it, retained.
    const late = await observe();
    const msg = await late.waitFor((m) => m.topic === AVAILABILITY);
    expect(msg).toEqual({ topic: AVAILABILITY, payload: 'online', retain: true });
  });

  it('publishes retained offline when it shuts down', async () => {
    hub = await quietHub(hubEnv(broker, data.dir));
    const o = await observe();
    await o.waitFor((m) => m.payload === 'online');
    await hub.close();
    hub = undefined;
    await o.waitFor((m) => m.payload === 'offline');

    const late = await observe();
    expect(await late.waitFor((m) => m.topic === AVAILABILITY)).toMatchObject({ payload: 'offline', retain: true });
  });

  it('leaves offline as its last will on a lost connection, then reconnects and says online again', async () => {
    hub = await quietHub(hubEnv(broker, data.dir));
    const o = await observe();
    await o.waitFor((m) => m.payload === 'online');
    const from = o.messages.length;

    broker.dropClient('g2labs-hub');

    const will = await o.waitFor((m) => m.payload === 'offline', from);
    const back = await o.waitFor((m) => m.payload === 'online', o.messages.indexOf(will) + 1);
    expect(back.topic).toBe(AVAILABILITY);
  });

  it('reconnects after the Broker restarts', async () => {
    hub = await quietHub(hubEnv(broker, data.dir));
    const admin = new AdminClient(hub.url);
    await admin.login();
    await eventually(() => admin.status(), (r) => r.json.broker.connected === true);

    const port = broker.port;
    await broker.stop();
    await eventually(() => admin.status(), (r) => r.json.broker.connected === false);

    broker = await TestBroker.start({ port });
    const o = await observe();
    await o.waitFor((m) => m.payload === 'online');
    await eventually(() => admin.status(), (r) => r.json.broker.connected === true);
  });

  it('logs in to the Broker with the configured username and password', async () => {
    await broker.stop();
    const credentials = { username: 'hub', password: 's3cret' };
    broker = await TestBroker.start({ credentials });
    hub = await quietHub(
      hubEnv(broker, data.dir, { HUB_MQTT_USERNAME: credentials.username, HUB_MQTT_PASSWORD: credentials.password }),
    );
    const o = await observe(broker, credentials);
    await o.waitFor((m) => m.payload === 'online');
  });
});

describe('Admin page', () => {
  beforeEach(async () => {
    hub = await quietHub(hubEnv(broker, data.dir));
  });

  it('refuses the API without logging in', async () => {
    const res = await new AdminClient(hub!.url).status();
    expect(res.status).toBe(401);
  });

  it('refuses a wrong password', async () => {
    const admin = new AdminClient(hub!.url);
    expect((await admin.login('nope')).status).toBe(401);
    expect((await admin.status()).status).toBe(401);
  });

  it('shows the Broker connection after logging in, until logging out', async () => {
    const admin = new AdminClient(hub!.url);
    expect((await admin.login()).status).toBe(200);
    const res = await eventually(() => admin.status(), (r) => r.json?.broker?.connected === true);
    expect(res.status).toBe(200);
    expect(res.json.broker.url).toBe(broker.url);

    expect((await admin.request('POST', '/api/logout')).status).toBe(200);
    expect((await admin.status()).status).toBe(401);
  });

  it('never shows Broker credentials', async () => {
    await hub!.close();
    hub = await quietHub(hubEnv(broker, data.dir, { HUB_MQTT_URL: `mqtt://hub:pw@127.0.0.1:${broker.port}` }));
    const admin = new AdminClient(hub.url);
    await admin.login();
    const res = await admin.status();
    expect(res.json.broker.url).toBe(broker.url);
  });
});

describe('Configuration and data', () => {
  it('refuses to start without an admin password', async () => {
    const env: Record<string, string> = hubEnv(broker, data.dir);
    delete env.HUB_ADMIN_PASSWORD;
    await expect(quietHub(env)).rejects.toThrow(/HUB_ADMIN_PASSWORD/);
  });

  it('keeps a migrated SQLite database in WAL mode in the data directory', async () => {
    const dir = join(data.dir, 'nested');
    hub = await quietHub(hubEnv(broker, dir));
    await hub.close();
    // Starting again on the same data runs no migration twice.
    hub = await quietHub(hubEnv(broker, dir));
    await hub.close();
    hub = undefined;

    const file = join(dir, 'hub.db');
    expect(existsSync(file)).toBe(true);
    const db = new Database(file, { readonly: true });
    try {
      expect(db.pragma('journal_mode', { simple: true })).toBe('wal');
      expect(db.pragma('user_version', { simple: true })).toBeGreaterThanOrEqual(1);
    } finally {
      db.close();
    }
  });
});
