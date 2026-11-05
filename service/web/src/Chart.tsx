// A small time-series line chart in SVG. Null values are gaps; `marks` are
// vertical lines at given times (uptime resets).

export interface ChartPoint {
  at: number;
  value: number | null;
}

const W = 600;
const H = 140;
const PAD = { left: 56, right: 8, top: 8, bottom: 20 };

export function Chart(props: {
  title: string;
  points: ChartPoint[];
  marks?: number[];
  format: (value: number) => string;
}) {
  const values = props.points.flatMap((p) => (p.value === null ? [] : [p.value]));
  if (props.points.length === 0 || values.length === 0) {
    return (
      <figure className="chart">
        <figcaption>{props.title}</figcaption>
        <p className="muted">No data yet.</p>
      </figure>
    );
  }

  const t0 = props.points[0].at;
  const t1 = Math.max(props.points[props.points.length - 1].at, t0 + 1);
  let lo = Math.min(...values);
  let hi = Math.max(...values);
  if (lo === hi) {
    lo -= 1;
    hi += 1;
  }
  const x = (t: number) => PAD.left + ((t - t0) / (t1 - t0)) * (W - PAD.left - PAD.right);
  const y = (v: number) => PAD.top + (1 - (v - lo) / (hi - lo)) * (H - PAD.top - PAD.bottom);

  // One polyline per run of non-null values.
  const runs: string[] = [];
  let run: string[] = [];
  for (const p of props.points) {
    if (p.value === null) {
      if (run.length) runs.push(run.join(' '));
      run = [];
    } else {
      run.push(`${x(p.at).toFixed(1)},${y(p.value).toFixed(1)}`);
    }
  }
  if (run.length) runs.push(run.join(' '));

  const time = (t: number) => new Date(t).toLocaleString(undefined, { dateStyle: 'short', timeStyle: 'short' });

  return (
    <figure className="chart">
      <figcaption>{props.title}</figcaption>
      <svg viewBox={`0 0 ${W} ${H}`} role="img" aria-label={props.title}>
        <line className="axis" x1={PAD.left} x2={W - PAD.right} y1={H - PAD.bottom} y2={H - PAD.bottom} />
        <text className="label" x={PAD.left - 4} y={y(hi) + 4} textAnchor="end">
          {props.format(hi)}
        </text>
        <text className="label" x={PAD.left - 4} y={y(lo)} textAnchor="end">
          {props.format(lo)}
        </text>
        <text className="label" x={PAD.left} y={H - 4}>
          {time(t0)}
        </text>
        <text className="label" x={W - PAD.right} y={H - 4} textAnchor="end">
          {time(t1)}
        </text>
        {(props.marks ?? []).map((t) => (
          <line key={t} className="mark" x1={x(t)} x2={x(t)} y1={PAD.top} y2={H - PAD.bottom}>
            <title>Restarted before {time(t)}</title>
          </line>
        ))}
        {runs.map((r, i) =>
          r.includes(' ') ? (
            <polyline key={i} className="line" points={r} />
          ) : (
            <circle key={i} className="dot-point" cx={r.split(',')[0]} cy={r.split(',')[1]} r={2} />
          ),
        )}
      </svg>
    </figure>
  );
}
