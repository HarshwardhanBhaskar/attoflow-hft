"""AttoFlow Live Market WebSocket Server

Connects to Binance USDT-M Futures live WebSocket stream (wss://fstream.binance.com),
parses real-time depth@0ms, trade, and bookTicker feeds, calculates micro-price & OFI,
and streams normalized live tick events to the AttoFlow terminal dashboard over local WebSocket (ws://127.0.0.1:8765).

Usage:
    python tools/live_server.py [--symbol btcusdt] [--port 8765]
"""
from __future__ import annotations

import argparse
import asyncio
import json
import logging
import sys
import time
from typing import Set

import websockets

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("attoflow_live")

BINANCE_WS_URL = "wss://fstream.binance.com/stream?streams={symbol}@depth@0ms/{symbol}@trade/{symbol}@bookTicker"

# Connected dashboard clients
CLIENTS: Set[websockets.WebSocketServerProtocol] = set()

# Live Market State
STATE = {
    "symbol": "BTCUSDT",
    "bids": {},  # price -> qty
    "asks": {},  # price -> qty
    "best_bid": 0.0,
    "best_ask": 0.0,
    "best_bid_qty": 0.0,
    "best_ask_qty": 0.0,
    "last_trade_price": 0.0,
    "last_trade_qty": 0.0,
    "last_trade_side": "BUY",
    "trades_count": 0,
    "messages_count": 0,
    "start_time": time.time(),
    "prev_bid": 0.0,
    "prev_ask": 0.0,
    "prev_bid_qty": 0.0,
    "prev_ask_qty": 0.0,
    "ofi": 0.0,
}


async def register(websocket):
    CLIENTS.add(websocket)
    logger.info(f"Dashboard client connected. Total clients: {len(CLIENTS)}")
    try:
        await websocket.wait_closed()
    finally:
        CLIENTS.remove(websocket)
        logger.info(f"Dashboard client disconnected. Total clients: {len(CLIENTS)}")


async def broadcast(message: str):
    if not CLIENTS:
        return
    await asyncio.gather(*[client.send(message) for client in CLIENTS], return_exceptions=True)


async def connect_binance(symbol: str):
    url = BINANCE_WS_URL.format(symbol=symbol.lower())
    logger.info(f"Connecting to Binance Futures WebSocket: {url}")

    async for ws in websockets.connect(url, ping_interval=20, ping_timeout=10):
        try:
            logger.info("Connected to Binance Futures live stream successfully!")
            async for raw_msg in ws:
                STATE["messages_count"] += 1
                try:
                    data = json.loads(raw_msg)
                    stream = data.get("stream", "")
                    payload = data.get("data", {})

                    now_ns = int(time.time() * 1e9)

                    if "depth" in stream:
                        # Depth snapshot / update
                        bids = payload.get("b", [])
                        asks = payload.get("a", [])
                        for p, q in bids:
                            price, qty = float(p), float(q)
                            if qty == 0:
                                STATE["bids"].pop(price, None)
                            else:
                                STATE["bids"][price] = qty

                        for p, q in asks:
                            price, qty = float(p), float(q)
                            if qty == 0:
                                STATE["asks"].pop(price, None)
                            else:
                                STATE["asks"][price] = qty

                        # Top levels
                        sorted_bids = sorted(STATE["bids"].items(), reverse=True)[:10]
                        sorted_asks = sorted(STATE["asks"].items())[:10]

                        if sorted_bids:
                            STATE["best_bid"], STATE["best_bid_qty"] = sorted_bids[0]
                        if sorted_asks:
                            STATE["best_ask"], STATE["best_ask_qty"] = sorted_asks[0]

                        # Calculate OFI
                        b_t, q_b_t = STATE["best_bid"], STATE["best_bid_qty"]
                        a_t, q_a_t = STATE["best_ask"], STATE["best_ask_qty"]
                        b_prev, q_b_prev = STATE["prev_bid"], STATE["prev_bid_qty"]
                        a_prev, q_a_prev = STATE["prev_ask"], STATE["prev_ask_qty"]

                        ofi_delta = 0.0
                        if b_t >= b_prev and b_prev > 0:
                            ofi_delta += q_b_t if b_t > b_prev else (q_b_t - q_b_prev)
                        else:
                            ofi_delta -= q_b_prev

                        if a_t <= a_prev and a_prev > 0:
                            ofi_delta -= q_a_t if a_t < a_prev else (q_a_t - q_a_prev)
                        else:
                            ofi_delta += q_a_prev

                        STATE["ofi"] = 0.8 * STATE["ofi"] + 0.2 * ofi_delta
                        STATE["prev_bid"], STATE["prev_bid_qty"] = b_t, q_b_t
                        STATE["prev_ask"], STATE["prev_ask_qty"] = a_t, q_a_t

                        mid_price = (b_t + a_t) / 2.0 if (b_t and a_t) else b_t
                        denom = q_b_t + q_a_t
                        micro_price = (b_t * q_a_t + a_t * q_b_t) / denom if denom > 0 else mid_price

                        frame = {
                            "type": "depth",
                            "symbol": symbol.upper(),
                            "timestamp": now_ns,
                            "bids": sorted_bids,
                            "asks": sorted_asks,
                            "mid_price": round(mid_price, 2),
                            "micro_price": round(micro_price, 2),
                            "best_bid": b_t,
                            "best_ask": a_t,
                            "ofi": round(STATE["ofi"], 3),
                            "msg_rate": STATE["messages_count"],
                        }
                        await broadcast(json.dumps(frame))

                    elif "trade" in stream:
                        STATE["trades_count"] += 1
                        price = float(payload.get("p", 0))
                        qty = float(payload.get("q", 0))
                        is_buyer_maker = payload.get("m", False)
                        side = "SELL" if is_buyer_maker else "BUY"

                        STATE["last_trade_price"] = price
                        STATE["last_trade_qty"] = qty
                        STATE["last_trade_side"] = side

                        trade_frame = {
                            "type": "trade",
                            "symbol": symbol.upper(),
                            "timestamp": now_ns,
                            "price": price,
                            "qty": qty,
                            "side": side,
                            "trades_total": STATE["trades_count"],
                        }
                        await broadcast(json.dumps(trade_frame))

                except Exception as e:
                    logger.error(f"Error parsing Binance message: {e}")

        except websockets.ConnectionClosed as e:
            logger.warning(f"Binance connection closed ({e}), retrying in 2 seconds...")
            await asyncio.sleep(2)


async def main():
    parser = argparse.ArgumentParser(description="AttoFlow Live Market Server")
    parser.add_argument("--symbol", default="btcusdt", help="Trading symbol (e.g. btcusdt)")
    parser.add_argument("--port", type=int, default=8765, help="WebSocket server port")
    args = parser.parse_args()

    symbol = args.symbol.lower()
    port = args.port

    logger.info(f"Starting AttoFlow Live WebSocket Server on ws://127.0.0.1:{port} for {symbol.upper()}...")
    server = await websockets.serve(register, "127.0.0.1", port)

    await asyncio.gather(server.wait_closed(), connect_binance(symbol))


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Live WebSocket server stopped.")
