import asyncio
from contextlib import asynccontextmanager

import httpx
from fastapi import FastAPI

# Replace this IP if your NodeMCU's address changes.
NODEMCU_URL = "http://192.168.4.1/api/data"

latest_data = {"ok": False, "error": "Waiting for NodeMCU"}
client: httpx.AsyncClient | None = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    global client
    client = httpx.AsyncClient(timeout=1.0)
    task = asyncio.create_task(poll_nodemcu())
    try:
        yield
    finally:
        task.cancel()
        try:
            await task
        except asyncio.CancelledError:
            pass
        await client.aclose()
        client = None


app = FastAPI(title="CerePhyte API", lifespan=lifespan)


async def poll_nodemcu():
    global latest_data
    while True:
        try:
            assert client is not None
            response = await client.get(NODEMCU_URL)
            response.raise_for_status()
            latest_data = response.json()
        except (httpx.HTTPError, ValueError):
            latest_data = {"ok": False, "error": "NodeMCU unavailable"}
        await asyncio.sleep(0.2)


@app.get("/telemetry")
async def telemetry():
    return latest_data
