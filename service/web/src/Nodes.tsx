// The Node list and a Node's detail page, refreshed by the Hub's live events.

import { useCallback, useEffect, useState } from 'react';
import type { NodeDetail, NodeList, NodeSummary } from '../../src/api.js';
import { Chart } from './Chart';

/** Calls `onChange` with the Node ID of every `node` event the Hub sends. */
export function useNodeEvents(onChange: (nodeId: string) => void) {
  useEffect(() => {
    const source = new EventSource('/api/events');
    const listener = (e: MessageEvent) => onChange((JSON.parse(e.data) as { node_id: string }).node_id);
    source.addEventListener('node', listener);
    return () => source.close();
  }, [onChange]);
}

/** Re-renders every `ms`, for "seen 5 s ago". */
function useNow(ms = 1000): number {
  const [now, setNow] = useState(Date.now());
  useEffect(() => {
    const timer = setInterval(() => setNow(Date.now()), ms);
    return () => clearInterval(timer);
  }, [ms]);
  return now;
}

function ago(iso: string, now: number): string {
  const s = Math.max(0, Math.round((now - Date.parse(iso)) / 1000));
  if (s < 60) return `${s} s ago`;
  if (s < 3600) return `${Math.floor(s / 60)} min ago`;
  if (s < 86400) return `${Math.floor(s / 3600)} h ago`;
  return `${Math.floor(s / 86400)} d ago`;
}

function Availability({ value }: { value: NodeSummary['availability'] }) {
  const cls = value === 'online' ? 'dot ok' : value === 'offline' ? 'dot bad' : 'dot unknown';
  return (
    <>
      <span className={cls} aria-hidden="true" />
      {value ?? 'unknown'}
    </>
  );
}

function rssi(value: number | null | undefined): string {
  return value === null || value === undefined ? '—' : `${value} dBm`;
}

export function NodeListPage() {
  const [nodes, setNodes] = useState<NodeSummary[] | null>(null);
  const [error, setError] = useState<string>();
  const now = useNow();

  const load = useCallback(async () => {
    try {
      const res = await fetch('/api/nodes');
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      setNodes(((await res.json()) as NodeList).nodes);
      setError(undefined);
    } catch (err) {
      setError(`Can't load the Nodes (${String(err)})`);
    }
  }, []);

  useEffect(() => void load(), [load]);
  useNodeEvents(load);

  return (
    <section>
      <h2>Nodes</h2>
      {error && <p className="error">{error}</p>}
      {nodes === null && !error && <p>Loading…</p>}
      {nodes?.length === 0 && <p className="muted">No Node has shown up on the Broker yet.</p>}
      {nodes && nodes.length > 0 && (
        <div className="table-scroll">
          <table>
            <thead>
              <tr>
                <th>Node</th>
                <th>Availability</th>
                <th>Firmware</th>
                <th>Board</th>
                <th>Known Network</th>
                <th>RSSI</th>
                <th>Last seen</th>
              </tr>
            </thead>
            <tbody>
              {nodes.map((n) => (
                <tr key={n.node_id}>
                  <td>
                    <a href={`#/nodes/${n.node_id}`}>{n.status?.name ?? n.node_id}</a>
                    {n.status?.name && <div className="muted mono">{n.node_id}</div>}
                  </td>
                  <td>
                    <Availability value={n.availability} />
                  </td>
                  <td>{n.status?.fw_version ?? '—'}</td>
                  <td className="mono">{n.status?.board ?? '—'}</td>
                  <td>{n.status?.network ?? '—'}</td>
                  <td>{rssi(n.status?.rssi)}</td>
                  <td title={n.last_seen}>{ago(n.last_seen, now)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </section>
  );
}

export function NodeDetailPage({ nodeId }: { nodeId: string }) {
  const [node, setNode] = useState<NodeDetail | null | 'gone'>(null);
  const [error, setError] = useState<string>();
  const now = useNow();

  const load = useCallback(async () => {
    try {
      const res = await fetch(`/api/nodes/${nodeId}`);
      if (res.status === 404) return setNode('gone');
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      setNode((await res.json()) as NodeDetail);
      setError(undefined);
    } catch (err) {
      setError(`Can't load the Node (${String(err)})`);
    }
  }, [nodeId]);

  useEffect(() => void load(), [load]);
  useNodeEvents(
    useCallback(
      (changed: string) => {
        if (changed === nodeId) void load();
      },
      [nodeId, load],
    ),
  );

  async function forget() {
    if (!confirm(`Forget Node ${nodeId}? Its Status history is deleted. A Node still running shows up again with its next Status.`)) {
      return;
    }
    const res = await fetch(`/api/nodes/${nodeId}`, { method: 'DELETE' });
    if (res.ok || res.status === 404) location.hash = '#/';
    else setError(`Forget failed (HTTP ${res.status})`);
  }

  const back = <a href="#/">← All Nodes</a>;
  if (node === 'gone') {
    return (
      <section>
        {back}
        <p>Node {nodeId} isn't registered.</p>
      </section>
    );
  }
  if (node === null) {
    return (
      <section>
        {back}
        {error ? <p className="error">{error}</p> : <p>Loading…</p>}
      </section>
    );
  }

  const s = node.status;
  const at = (iso: string) => Date.parse(iso);
  const resets = node.uptime_resets.map(at);
  return (
    <section>
      {back}
      <div className="title-row">
        <h2>{s?.name ?? node.node_id}</h2>
        <button type="button" className="danger" onClick={forget}>
          Forget
        </button>
      </div>
      {error && <p className="error">{error}</p>}
      <dl>
        <dt>Node ID</dt>
        <dd className="mono">{node.node_id}</dd>
        <dt>Availability</dt>
        <dd>
          <Availability value={node.availability} />
        </dd>
        <dt>Firmware</dt>
        <dd>{s?.fw_version ?? '—'}</dd>
        <dt>Board</dt>
        <dd className="mono">{s?.board ?? '—'}</dd>
        <dt>Known Network</dt>
        <dd>{s?.network ?? '—'}</dd>
        <dt>IP address</dt>
        <dd className="mono">{s?.ip ?? '—'}</dd>
        <dt>RSSI</dt>
        <dd>{rssi(s?.rssi)}</dd>
        <dt>Uptime</dt>
        <dd>{s ? `${s.uptime_s} s` : '—'}</dd>
        <dt>Last seen</dt>
        <dd title={node.last_seen}>{ago(node.last_seen, now)}</dd>
        <dt>First seen</dt>
        <dd>{new Date(node.first_seen).toLocaleString()}</dd>
      </dl>

      <h3>Status history</h3>
      <p className="muted">
        {node.history.length} Status reports, {resets.length} uptime {resets.length === 1 ? 'reset' : 'resets'} (red lines).
      </p>
      <Chart
        title="RSSI (dBm)"
        points={node.history.map((p) => ({ at: at(p.at), value: p.rssi }))}
        marks={resets}
        format={(v) => `${Math.round(v)}`}
      />
      <Chart
        title="Free memory (bytes)"
        points={node.history.map((p) => ({ at: at(p.at), value: p.free_heap }))}
        marks={resets}
        format={(v) => `${Math.round(v)}`}
      />
      <Chart
        title="Uptime (s)"
        points={node.history.map((p) => ({ at: at(p.at), value: p.uptime_s }))}
        marks={resets}
        format={(v) => `${Math.round(v)}`}
      />
    </section>
  );
}
