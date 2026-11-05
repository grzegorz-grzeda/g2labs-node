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
import type { NodeRegistry } from './nodes.js';
import type { HubStatus, NodeList } from './api.js';

export const SESSION_COOKIE = 'hub_session';
const SESSION_TTL_MS = 30 * 24 * 3600 * 1000;

export interface HttpDeps {
  config: Config;
  broker: BrokerConnection;
  nodes: NodeRegistry;
  /** Directory of the built admin page, if there is one. */
  publicDir?: string;
}

function digest(s: string): Buffer {
  return createHash('sha256').update(s).digest();
}

export function createHttp({ config, broker, nodes, publicDir }: HttpDeps): FastifyInstance {
  // Event streams stay open, so closing must not wait for them.
  const app = Fastify({ logger: false, forceCloseConnections: true });
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

  app.get('/api/status', { preHandler: requireSession }, async (): Promise<HubStatus> => ({
    broker: { url: redact(config.mqttUrl), connected: broker.connected },
    mqtt: { invalid_messages: nodes.invalidMessages },
  }));

  app.get('/api/nodes', { preHandler: requireSession }, async (): Promise<NodeList> => ({ nodes: nodes.list() }));

  app.get<{ Params: { id: string } }>('/api/nodes/:id', { preHandler: requireSession }, async (req, reply) => {
    const node = nodes.get(req.params.id);
    return node ?? reply.code(404).send({ error: 'no such Node' });
  });

  // Live updates: a server-sent `node` event, with the Node ID, whenever a
  // Node changes or is forgotten. The page then fetches what it shows.
  app.get('/api/events', { preHandler: requireSession }, async (req, reply) => {
    reply.hijack();
    const res = reply.raw;
    res.writeHead(200, {
      'content-type': 'text/event-stream',
      'cache-control': 'no-cache',
      connection: 'keep-alive',
    });
    res.write(': connected\n\n');
    const unsubscribe = nodes.onChange((nodeId) => {
      res.write(`event: node\ndata: ${JSON.stringify({ node_id: nodeId })}\n\n`);
    });
    // Keeps proxies from timing out an idle stream.
    const keepalive = setInterval(() => res.write(': keepalive\n\n'), 30_000);
    req.raw.on('close', () => {
      clearInterval(keepalive);
      unsubscribe();
    });
  });

  app.delete<{ Params: { id: string } }>('/api/nodes/:id', { preHandler: requireSession }, async (req, reply) => {
    return (await nodes.forget(req.params.id)) ? { ok: true } : reply.code(404).send({ error: 'no such Node' });
  });

  if (publicDir && existsSync(publicDir)) {
    app.register(fastifyStatic, { root: publicDir });
  }

  return app;
}
