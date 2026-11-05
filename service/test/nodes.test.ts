import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { startHub, type Hub } from '../src/hub.js';
import {
  AdminClient,
  contractExamples,
  eventually,
  FakeNodes,
  hubEnv,
  Observer,
  sleep,
  quietHub,
  statusExample,
  tempDataDir,
  TestBroker,
} from './harness.js';

let broker: TestBroker;
let data: ReturnType<typeof tempDataDir>;
let hub: Hub;
let nodes: FakeNodes;
let admin: AdminClient;

const logged: string[] = [];

async function start(extraEnv: Record<string, string> = {}, now?: () => number) {
  hub = await startHub(hubEnv(broker, data.dir, extraEnv), { log: (msg) => logged.push(msg), now });
  admin = new AdminClient(hub.url);
  await admin.login();
  await eventually(() => admin.status(), (r) => r.json.broker.connected === true);
}

/** Waits until the Hub lists the Node and returns it. */
function listed(nodeId: string, check: (node: any) => boolean = () => true) {
  return eventually(
    () => admin.nodes(),
    (r) => r.json.nodes.some((n: any) => n.node_id === nodeId && check(n)),
  ).then((r) => r.json.nodes.find((n: any) => n.node_id === nodeId));
}

beforeEach(async () => {
  broker = await TestBroker.start();
  data = tempDataDir();
  logged.length = 0;
  nodes = await FakeNodes.connect(broker);
});

afterEach(async () => {
  await nodes.close();
  await hub?.close();
  await broker.stop();
  data.remove();
});

describe('Node registry', () => {
  it.each(contractExamples('status').map((e) => [e.file, JSON.parse(e.text)]))(
    'registers a Node from its Status (%s)',
    async (_file, example) => {
      await start();
      await nodes.status(example);
      const node = await listed(example.node_id);
      expect(node.status).toEqual(example);
    },
  );

  it('registers a Node from its availability, before any Status', async () => {
    await start();
    await nodes.availability('a1b2c3d4e5f6', 'online');
    expect(await listed('a1b2c3d4e5f6')).toMatchObject({ availability: 'online', status: null });

    await nodes.availability('a1b2c3d4e5f6', 'offline');
    await listed('a1b2c3d4e5f6', (n) => n.availability === 'offline');
  });

  it('registers Nodes from retained availability already on the Broker when it starts', async () => {
    await nodes.availability('a1b2c3d4e5f6', 'offline');
    await start();
    expect(await listed('a1b2c3d4e5f6')).toMatchObject({ availability: 'offline' });
  });

  it('is not a Node itself', async () => {
    await start();
    await nodes.status(statusExample());
    await listed(statusExample().node_id);
    expect((await admin.nodes()).json.nodes.map((n: any) => n.node_id)).toEqual([statusExample().node_id]);
  });

  it('keeps a Node\'s latest Status and availability side by side, and when it was last seen', async () => {
    let clock = Date.parse('2026-11-01T10:00:00Z');
    await start({}, () => clock);
    await nodes.availability('404cca0ab1f2', 'online');
    await listed('404cca0ab1f2');
    clock += 60_000;
    await nodes.status(statusExample('named.json'));
    const node = await listed('404cca0ab1f2', (n) => n.status !== null);
    expect(node).toMatchObject({
      availability: 'online',
      first_seen: '2026-11-01T10:00:00.000Z',
      last_seen: '2026-11-01T10:01:00.000Z',
      status: { name: 'Kitchen' },
    });
  });
});

describe('Payload validation', () => {
  const invalid: [string, string, string | object][] = [
    ['Status that is not JSON', 'g2labs/404cca0ab1f2/status', '{"node_id": '],
    ['Status missing a required field', 'g2labs/404cca0ab1f2/status', { ...statusExample(), fw_version: undefined }],
    ['Status with a field of the wrong type', 'g2labs/404cca0ab1f2/status', { ...statusExample(), rssi: 'weak' }],
    ['Status for another Node', 'g2labs/a1b2c3d4e5f6/status', statusExample()],
    ['availability that is neither online nor offline', 'g2labs/404cca0ab1f2/availability', 'maybe'],
    ['a topic whose Node ID is not a Node ID', 'g2labs/KITCHEN/availability', 'online'],
  ];

  it.each(invalid)('counts and logs %s, and registers nothing', async (_what, topic, payload) => {
    await start();
    await nodes.publish(topic, payload);
    await eventually(() => admin.status(), (r) => r.json.mqtt.invalid_messages === 1);
    expect(logged.some((line) => line.includes(topic))).toBe(true);
    expect((await admin.nodes()).json.nodes).toEqual([]);

    // The Hub carries on.
    await nodes.status(statusExample());
    await listed(statusExample().node_id);
  });

  it('ignores an empty availability, the Broker clearing a retained message', async () => {
    await start();
    await nodes.publish('g2labs/404cca0ab1f2/availability', '', true);
    await nodes.status(statusExample('rssi-null.json'));
    await listed(statusExample('rssi-null.json').node_id);
    expect((await admin.nodes()).json.nodes).toHaveLength(1);
    expect((await admin.status()).json.mqtt.invalid_messages).toBe(0);
  });

  it('ignores fields it does not know', async () => {
    await start();
    await nodes.status({ ...statusExample(), added_later: { x: 1 } });
    const node = await listed(statusExample().node_id);
    expect(node.status).toEqual(statusExample());
  });
});

describe('Status history', () => {
  const T0 = Date.parse('2026-11-01T10:00:00Z');
  let clock: number;

  beforeEach(async () => {
    clock = T0;
    await start({}, () => clock);
  });

  /** Publishes a Status at the given minute after T0 and waits for the Hub to have it. */
  async function statusAt(minute: number, fields: Record<string, unknown>, file = 'named.json') {
    clock = T0 + minute * 60_000;
    const status = { ...statusExample(file), ...fields };
    await nodes.status(status);
    await listed(status.node_id, (n) => n.last_seen === new Date(clock).toISOString());
  }

  it('keeps every Status for the charts, oldest first, and finds the uptime resets', async () => {
    await statusAt(0, { uptime_s: 100, rssi: -60, free_heap: 50000 });
    await statusAt(1, { uptime_s: 160, rssi: null, free_heap: 49000 });
    await statusAt(2, { uptime_s: 5, rssi: -70, free_heap: 52000 }); // rebooted
    await statusAt(3, { uptime_s: 65, rssi: -71, free_heap: 51000 });

    const res = await admin.node('404cca0ab1f2');
    expect(res.status).toBe(200);
    expect(res.json).toMatchObject({ node_id: '404cca0ab1f2', status: { uptime_s: 65 } });
    expect(res.json.history).toEqual([
      { at: '2026-11-01T10:00:00.000Z', uptime_s: 100, rssi: -60, free_heap: 50000 },
      { at: '2026-11-01T10:01:00.000Z', uptime_s: 160, rssi: null, free_heap: 49000 },
      { at: '2026-11-01T10:02:00.000Z', uptime_s: 5, rssi: -70, free_heap: 52000 },
      { at: '2026-11-01T10:03:00.000Z', uptime_s: 65, rssi: -71, free_heap: 51000 },
    ]);
    expect(res.json.uptime_resets).toEqual(['2026-11-01T10:02:00.000Z']);
  });

  it('keeps each Node\'s history apart', async () => {
    await statusAt(0, { uptime_s: 1 });
    await statusAt(1, { uptime_s: 2 }, 'rssi-null.json');
    expect((await admin.node('404cca0ab1f2')).json.history).toHaveLength(1);
    expect((await admin.node('a1b2c3d4e5f6')).json.history).toHaveLength(1);
  });

  it('answers 404 for a Node it has never seen', async () => {
    expect((await admin.node('000000000000')).status).toBe(404);
  });
});

describe('Status retention', () => {
  const T0 = Date.parse('2026-10-01T00:00:00Z');
  const DAY = 24 * 3600 * 1000;
  let clock: number;

  async function statusOnDay(day: number, uptime_s: number) {
    clock = T0 + day * DAY;
    await nodes.status({ ...statusExample(), uptime_s });
    await listed('404cca0ab1f2', (n) => n.last_seen === new Date(clock).toISOString());
  }

  const uptimes = async () => (await admin.node('404cca0ab1f2')).json.history.map((p: any) => p.uptime_s);

  it('keeps 30 days of Status history by default', async () => {
    clock = T0;
    await start({}, () => clock);
    await statusOnDay(0, 1);
    await statusOnDay(29, 2);
    expect(await uptimes()).toEqual([1, 2]);
    await statusOnDay(31, 3);
    expect(await uptimes()).toEqual([2, 3]);
  });

  it('drops the history of a silent Node too, but keeps the Node', async () => {
    clock = T0;
    await start({}, () => clock);
    await statusOnDay(0, 1);
    clock = T0 + 31 * DAY;
    const node = (await admin.node('404cca0ab1f2')).json;
    expect(node.history).toEqual([]);
    expect(node.status.uptime_s).toBe(1);
  });

  it('keeps as many days as HUB_STATUS_RETENTION_DAYS says', async () => {
    clock = T0;
    await start({ HUB_STATUS_RETENTION_DAYS: '2' }, () => clock);
    await statusOnDay(0, 1);
    await statusOnDay(1, 2);
    await statusOnDay(4, 3);
    expect(await uptimes()).toEqual([3]);
  });

  it('refuses a retention that is not a positive number of days', async () => {
    await expect(quietHub(hubEnv(broker, data.dir, { HUB_STATUS_RETENTION_DAYS: '0' }))).rejects.toThrow(
      /HUB_STATUS_RETENTION_DAYS/,
    );
  });
});

describe('Forget', () => {
  it('deletes the Node and its history, and clears its retained availability', async () => {
    await start();
    await nodes.availability('404cca0ab1f2', 'offline');
    await nodes.status({ ...statusExample(), uptime_s: 1 });
    await listed('404cca0ab1f2', (n) => n.status !== null);

    expect((await admin.forget('404cca0ab1f2')).status).toBe(200);
    expect((await admin.nodes()).json.nodes).toEqual([]);
    expect((await admin.node('404cca0ab1f2')).status).toBe(404);

    // Nothing retained is left for a newcomer on the Broker.
    const late = await Observer.connect(broker, 'g2labs/404cca0ab1f2/#');
    try {
      await sleep(300);
      expect(late.messages).toEqual([]);
    } finally {
      await late.close();
    }

    // Back with a fresh history when it reports again.
    await nodes.status({ ...statusExample(), uptime_s: 2 });
    await listed('404cca0ab1f2');
    expect((await admin.node('404cca0ab1f2')).json.history.map((p: any) => p.uptime_s)).toEqual([2]);
  });

  it('answers 404 for a Node it has never seen', async () => {
    await start();
    expect((await admin.forget('000000000000')).status).toBe(404);
  });
});

describe('Live updates', () => {
  it('tells the admin page about every change to a Node', async () => {
    await start();
    const stream = await admin.events();
    try {
      expect(stream.status).toBe(200);
      await nodes.availability('404cca0ab1f2', 'online');
      await stream.waitFor((e) => e.event === 'node' && e.data.node_id === '404cca0ab1f2');

      const from = stream.events.length;
      await nodes.status(statusExample());
      await stream.waitFor((e) => e.event === 'node' && e.data.node_id === '404cca0ab1f2', from);

      const beforeForget = stream.events.length;
      await admin.forget('404cca0ab1f2');
      await stream.waitFor((e) => e.event === 'node' && e.data.node_id === '404cca0ab1f2', beforeForget);
    } finally {
      stream.close();
    }
  });

  it('needs a login', async () => {
    await start();
    const stream = await new AdminClient(hub.url).events();
    stream.close();
    expect(stream.status).toBe(401);
  });
});
