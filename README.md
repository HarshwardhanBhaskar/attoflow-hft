# AttoFlow — High-Frequency Execution Engine & Microstructure Replay Terminal

<p align="center">
  <a href="https://attoflow-hft.vercel.app/?session=sample&play=1">
    <img src="https://img.shields.io/badge/🚀_Live_Demo-attoflow--hft.vercel.app-blueviolet?style=for-the-badge" alt="Live Demo"/>
  </a>
</p>

<p align="center">
  <a href="cpp_core/"><img src="https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=c%2B%2B" alt="C++20"/></a>
  <a href="runner/"><img src="https://img.shields.io/badge/Rust-1.80%2B-black?style=for-the-badge&logo=rust" alt="Rust"/></a>
  <a href="dashboard/"><img src="https://img.shields.io/badge/TypeScript-5.6-3178C6?style=for-the-badge&logo=typescript" alt="TypeScript"/></a>
  <a href="tools/live_server.py"><img src="https://img.shields.io/badge/WebSocket-Live_Streaming-success?style=for-the-badge&logo=websocket" alt="WebSocket"/></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-green.svg?style=for-the-badge" alt="License"/></a>
  <a href="cpp_core/"><img src="https://img.shields.io/badge/Latency-Sub--500ns-critical?style=for-the-badge" alt="Latency"/></a>
</p>

---

## 🧠 What Is AttoFlow?

**In simple terms**: AttoFlow is a complete platform that simulates how real stock/crypto exchanges work at the fastest possible speed — processing **millions of market events per second** — and displays everything in a retro Bloomberg-style terminal that looks straight out of a trading desk.

**In technical terms**: AttoFlow is a deterministic, event-driven **High-Frequency Trading (HFT)** simulation and live market analysis platform. It reconstructs real exchange dynamics at sub-microsecond precision: order queue priority (who gets filled first), network packet delays, order rejections, and tick-by-tick orderbook depth.

### Why does this matter?

In high-frequency trading, your order doesn't just "execute" — it sits in a **queue** behind thousands of other orders. If 500 BTC of resting orders are ahead of you at the best bid price, you don't get filled until the market trades through **all 500 BTC** first. Regular trading simulators completely ignore this. AttoFlow models it accurately.

---

## 📸 Terminal Cockpit — Live Demo

> **👉 [Click here to try the live demo →](https://attoflow-hft.vercel.app/?session=sample&play=1)**

![AttoFlow High-Frequency Execution Terminal](docs/assets/dashboard_terminal.png)

> **What you're seeing**: A real BTCUSDT Binance Futures session replayed tick-by-tick at microsecond resolution. The terminal renders 11 synchronized panels: L2 orderbook depth ladder, queue position tracking, round-trip latency sparklines, P&L risk accounting, Order Flow Imbalance (OFI) alpha signals, and live trade execution feeds — all running at >6 million events/second.

---

## ⚡ Key Capabilities at a Glance

| Component | What It Does | Performance |
| :--- | :--- | :--- |
| **C++20 LOB Core** | Zero-allocation Limit Order Book matching engine with cache-aligned data structures | **<480 ns** per order insert, **2.08M orders/sec** |
| **Rust Event Engine** | Replays binary `.hbr` session recordings with realistic queue & latency simulation | **6.14M events/sec** throughput |
| **Live WebSocket Stream** | Connects directly to Binance Futures live feeds (`wss://fstream.binance.com`) | Real-time depth, trades, OFI |
| **Queue Position Model** | Tracks exact position ahead of your order using probabilistic Price-Time priority | `PowerProbQueueModel3` |
| **Order Flow Alpha (OFI)** | Calculates micro-price fair value and order flow imbalance signals | Cont-Kukanov-Stoikov model |
| **Terminal UI** | Bloomberg-style 11-panel retro IBM VGA cockpit (Vite + TypeScript) | Pure CSS grid, zero canvas |

---

## 🏗️ System Architecture

```text
┌──────────────────────────────────────────────────────────────────────────────────┐
│                          MARKET DATA SOURCES                                     │
│                                                                                  │
│  [Binance USDT-M Futures]                    [Live WebSocket Server]             │
│        │                                            │                            │
│        ├── tools/collect.py (record to .gz)          ├── tools/live_server.py     │
│        └── tools/prepare.py (normalize to .npz)      └── ws://127.0.0.1:8765     │
└────────────────────────────────────────┬─────────────────────────────────────────┘
                                         │
                                         ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│                      HIGH-PERFORMANCE RUNNER (RUST)                              │
│                                                                                  │
│  • Binary .hbr Decoder & Event Dispatcher (6.14M events/sec)                     │
│  • Strategy Engine: Grid Market Maker (Avellaneda-Stoikov Inventory Skew)        │
│  • Realistic FIFO Queue Depth Tracker & Flight Latency Emulator                  │
└────────────────────────────────────────┬─────────────────────────────────────────┘
                                         │
                                         ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│                      MATCHING ENGINE CORE (C++20)                                │
│                                                                                  │
│  • Zero-Allocation Limit Order Book — alignas(64) cache-line aligned             │
│  • O(1) Order Insertion (<480ns) & Cancellation (<280ns)                         │
│  • Crossing Depth Sweeper: 50 BTC across 59 levels in 80μs                      │
└────────────────────────────────────────┬─────────────────────────────────────────┘
                                         │
                                         ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│                   REPLAY TERMINAL COCKPIT (VITE + TYPESCRIPT)                    │
│                                                                                  │
│  • 11 Synchronized Real-Time Panels (Orderbook, Trades, P&L, OFI, Latency...)   │
│  • IBM VGA 9x16 Bitmap Font, CGA/EGA 16-Color Palette                           │
│  • Modes: Session Replay (.hbr)  |  Live WebSocket Streaming                    │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 🎯 The Problem AttoFlow Solves

Standard trading simulators test strategies using 1-minute candle bars. In high-frequency market making, this creates **catastrophic simulation bias**:

| Problem | What Happens in Reality | How AttoFlow Handles It |
| :--- | :--- | :--- |
| **Queue Priority** | Your limit order sits behind resting volume. You don't get filled until the market trades through every unit ahead of you. | Models exact FIFO queue position with `PowerProbQueueModel3`. |
| **Flight Latency** | Your order takes 2–200ms to reach the exchange. The market can move against you during transit. | Simulates realistic packet flight times and post-only (GTX) rejections. |
| **Adverse Selection** | When you DO get filled, it's often because the market is moving against you. | Tracks fill quality, time-at-touch, and inventory risk exposure. |
| **Fee Microstructure** | BTC spread is ~$0.10 (0.013 bps). Profit depends on maker rebate (+0.5bps) vs taker fee (-2.0bps). | Full maker/taker fee accounting in the P&L Risk Cockpit. |

---

## 🚀 Benchmark Results

Compiled with `-O3 -std=c++20` on x86-64:

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

## 📊 Terminal Cockpit Panels

| # | Panel | What It Shows |
| :---: | :--- | :--- |
| **1** | **Level-2 Book Ladder** | Full orderbook depth with resting bids/asks, active queue markers, and real-time trade flash overlays. |
| **2** | **Queue Position** | Volume ahead of each active order, estimated fill probability, and cancellation tracking. |
| **3** | **Latency Monitor** | Sparkline graph of feed packet latency and order round-trip time (RTT) in milliseconds. |
| **4** | **Executions** | Complete fill tape with timestamp, side, price, and time-at-touch duration before fill. |
| **5** | **P&L & Risk Cockpit** | Real-time cashflow, mark-to-market P&L, maker rebate vs taker fee ledger, and ASCII equity curve (`▂▃▅▆▇█`). |
| **6** | **Alpha & OFI** | Order Flow Imbalance signal, micro-price divergence, and volume ratio pressure gauges (`[████████░░░░]`). |
| **7** | **Market** | 60-second rolling mid-price chart and cumulative inventory position graph. |
| **8** | **Trades** | Microsecond public trade feed with buyer/seller initiation color-coding. |
| **9** | **Engine** | Replay telemetry: processing rate (>6.14M events/sec), dataset info, time range. |
| **0** | **Collector** | WebSocket stream health, message throughput histogram, and packet breakdown stats. |

---

## 📐 Mathematical Foundations

### 1. Book Pressure — Micro-Price Fair Value
Instead of a simple mid-price, AttoFlow calculates the volume-weighted micro-price that accounts for orderbook imbalance:

$$P_{\text{micro}} = \frac{P_{\text{bid}} \cdot Q_{\text{ask}} + P_{\text{ask}} \cdot Q_{\text{bid}}}{Q_{\text{bid}} + Q_{\text{ask}}}$$

### 2. Order Flow Imbalance (OFI)
Captures net volume shift across microsecond tick intervals — a leading indicator of short-term price direction:

$$\text{OFI}_t = I_{\{P_{b,t} \ge P_{b,t-1}\}} Q_{b,t} - I_{\{P_{b,t} \le P_{b,t-1}\}} Q_{b,t-1} - I_{\{P_{a,t} \le P_{a,t-1}\}} Q_{a,t} + I_{\{P_{a,t} \ge P_{a,t-1}\}} Q_{a,t-1}$$

### 3. Inventory Skew — Avellaneda-Stoikov Variant
Dynamically adjusts bid/ask quotes based on accumulated inventory to prevent catastrophic directional drawdowns:

$$\delta_{\text{skew}} = -\gamma \cdot q$$

where $\gamma$ is the risk aversion parameter and $q$ is current inventory.

---

## 🛠️ Quick Start Guide

### 1. Compile & Run C++20 Core Benchmark
```bash
cd cpp_core
g++ -std=c++20 -O3 -Iinclude src/orderbook.cpp benchmark/benchmark_lob.cpp -o benchmark_lob
./benchmark_lob
```

### 2. Run Rust Event Runner
```bash
cd runner
cargo run --release
```

### 3. Launch Replay Terminal UI
```bash
cd dashboard
npm install
npm run dev
```
Open **[http://127.0.0.1:5180/?session=sample](http://127.0.0.1:5180/?session=sample)** in your browser.

---

## 📡 Live Market WebSocket Streaming

AttoFlow supports **real-time live streaming** directly from Binance USDT-M Futures via WebSocket (`wss://fstream.binance.com`). No API key required for public market data.

```bash
# Start the live market relay server
python tools/live_server.py --symbol btcusdt --port 8765
```

Open **[http://127.0.0.1:5180/?mode=live](http://127.0.0.1:5180/?mode=live)** to view real-time orderbook depth, live trade executions, micro-price movements, and Order Flow Imbalance (OFI) signals streaming directly from the exchange.

---

## ⌨️ Terminal Navigation

| Key | Action |
| :---: | :--- |
| `SPACE` | Play / Pause |
| `←` `→` | Seek ±5 seconds (`Shift` for ±30s) |
| `↑` `↓` | Double / half playback speed |
| `1`–`9` | Fullscreen focus on panel |
| `0` | Return to full cockpit grid |
| `L` | Toggle Landscape / Portrait layout |
| `B` | Toggle orderbook ladder mode |
| `R` | Restart replay |

---

## 📁 Repository Structure

```
attoflow-hft/
├── cpp_core/              # C++20 zero-allocation Limit Order Book & matching engine
│   ├── include/           # Cache-aligned Order, PriceLevel, BBO, LOB headers
│   ├── src/               # Matching engine (Limit, Market, Post-Only, Cancel)
│   └── benchmark/         # Performance benchmark harness
├── runner/                # Rust high-throughput event-driven strategy runner
│   └── src/               # Market-making strategy, queue tracker, .hbr recorder
├── dashboard/             # Retro IBM VGA terminal UI (Vite + TypeScript)
│   └── src/panels/        # 11 panel modules (book, queue, latency, pnl, ofi...)
├── tools/                 # Python data pipeline & live streaming
│   ├── collect.py         # Binance WebSocket feed collector
│   ├── prepare.py         # Raw stream → normalized tick array converter
│   ├── live_server.py     # Live WebSocket relay server (Binance → Dashboard)
│   └── hbr.py             # Binary recording parser & microstructure analyzer
├── data/                  # Session recordings (.hbr) & feed telemetry
└── vercel.json            # Deployment configuration
```

---

## 👨‍💻 Author & License

Designed & Developed by **Harshwardhan Bhaskar**

[![GitHub](https://img.shields.io/badge/GitHub-HarshwardhanBhaskar-181717?style=flat-square&logo=github)](https://github.com/HarshwardhanBhaskar)
[![Live Demo](https://img.shields.io/badge/Live_Demo-attoflow--hft.vercel.app-blueviolet?style=flat-square)](https://attoflow-hft.vercel.app)

Licensed under the **MIT License** — see [LICENSE](LICENSE) for details.
