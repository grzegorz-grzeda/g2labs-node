import { useCallback, useEffect, useRef, useState, type FormEvent } from 'react';

interface Status {
  broker: { url: string; connected: boolean };
}

type State =
  | { kind: 'loading' }
  | { kind: 'login'; error?: string }
  | { kind: 'ready'; status: Status }
  | { kind: 'error'; message: string };

const POLL_MS = 2000;

export function App() {
  const [state, setState] = useState<State>({ kind: 'loading' });

  const refresh = useCallback(async () => {
    try {
      const res = await fetch('/api/status');
      if (res.status === 401) {
        setState((s) => (s.kind === 'login' ? s : { kind: 'login' }));
        return;
      }
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      setState({ kind: 'ready', status: (await res.json()) as Status });
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
      {state.kind === 'ready' && <Overview status={state.status} />}
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

function Overview({ status }: { status: Status }) {
  const { connected, url } = status.broker;
  return (
    <section>
      <h2>Broker</h2>
      <p>
        <span className={connected ? 'dot ok' : 'dot bad'} aria-hidden="true" />
        {connected ? 'Connected' : 'Not connected'} to <code>{url}</code>
      </p>
    </section>
  );
}
