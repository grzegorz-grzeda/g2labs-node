// The Hub's connection to the Broker: announces the Hub's availability and
// keeps reconnecting after a failed connect or a lost connection.

import mqtt, { type MqttClient } from 'mqtt';
import type { Config } from './config.js';

export const HUB_AVAILABILITY_TOPIC = 'g2labs/hub/availability';
export const HUB_CLIENT_ID = 'g2labs-hub';

export interface BrokerConnection {
  readonly connected: boolean;
  /** Publishes `offline` and disconnects. */
  close(): Promise<void>;
}

export function connectBroker(config: Config, log: (msg: string) => void): BrokerConnection {
  const client: MqttClient = mqtt.connect(config.mqttUrl, {
    clientId: HUB_CLIENT_ID,
    username: config.mqttUsername,
    password: config.mqttPassword,
    protocolVersion: 4,
    clean: true,
    keepalive: 60,
    reconnectPeriod: config.mqttReconnectMs,
    connectTimeout: 30_000,
    will: { topic: HUB_AVAILABILITY_TOPIC, payload: Buffer.from('offline'), qos: 1, retain: true },
  });

  client.on('connect', () => {
    log(`connected to the Broker at ${redact(config.mqttUrl)}`);
    client.publish(HUB_AVAILABILITY_TOPIC, 'online', { qos: 1, retain: true });
  });
  client.on('close', () => log('disconnected from the Broker'));
  client.on('error', (err) => log(`Broker error: ${err.message}`));

  return {
    get connected() {
      return client.connected;
    },
    async close() {
      // A clean disconnect doesn't fire the last will, so say `offline` first.
      if (client.connected) {
        await client.publishAsync(HUB_AVAILABILITY_TOPIC, 'offline', { qos: 1, retain: true });
      }
      await client.endAsync();
    },
  };
}

/** The URL without any credentials in it, for logs and the admin page. */
export function redact(url: string): string {
  try {
    const parsed = new URL(url);
    parsed.username = '';
    parsed.password = '';
    return parsed.toString().replace(/\/$/, '');
  } catch {
    return url;
  }
}
