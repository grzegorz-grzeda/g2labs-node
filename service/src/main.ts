// Runs the Hub, configured from environment variables, until SIGINT/SIGTERM.

import { ConfigError } from './config.js';
import { startHub } from './hub.js';

try {
  const hub = await startHub(process.env);
  let stopping = false;
  const stop = async () => {
    if (stopping) return;
    stopping = true;
    await hub.close();
    process.exit(0);
  };
  process.on('SIGINT', stop);
  process.on('SIGTERM', stop);
} catch (err) {
  console.error(err instanceof ConfigError ? `config: ${err.message}` : err);
  process.exit(1);
}
