// The admin page and its API. The page itself is static; everything it shows
// comes from /api, which needs a session from logging in with the admin
// password.

import Fastify, { type FastifyInstance, type FastifyReply, type FastifyRequest } from 'fastify';
import fastifyCookie from '@fastify/cookie';
import fastifyStatic from '@fastify/static';
import { createHash, randomBytes, timingSafeEqual } from 'node:crypto';
import { existsSync } from 'node:fs';
import type { Config } from './config.js';
import { redact, type BrokerConnection } from './broker.js';

export const SESSION_COOKIE = 'hub_session';
const SESSION_TTL_MS = 30 * 24 * 3600 * 1000;

export interface HttpDeps {
  config: Config;
  broker: BrokerConnection;
  /** Directory of the built admin page, if there is one. */
  publicDir?: string;
}

function digest(s: string): Buffer {
  return createHash('sha256').update(s).digest();
}

export function createHttp({ config, broker, publicDir }: HttpDeps): FastifyInstance {
  const app = Fastify({ logger: false });
  // Session token -> expiry (ms since epoch). In memory: a restart logs out.
  const sessions = new Map<string, number>();
  const passwordDigest = digest(config.adminPassword);

  app.register(fastifyCookie);

  function session(req: FastifyRequest): string | undefined {
    const token = req.cookies[SESSION_COOKIE];
    if (!token) return undefined;
    const expiry = sessions.get(token);
    if (expiry === undefined) return undefined;
    if (expiry < Date.now()) {
      sessions.delete(token);
      return undefined;
    }
    return token;
  }

  async function requireSession(req: FastifyRequest, reply: FastifyReply) {
    if (!session(req)) {
      return reply.code(401).send({ error: 'login required' });
    }
  }

  app.post<{ Body: { password?: unknown } }>('/api/login', async (req, reply) => {
    const password = req.body?.password;
    if (typeof password !== 'string' || !timingSafeEqual(digest(password), passwordDigest)) {
      return reply.code(401).send({ error: 'wrong password' });
    }
    const token = randomBytes(32).toString('base64url');
    sessions.set(token, Date.now() + SESSION_TTL_MS);
    return reply
      .setCookie(SESSION_COOKIE, token, {
        path: '/',
        httpOnly: true,
        sameSite: 'strict',
        maxAge: SESSION_TTL_MS / 1000,
      })
      .send({ ok: true });
  });

  app.post('/api/logout', async (req, reply) => {
    const token = session(req);
    if (token) sessions.delete(token);
    return reply.clearCookie(SESSION_COOKIE, { path: '/' }).send({ ok: true });
  });

  app.get('/api/status', { preHandler: requireSession }, async () => ({
    broker: { url: redact(config.mqttUrl), connected: broker.connected },
  }));

  if (publicDir && existsSync(publicDir)) {
    app.register(fastifyStatic, { root: publicDir });
  }

  return app;
}
