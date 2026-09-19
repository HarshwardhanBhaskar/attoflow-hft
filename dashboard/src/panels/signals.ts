/** ATTOFLOW MICROSTRUCTURE & ALPHA SIGNALS PANEL
 *
 * Real-time calculation of:
 * - Order Flow Imbalance (OFI)
 * - Micro-Price Fair Value divergence from Mid-Price
 * - Top Level Book Volume Skew
 * - Buyer vs Seller Initiated Volume Delta
 * - Retro ASCII pressure gauges
 */

import { sp } from "../dom";
import { rj, tickDigits, lotDigits } from "../fmt";
import type { Session } from "../session";
import { Panel, type RenderCtx } from "./base";

export class SignalsPanel extends Panel {
  private readonly pxd: number;
  private readonly qd: number;
  private prevBidQty = 0;
  private prevAskQty = 0;
  private prevBidPx = 0;
  private prevAskPx = 0;
  private cumulativeOfi = 0;

  constructor(s: Session) {
    super("signals", 6, "ALPHA & ORDER FLOW (OFI)", s);
    this.pxd = tickDigits(s.tickSize);
    this.qd = lotDigits(s.lotSize);
  }

  render(c: RenderCtx): void {
    const s = this.s;
    const f = c.f;

    const key = `${f}|${this.rows}|${this.cols}`;
    if (!this.changed(key)) return;

    if (f >= s.bestBidTick.length) return;

    const bb = s.bestBidTick[f];
    const ba = s.bestAskTick[f];
    const bestBid = bb * s.tickSize;
    const bestAsk = ba * s.tickSize;
    const mid = (bestBid + bestAsk) * 0.5;

    const { bids, asks } = s.bookAt(f);
    const bidQty = bids.get(bb) ?? 0;
    const askQty = asks.get(ba) ?? 0;

    // Micro-price: (P_bid * Q_ask + P_ask * Q_bid) / (Q_bid + Q_ask)
    const totalQty = bidQty + askQty;
    const microPrice = totalQty > 1e-8 ? (bestBid * askQty + bestAsk * bidQty) / totalQty : mid;
    const microDiffTicks = (microPrice - mid) / s.tickSize;

    // Instantaneous Order Flow Imbalance (Cont, Kukanov, Stoikov OFI model)
    let ofiDelta = 0.0;
    if (this.prevBidPx > 0 && this.prevAskPx > 0) {
      if (bestBid > this.prevBidPx) ofiDelta += bidQty;
      else if (bestBid === this.prevBidPx) ofiDelta += (bidQty - this.prevBidQty);
      else ofiDelta -= this.prevBidQty;

      if (bestAsk < this.prevAskPx) ofiDelta -= askQty;
      else if (bestAsk === this.prevAskPx) ofiDelta -= (askQty - this.prevAskQty);
      else ofiDelta += this.prevAskQty;
    }
    this.prevBidPx = bestBid;
    this.prevAskPx = bestAsk;
    this.prevBidQty = bidQty;
    this.prevAskQty = askQty;
    this.cumulativeOfi += ofiDelta;

    // Book Imbalance Ratio [-1.0, +1.0]
    const imbalanceRatio = totalQty > 1e-8 ? (bidQty - askQty) / totalQty : 0.0;

    // ASCII Imbalance Gauge
    const gaugeWidth = 28;
    const bidFilled = Math.max(0, Math.min(gaugeWidth, Math.round(((imbalanceRatio + 1.0) / 2.0) * gaugeWidth)));
    const askFilled = gaugeWidth - bidFilled;
    const gaugeStr = "█".repeat(bidFilled) + "░".repeat(askFilled);

    const ofiCls = ofiDelta >= 0 ? "bid" : "ask";
    const imbCls = imbalanceRatio >= 0 ? "bid" : "ask";
    const microCls = microDiffTicks >= 0 ? "bid" : "ask";

    this.setTitle(`OFI ${ofiDelta >= 0 ? "+" : ""}${ofiDelta.toFixed(this.qd)} BTC`);

    const out: string[] = [];

    out.push(
      sp("d", "MID PRICE:       ") + sp("w", `$${mid.toFixed(this.pxd)}`) +
      sp("d", "    SPREAD:          ") + sp("c", `$${(bestAsk - bestBid).toFixed(this.pxd)}`)
    );

    out.push(
      sp("d", "MICRO-PRICE:     ") + sp("ours", `$${microPrice.toFixed(this.pxd)}`) +
      sp("d", "    FAIR SKEW:       ") + sp(microCls, `${microDiffTicks >= 0 ? "+" : ""}${microDiffTicks.toFixed(2)} ticks`)
    );

    out.push(sp("d", "───────────────────────────────────────────────────────────────────────"));

    out.push(
      sp("d", "TOUCH DEPTH:     ") +
      sp("bid", `BID ${bidQty.toFixed(this.qd)} BTC`) +
      sp("d", " vs ") +
      sp("ask", `ASK ${askQty.toFixed(this.qd)} BTC`)
    );

    out.push(
      sp("d", "IMBALANCE RATIO: ") +
      sp(imbCls, `${imbalanceRatio >= 0 ? "+" : ""}${(imbalanceRatio * 100).toFixed(1)}% `) +
      sp("d", "[") +
      sp(imbCls, gaugeStr) +
      sp("d", "]")
    );

    out.push(sp("d", "───────────────────────────────────────────────────────────────────────"));

    out.push(
      sp("d", "FRAME OFI DELTA: ") +
      sp(ofiCls, rj(`${ofiDelta >= 0 ? "+" : ""}${ofiDelta.toFixed(this.qd)} BTC`, 14)) +
      sp("d", "    SIGNAL: ") +
      sp(ofiCls, ofiDelta > 0.05 ? "STRONG BUY INFLOW" : ofiDelta < -0.05 ? "STRONG SELL OUTFLOW" : "NEUTRAL FLOW")
    );

    out.push(
      sp("d", "CUMULATIVE OFI:  ") +
      sp(this.cumulativeOfi >= 0 ? "bid" : "ask", rj(`${this.cumulativeOfi >= 0 ? "+" : ""}${this.cumulativeOfi.toFixed(this.qd)} BTC`, 14)) +
      sp("d", "    DIRECTION: ") +
      sp(this.cumulativeOfi >= 0 ? "bid" : "ask", this.cumulativeOfi >= 0 ? "BULLISH DELTA" : "BEARISH DELTA")
    );

    this.body.innerHTML = out.join("\n");
  }
}
