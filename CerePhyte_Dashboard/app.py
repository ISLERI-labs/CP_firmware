import csv
import time
from datetime import datetime, timezone
from pathlib import Path

import pandas as pd
import plotly.graph_objects as go
import requests
import streamlit as st

APP_DIR = Path(__file__).resolve().parent
CSV_PATH = APP_DIR / "cerephyte_history.csv"
CSV_COLUMNS = ["captured_at", "sample", "a0", "a1", "bat", "curr", "t2", "t3", "age_ms"]
DEFAULT_API = "http://127.0.0.1:8000/telemetry"

st.set_page_config(
    page_title="CerePhyte | Live Instrumentation",
    page_icon="🌿",
    layout="wide",
    initial_sidebar_state="expanded",
)

st.markdown("""
<style>
:root { color-scheme: dark; }
.block-container { padding-top: 1.1rem; padding-bottom: 2rem; max-width: 1600px; }
[data-testid="stAppViewContainer"] { background: #0a0f16; }
[data-testid="stSidebar"] { background: #101824; border-right: 1px solid #233044; }
.hero {
  padding: 1.15rem 1.4rem; border: 1px solid #26364a; border-radius: 18px;
  background: linear-gradient(120deg, #142233 0%, #101923 65%, #10221f 100%);
  margin-bottom: 1rem;
}
.eyebrow { color: #71dfbd; font-size: .76rem; font-weight: 700; letter-spacing: .16em; }
.hero h1 { margin: .25rem 0 .15rem 0; font-size: 2rem; color: #f2f7fc; }
.hero p { margin: 0; color: #9fb1c5; }
div[data-testid="stMetric"] {
  background: #111b28; border: 1px solid #243449; border-radius: 14px;
  padding: .8rem 1rem;
}
div[data-testid="stMetricLabel"] { color: #9fb1c5; }
div[data-testid="stMetricValue"] { color: #edf5ff; }
.panel {
  border: 1px solid #243449; border-radius: 15px; padding: 1rem 1.1rem;
  background: #0f1722; margin-bottom: .7rem;
}
.small-muted { color: #93a5ba; font-size: .86rem; }
</style>
""", unsafe_allow_html=True)


def init_csv():
    if not CSV_PATH.exists() or CSV_PATH.stat().st_size == 0:
        with CSV_PATH.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=CSV_COLUMNS)
            writer.writeheader()


def history_count():
    try:
        with CSV_PATH.open("r", newline="", encoding="utf-8") as f:
            return max(0, sum(1 for _ in f) - 1)
    except OSError:
        return 0


def get_latest_sample():
    api_url = st.session_state.get("api_url", DEFAULT_API).strip().rstrip("/")
    try:
        response = requests.get(api_url, timeout=1.2, headers={"Cache-Control": "no-cache"})
        response.raise_for_status()
        data = response.json()
        if not data.get("ok", False):
            return data, False, f"NodeMCU/API reports: {data.get('error', 'no valid data')}"
        required = ("sample", "a0", "a1", "bat", "curr", "t2", "t3")
        missing = [key for key in required if key not in data]
        if missing:
            return data, False, "Missing fields: " + ", ".join(missing)
        return data, True, ""
    except requests.RequestException as exc:
        return None, False, f"Cannot reach FastAPI at {api_url}: {exc}"
    except (ValueError, TypeError) as exc:
        return None, False, f"Invalid JSON response: {exc}"


def save_if_new(data):
    """Append one row per changed Arduino sample number to the CSV archive."""
    init_csv()
    sample = int(data["sample"])
    try:
        # Read only the last CSV row to avoid duplicating the same live snapshot.
        with CSV_PATH.open("r", newline="", encoding="utf-8") as f:
            rows = csv.DictReader(f)
            last_sample = None
            for row in rows:
                last_sample = row.get("sample")
        if last_sample is not None and last_sample != "" and int(last_sample) == sample:
            return False
    except (OSError, ValueError, csv.Error):
        pass

    row = {
        "captured_at": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
        "sample": sample,
        "a0": float(data["a0"]),
        "a1": float(data["a1"]),
        "bat": float(data["bat"]),
        "curr": float(data["curr"]),
        "t2": int(data["t2"]),
        "t3": int(data["t3"]),
        "age_ms": int(data.get("age_ms") or 0),
    }
    with CSV_PATH.open("a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_COLUMNS)
        writer.writerow(row)
    return True


def load_history(seconds=60, limit=3000):
    init_csv()
    try:
        df = pd.read_csv(CSV_PATH)
    except (OSError, pd.errors.EmptyDataError, pd.errors.ParserError):
        return pd.DataFrame(columns=CSV_COLUMNS)
    if df.empty:
        return df

    for col in ("sample", "a0", "a1", "bat", "curr", "t2", "t3", "age_ms"):
        df[col] = pd.to_numeric(df[col], errors="coerce")
    # Keep timestamps timezone-aware in UTC. This avoids the previous UTC-vs-IST
    # mismatch that could filter every saved point out of the graph window.
    df["captured_at"] = pd.to_datetime(df["captured_at"], utc=True, errors="coerce")
    df = df.dropna(subset=["captured_at", "a0"]).sort_values("captured_at").tail(int(limit))
    cutoff = pd.Timestamp.now(tz="UTC") - pd.Timedelta(seconds=int(seconds))
    return df[df["captured_at"] >= cutoff].copy()


def load_all_history(limit=10000):
    init_csv()
    try:
        df = pd.read_csv(CSV_PATH)
    except (OSError, pd.errors.EmptyDataError, pd.errors.ParserError):
        return pd.DataFrame(columns=CSV_COLUMNS)
    if df.empty:
        return df
    df["captured_at"] = pd.to_datetime(df["captured_at"], utc=True, errors="coerce")
    for col in ("sample", "a0", "a1", "bat", "curr", "t2", "t3", "age_ms"):
        df[col] = pd.to_numeric(df[col], errors="coerce")
    return df.dropna(subset=["captured_at"]).sort_values("captured_at").tail(int(limit))


def make_a0_chart(df):
    fig = go.Figure()
    if not df.empty:
        fig.add_trace(go.Scattergl(
            x=df["captured_at"], y=df["a0"], name="A0",
            mode="lines", line=dict(width=2, color="#55e6bd"),
            hovertemplate="%{x|%H:%M:%S.%L}<br>A0: %{y:.4f} V<extra></extra>",
        ))
    fig.update_layout(
        height=390, margin=dict(l=12, r=18, t=20, b=12),
        paper_bgcolor="rgba(0,0,0,0)", plot_bgcolor="#0c141e",
        font=dict(color="#b7c7d9", size=12),
        xaxis=dict(title=None, showgrid=True, gridcolor="#1c2a3a", zeroline=False),
        yaxis=dict(title="Voltage (V)", showgrid=True, gridcolor="#1c2a3a", zeroline=False),
        legend=dict(orientation="h", yanchor="bottom", y=1.01, xanchor="right", x=1),
        hovermode="x unified",
    )
    return fig


init_csv()

with st.sidebar:
    st.markdown("## ⚙️ Acquisition")
    api_url = st.text_input("FastAPI telemetry URL", value=st.session_state.get("api_url", DEFAULT_API))
    st.session_state["api_url"] = api_url
    refresh_ms = st.select_slider(
        "Dashboard refresh",
        options=[250, 350, 500, 750, 1000],
        value=st.session_state.get("refresh_ms", 350),
        format_func=lambda x: f"{x} ms",
    )
    st.session_state["refresh_ms"] = refresh_ms
    history_seconds = st.select_slider(
        "Graph time window",
        options=[10, 30, 60, 120, 300, 600],
        value=st.session_state.get("history_seconds", 60),
        format_func=lambda x: f"{x} seconds" if x < 60 else f"{x // 60} min" if x % 60 == 0 else f"{x} seconds",
    )
    st.session_state["history_seconds"] = history_seconds
    st.divider()
    st.markdown("### 💾 History")
    count = history_count()
    st.caption(f"{count:,} saved samples in local CSV history.")
    if st.button("Clear saved history", type="secondary", use_container_width=True):
        with CSV_PATH.open("w", newline="", encoding="utf-8") as f:
            csv.DictWriter(f, fieldnames=CSV_COLUMNS).writeheader()
        st.session_state.pop("last_saved_sample", None)
        st.rerun()
    st.caption("History is stored on this computer in `cerephyte_history.csv`.")

st.markdown("""
<div class="hero">
  <div class="eyebrow">ISLERI · BIOSIGNAL INSTRUMENTATION</div>
  <h1>CerePhyte <span style="color:#71dfbd">/</span> Live Console</h1>
  <p>Rev. 4.5 · Local telemetry · Continuous A0 monitoring with persistent history</p>
</div>
""", unsafe_allow_html=True)

# A fragment refreshes the live section without rerunning the entire dashboard/sidebar.
@st.fragment(run_every=f"{refresh_ms}ms")
def live_panel():
    data, good, error = get_latest_sample()

    if good:
        try:
            save_if_new(data)
        except OSError as exc:
            st.error(f"Could not save history: {exc}")

        st.success("LIVE · FastAPI connected · Newest complete telemetry snapshot")
        c1, c2, c3, c4 = st.columns(4)
        c1.metric("A0 · Signal", f"{float(data['a0']):.4f} V", help="ADS1115 channel A0 value delivered by the NodeMCU API.")
        c2.metric("Battery", f"{float(data['bat']):.2f} V")
        c3.metric("Current", f"{float(data['curr']):.2f} mA")
        c4.metric("Sample counter", f"{int(data['sample']):,}")

        c5, c6, c7 = st.columns(3)
        c5.metric("Touch T2", "HIGH" if int(data["t2"]) else "LOW")
        c6.metric("Touch T3 / LED", "HIGH · LED ON" if int(data["t3"]) else "LOW · LED OFF")
        age = data.get("age_ms")
        c7.metric("Packet age", f"{int(age)} ms" if age is not None else "—")

        df = load_history(history_seconds)
        st.markdown("### Live signal · A0")
        st.caption(f"Showing the last {history_seconds} seconds · {len(df):,} saved points · timestamps normalized to UTC")
        st.plotly_chart(make_a0_chart(df), use_container_width=True, config={
            "displaylogo": False,
            "scrollZoom": True,
            "responsive": True,
            "modeBarButtonsToRemove": ["lasso2d", "select2d"],
        }, key="a0_live_chart")
        with st.expander("Latest JSON payload"):
            st.json(data)
    else:
        st.error("NO LIVE DATA")
        st.warning(error)
        st.info("Check that FastAPI is running, that its telemetry endpoint returns `ok: true`, and that the laptop can reach the NodeMCU.")
        st.code(f"FastAPI URL: {api_url}")

live_panel()

st.divider()
st.markdown("### Recorded history")
all_df = load_all_history(10000)

if all_df.empty:
    st.caption("No samples saved yet. Keep the dashboard open while CerePhyte is running.")
else:
    all_df = all_df.sort_values("captured_at")
    st.caption(f"Local CSV archive contains {len(all_df):,} most recent samples.")
    csv_export = all_df.to_csv(index=False).encode("utf-8")
    st.download_button(
        "⬇️ Export history as CSV",
        data=csv_export,
        file_name=f"cerephyte_history_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv",
        mime="text/csv",
    )
    display_df = all_df.tail(20).iloc[::-1].copy()
    display_df["captured_at"] = display_df["captured_at"].dt.strftime("%Y-%m-%d %H:%M:%S.%f UTC")
    st.dataframe(display_df, use_container_width=True, hide_index=True)

st.caption("CerePhyte is a prototype instrument. Verify calibration and signal integrity before interpreting measurements.")
