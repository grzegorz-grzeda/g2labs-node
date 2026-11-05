import { useCallback, useEffect, useRef, useState, type FormEvent } from 'react';
import type { HubStatus } from '../../src/api.js';
import { NodeDetailPage, NodeListPage } from './Nodes';

/** The page to show, from the URL's hash: #/ or #/nodes/<node-id>. */
function useRoute(): { nodeId?: string } {
  const parse = () => {
    const match = /^#\/nodes\/([^/]+)$/.exec(location.hash);
    return match ? { nodeId: decodeURIComponent(match[1]) } : {};
  };
  const [route, setRoute] = useState(parse);
  useEffect(() => {
    const onChange = () => setRoute(parse());
    window.addEventListener('hashchange', onChange);
    return () => window.removeEventListener('hashchange', onChange);
  }, []);
  return route;
}

type State =
  | { kind: 'loading' }
  | { kind: 'login'; error?: string }
  | { kind: 'ready'; status: HubStatus }
  | { kind: 'error'; message: string };

const POLL_MS = 2000;

export function App() {
  const [state, setState] = useState<State>({ kind: 'loading' });
  const route = useRoute();

  const refresh = useCallback(async () => {
    try {
      const res = await fetch('/api/status');
      if (res.status === 401) {
        setState((s) => (s.kind === 'login' ? s : { kind: 'login' }));
        return;
      }
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      setState({ kind: 'ready', status: (await res.json()) as HubStatus });
    } catch (err) {
      setState({ kind: 'error', message: `Can't reach the Hub (${String(err)})` });
    }
  }, []);

  // Poll while logged in (or retrying after an error), not on the login form.
  const polling = useRef(true);
  polling.current = state.kind !== 'login';

  useEffect(() => {
    refresh();
    const timer = setInterval(() => {
      if (polling.current) refresh();
    }, POLL_MS);
    return () => clearInterval(timer);
  }, [refresh]);

  async function logout() {
    await fetch('/api/logout', { method: 'POST' });
    setState({ kind: 'login' });
  }

  return (
    <main>
      <header>
        <h1>g2labs Hub</h1>
        {state.kind === 'ready' && (
          <button type="button" onClick={logout}>
            Log out
          </button>
        )}
      </header>
      {state.kind === 'loading' && <p>Loading…</p>}
      {state.kind === 'error' && <p className="error">{state.message}</p>}
      {state.kind === 'login' && <Login error={state.error} onLoggedIn={refresh} onError={(error) => setState({ kind: 'login', error })} />}
      {state.kind === 'ready' && (
        <>
          <Overview status={state.status} />
          {route.nodeId ? <NodeDetailPage key={route.nodeId} nodeId={route.nodeId} /> : <NodeListPage />}
        </>
      )}
    </main>
  );
}

function Login(props: { error?: string; onLoggedIn: () => void; onError: (error: string) => void }) {
  const [password, setPassword] = useState('');

  async function submit(e: FormEvent) {
    e.preventDefault();
    const res = await fetch('/api/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ password }),
    });
    setPassword('');
    if (res.ok) props.onLoggedIn();
    else props.onError(res.status === 401 ? 'Wrong password' : `Login failed (HTTP ${res.status})`);
  }

  return (
    <form onSubmit={submit}>
      <label>
        Admin password
        <input type="password" autoFocus autoComplete="current-password" value={password} onChange={(e) => setPassword(e.target.value)} />
      </label>
      <button type="submit">Log in</button>
      {props.error && <p className="error">{props.error}</p>}
    </form>
  );
}

function Overview({ status }: { status: HubStatus }) {
  const { connected, url } = status.broker;
  const invalid = status.mqtt.invalid_messages;
  return (
    <section>
      <p>
        <span className={connected ? 'dot ok' : 'dot bad'} aria-hidden="true" />
        Broker: {connected ? 'connected' : 'not connected'} to <code>{url}</code>
      </p>
      {invalid > 0 && (
        <p className="error">
          {invalid} invalid {invalid === 1 ? 'message' : 'messages'} from Nodes ignored since the Hub started (see the
          Hub's log).
        </p>
      )}
    </section>
  );
}
