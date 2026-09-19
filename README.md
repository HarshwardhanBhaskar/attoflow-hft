# AttoFlow — High-Frequency Trading & Microstructure Replay Engine

![Language](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat&logo=c%2B%2B)
![Rust](https://img.shields.io/badge/Rust-1.80%2B-black?style=flat&logo=rust)
![TypeScript](https://img.shields.io/badge/TypeScript-5.6-3178C6?style=flat&logo=typescript)
![License](https://img.shields.io/badge/License-MIT-green.svg)
![Status](https://img.shields.io/badge/Execution-Sub--Microsecond-critical)

**AttoFlow** is a deterministic, event-driven High-Frequency Trading (HFT) simulation platform and microstructure replay terminal. It models real-world exchange matching dynamics at nanosecond precision: Price-Time (FIFO) queue priority, flight latency, order cancellations, post-only (GTX) order rejections, and continuous market trades.

The platform combines a **zero-allocation C++20 Limit Order Book matching core**, a **high-throughput Rust simulation runner** capable of processing over **6,000,000 events/sec**, and a **Bloomberg-style IBM VGA text-mode execution terminal** featuring real-time P&L tracking and Order Flow Imbalance (OFI) alpha signals.

---

## Architecture Overview

```
attoflow-hft/
├── cpp_core/         # C++20 zero-allocation Limit Order Book & matching engine
│   ├── include/      # Cache-aligned Order, PriceLevel, BBO, and LOB definitions
│   ├── src/          # Matching engine (Limit, Market, Post-Only, Cancels)
│   └── benchmark/    # Benchmark harness (>2.0M orders/sec, sub-500ns latency)
├── runner/           # High-throughput Rust event-driven strategy runner
│   └── src/          # Market-making strategy, queue position tracker, .hbr recorder
├── dashboard/        # Retro IBM VGA text-mode financial replay terminal (Vite + TS)
│   └── src/panels/   # LOB ladder, queue ahead, latency, fills, P&L, OFI, trades
├── tools/            # Python data ingestion & telemetry pipeline
│   ├── collect.py    # Binance USDT-M Futures WebSocket collector
│   ├── prepare.py    # Raw stream converter to normalized tick arrays
│   └── hbr.py        # Binary recording parser & microstructure analyzer
└── data/             # High-frequency session recordings (.hbr) & feed telemetry
```

---

## Why Conventional Backtesters Fail in HFT

Standard backtesters evaluate strategies using 1-second or 1-minute OHLCV bars. In high-frequency market making, this leads to catastrophic simulation bias:

1. **Queue Priority (FIFO / Price-Time)**: Placing an order at the touch (best bid) does not guarantee a fill when a market trade hits that price. Your order sits behind resting volume; the market must trade through every single unit ahead of you before your order receives execution. AttoFlow models this dynamically using probabilistic queue models (`PowerProbQueueModel3`).
2. **Flight Latency & Adverse Selection**: Packet transmission across the WAN introduces $2\text{--}200\text{ ms}$ of feed latency and order round-trip time. If the market moves while your limit order is in flight, **post-only orders (GTX) get rejected**.
3. **Execution Economics**: BTC quotes at the touch operate on a tight 1-tick spread ($0.013\text{ bps}$). Success requires accounting for maker rebates ($+0.005\%$) versus retail taker fees ($-0.02\%$).

---

## 1. C++20 Core Matching Engine (`cpp_core/`)

A standalone, ultra-low latency C++20 Limit Order Book engine designed for predictable, deterministic execution.

### Key Performance Highlights:
* **Zero Heap Allocations in Hot Path**: Order structures are cache-line aligned (`alignas(64)`) to minimize cache misses.
* **Price-Time Priority (FIFO)**: Doubly linked price level buckets provide $O(1)$ order insertion and cancellation.
* **Crossing Depth Sweep**: Sweeps across multiple liquidity levels in microseconds.

### Benchmark Results (Compiled with `-O3 -std=c++20`):
```text
====================================================
   ATTOFLOW HFT ENGINE - LOB BENCHMARK (C++20)     
   Author: Harshwardhan Bhaskar                      
====================================================

[1] Benchmarking Resting Order Insertions (500,000 orders)...
    -> Total time:   240.1 ms
    -> Throughput:   2,082,421 orders/sec
    -> Avg Latency:  480.2 ns / order

[2] Benchmarking Order Cancellations (100,000 orders)...
    -> Total time:   27.8 ms
    -> Throughput:   3,597,329 cancels/sec
    -> Avg Latency:  278.0 ns / cancel

[3] Benchmarking Aggressive Market Matching (Crossing Depth)...
    -> Matched 50.0 BTC across 59 price levels in 80.40 us
```

---

## 2. Replay Terminal Cockpit (`dashboard/`)

The terminal renders a retro IBM VGA 9x16 bitmap console with 10 synchronized execution panels:

1. **`BOOK` [1]**: 46-column Level-2 order book ladder displaying resting bids, asks, active queue positions, and flashing trade hits.
2. **`QUEUE POSITION` [2]**: Estimated queue depth ahead of each active resting order, probability of fill, and cancellation status.
3. **`LATENCY` [3]**: Real-time rolling chart of feed latency and order entry/response round-trip times.
4. **`EXECUTIONS` [4]**: Complete fill tape with time-at-touch before fill and rested duration.
5. **`P&L & RISK COCKPIT` [5]**: Real-time cashflow accounting, mark-to-market unrealized P&L, maker rebate vs. retail fee ledger, and a dynamic **ASCII equity curve** (` ▂▃▅▆▇█`).
6. **`ALPHA & ORDER FLOW (OFI)` [6]**: Cont-Kukanov-Stoikov Order Flow Imbalance, micro-price divergence, and retro volume ratio pressure gauges (`[████████░░░░]`).
7. **`MARKET` [7]**: 60-second rolling mid-price chart and cumulative inventory position graph.
8. **`TRADES` [8]**: Tick-by-tick public trade flow (buyer/seller initiated).
9. **`ENGINE` [9]**: Simulation telemetry processing over **$6.14\text{ million}$ events per wall-clock second**.
10. **`COLLECTOR` [0]**: WebSocket stream health, message throughput, and packet jitter warnings.

---

## Mathematical Foundations

### 1. Book Pressure (Micro-Price Fair Value)
Rather than simple mid-price, the engine computes the instantaneous order book pressure:
$$P_{\text{micro}} = \frac{P_{\text{bid}} \cdot Q_{\text{ask}} + P_{\text{ask}} \cdot Q_{\text{bid}}}{Q_{\text{bid}} + Q_{\text{ask}}}$$

### 2. Order Flow Imbalance (OFI)
Order Flow Imbalance captures microsecond net volume shift across tick intervals:
$$\text{OFI}_t = I_{\{P_{b,t} \ge P_{b,t-1}\}} Q_{b,t} - I_{\{P_{b,t} \le P_{b,t-1}\}} Q_{b,t-1} - I_{\{P_{a,t} \le P_{a,t-1}\}} Q_{a,t} + I_{\{P_{a,t} \ge P_{a,t-1}\}} Q_{a,t-1}$$

### 3. Inventory Skew (Avellaneda-Stoikov variant)
Quotes are continuously adjusted based on current accumulated inventory $q$:
$$\delta_{\text{skew}} = -\gamma \cdot q$$
Biasing quotes to offload inventory and prevent catastrophic directional drawdowns.

---

## Getting Started

### 1. Build & Run C++20 Core Benchmark
```bash
cd cpp_core
g++ -std=c++20 -O3 -Iinclude src/orderbook.cpp benchmark/benchmark_lob.cpp -o benchmark_lob
./benchmark_lob
```

### 2. Launch the Replay Terminal
```bash
cd dashboard
npm install
npm run dev
```
Open **`http://127.0.0.1:5180/?session=sample`** in your browser.

### Terminal Hotkeys:
* `SPACE`: Play / Pause simulation replay
* `←` / `→`: Seek backward / forward 5 seconds (`Shift + Arrow` for 30s)
* `↑` / `↓`: Double / half playback speed
* `1` – `9`: Fullscreen focus on any individual panel
* `0`: Return to 10-panel cockpit grid
* `L`: Toggle between Landscape (desktop) and Portrait (mobile 9:16)
* `B`: Toggle order book ladder mode (active levels vs every tick)
* `R`: Restart replay

---

## License & Author

Developed by **Harshwardhan Bhaskar**  
GitHub: [@HarshwardhanBhaskar](https://github.com/HarshwardhanBhaskar/attoflow-hft)  
Distributed under the MIT License.
