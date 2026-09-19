/** ATTOFLOW PNL & RISK ANALYTICS PANEL
 *
 * Real-time calculation of:
 * - Realized P&L from filled orders
 * - Unrealized P&L (Mark-to-market at mid price)
 * - Maker Rebate (+0.005%) vs Retail Fee (-0.02%) accounting
 * - Net Total P&L
 * - Retro ASCII Equity Curve sparkline
 */

import { sp } from "../dom";
import { lj, rj, lotDigits } from "../fmt";
import type { Session } from "../session";
import { Panel, type RenderCtx } from "./base";

export class PnlPanel extends Panel {
  private readonly qd: number;
  private equityHistory: number[] = [];

  constructor(s: Session) {
    super("pnl", 5, "P&L & RISK COCKPIT", s);
    this.qd = lotDigits(s.lotSize);
  }

  render(c: RenderCtx): void {
    const s = this.s;
    const f = c.f;
    const evN = s.eventsUpTo(c.t);

    const key = `${f}|${evN}|${this.rows}|${this.cols}`;
    if (!this.changed(key)) return;

    // 1. Calculate fills and cashflow up to current time
    let cashFlow = 0.0;
    let inventory = 0.0;
    let totalBoughtQty = 0.0;
    let totalBoughtCash = 0.0;
    let totalSoldQty = 0.0;
    let totalSoldCash = 0.0;
    let fillCount = 0;

    for (let j = 0; j < s.fillEvents.length; j++) {
      const i = s.fillEvents[j];
      if (i >= evN) break;
      fillCount++;

      const side = s.eSide[i]; // 1 = buy, -1 = sell
      const px = s.eExecTick[i] * s.tickSize;
      const qty = s.eQty[i];

      if (side === 1) {
        cashFlow -= px * qty;
        inventory += qty;
        totalBoughtQty += qty;
        totalBoughtCash += px * qty;
      } else {
        cashFlow += px * qty;
        inventory -= qty;
        totalSoldQty += qty;
        totalSoldCash += px * qty;
      }
    }

    const bb = f < s.bestBidTick.length ? s.bestBidTick[f] : 0;
    const ba = f < s.bestAskTick.length ? s.bestAskTick[f] : 0;
    const currentMid = (bb + ba) * 0.5 * s.tickSize;
    const unrl = inventory * currentMid;
    const grossPnl = cashFlow + unrl;

    // Fees / Rebates
    // Maker rebate assumption: +0.005% (+0.5 bps)
    // Retail maker fee comparison: -0.02% (-2.0 bps)
    const tradedNotional = totalBoughtCash + totalSoldCash;
    const makerRebate = tradedNotional * 0.00005; // 0.005% rebate
    const netPnlWithRebate = grossPnl + makerRebate;
    const retailFee = tradedNotional * 0.00020;   // 0.02% fee
    const netPnlRetail = grossPnl - retailFee;

    // Keep equity sparkline history (last 40 points)
    if (this.equityHistory.length === 0 || f % 20 === 0) {
      this.equityHistory.push(netPnlWithRebate);
      if (this.equityHistory.length > 40) this.equityHistory.shift();
    }

    // Build ASCII sparkline:   ▂ ▃ ▄ ▅ ▆ ▇ █
    const sparkChars = [" ", "▂", "▃", "▄", "▅", "▆", "▇", "█"];
    let sparkline = "";
    if (this.equityHistory.length > 1) {
      const minEq = Math.min(...this.equityHistory);
      const maxEq = Math.max(...this.equityHistory);
      const range = maxEq - minEq;
      for (const val of this.equityHistory) {
        const idx = range > 1e-6 ? Math.min(7, Math.max(0, Math.floor(((val - minEq) / range) * 7))) : 3;
        sparkline += sparkChars[idx];
      }
    }

    const pnlSign = netPnlWithRebate >= 0 ? "+" : "";
    const pnlCls = netPnlWithRebate >= 0 ? "bid" : "ask";
    this.setTitle(`NET ${pnlSign}$${netPnlWithRebate.toFixed(2)}`);

    const out: string[] = [];

    // Header summary lines
    out.push(
      sp("d", "REALIZED CASHFLOW: ") +
      sp("w", rj(`${cashFlow >= 0 ? "+" : ""}$${cashFlow.toFixed(2)}`, 11)) +
      sp("d", "  UNREALIZED MTM: ") +
      sp(unrl >= 0 ? "bid" : "ask", rj(`${unrl >= 0 ? "+" : ""}$${unrl.toFixed(2)}`, 11))
    );

    out.push(
      sp("d", "GROSS TRADING PNL: ") +
      sp(grossPnl >= 0 ? "bid" : "ask", rj(`${grossPnl >= 0 ? "+" : ""}$${grossPnl.toFixed(2)}`, 11)) +
      sp("d", "  INVENTORY (POS):") +
      sp("ours", rj(`${inventory >= 0 ? "+" : ""}${inventory.toFixed(this.qd)} BTC`, 11))
    );

    out.push(sp("d", "───────────────────────────────────────────────────────────────────────"));

    out.push(
      sp("d", "MAKER REBATE (+0.5bps): ") +
      sp("bid", rj(`+$${makerRebate.toFixed(2)}`, 9)) +
      sp("d", " -> ") +
      sp(pnlCls, `NET P&L (REBATE):  ${pnlSign}$${netPnlWithRebate.toFixed(2)}`)
    );

    out.push(
      sp("d", "RETAIL FEE   (-2.0bps): ") +
      sp("ask", rj(`-$${retailFee.toFixed(2)}`, 9)) +
      sp("d", " -> ") +
      sp(netPnlRetail >= 0 ? "bid" : "ask", `NET P&L (RETAIL):  ${netPnlRetail >= 0 ? "+" : ""}$${netPnlRetail.toFixed(2)}`)
    );

    out.push(sp("d", "───────────────────────────────────────────────────────────────────────"));

    // Volume & Trade counts
    out.push(
      sp("d", "FILLS: ") + sp("w", lj(fillCount.toString(), 6)) +
      sp("d", "TRADED NOTIONAL: ") + sp("c", `$${(tradedNotional / 1000).toFixed(1)}k `) +
      sp("d", "BUY/SELL: ") +
      sp("bid", `${totalBoughtQty.toFixed(this.qd)}`) + sp("d", " / ") +
      sp("ask", `${totalSoldQty.toFixed(this.qd)} BTC`)
    );

    out.push("");
    out.push(
      sp("d", "EQUITY CURVE: [") +
      sp(pnlCls, sparkline.padEnd(40, "░")) +
      sp("d", "] ") +
      sp("w", `${pnlSign}$${netPnlWithRebate.toFixed(2)}`)
    );

    this.body.innerHTML = out.join("\n");
  }
}
