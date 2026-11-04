// The Hub's SQLite database: one file in the data directory, in WAL mode,
// brought up to date by the migrations below when the Hub starts.

import Database from 'better-sqlite3';
import { mkdirSync } from 'node:fs';
import { join } from 'node:path';

export type Db = Database.Database;

/**
 * Schema migrations, applied in order. Migration N brings the schema to
 * version N (SQLite's `user_version`). Only ever append: a released migration
 * never changes, because databases in the field have already run it.
 */
const migrations: string[] = [
  // 1: key/value Hub settings.
  `CREATE TABLE settings (
     key   TEXT PRIMARY KEY,
     value TEXT NOT NULL
   ) STRICT;`,
];

export const DB_FILE = 'hub.db';

export function openDb(dataDir: string): Db {
  mkdirSync(dataDir, { recursive: true });
  const db = new Database(join(dataDir, DB_FILE));
  db.pragma('journal_mode = WAL');
  db.pragma('foreign_keys = ON');
  migrate(db);
  return db;
}

function migrate(db: Db): void {
  const current = db.pragma('user_version', { simple: true }) as number;
  if (current > migrations.length) {
    throw new Error(
      `database schema version ${current} is newer than this Hub knows (${migrations.length}); ` +
        'was the Hub downgraded?',
    );
  }
  for (let version = current + 1; version <= migrations.length; version++) {
    db.transaction(() => {
      db.exec(migrations[version - 1]);
      db.pragma(`user_version = ${version}`);
    })();
  }
}
