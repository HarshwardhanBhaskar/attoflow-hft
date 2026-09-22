/** Live WebSocket Connection Manager for AttoFlow Terminal.
 * Connects to the local live market server at ws://127.0.0.1:8765
 */
export interface LiveDepthFrame {
  type: "depth";
  symbol: string;
  timestamp: number;
  bids: [number, number][];
  asks: [number, number][];
  mid_price: number;
  micro_price: number;
  best_bid: number;
  best_ask: number;
  ofi: number;
  msg_rate: number;
}

export interface LiveTradeFrame {
  type: "trade";
  symbol: string;
  timestamp: number;
  price: number;
  qty: number;
  side: "BUY" | "SELL";
  trades_total: number;
}

export type LiveFrame = LiveDepthFrame | LiveTradeFrame;

export class LiveClient {
  private ws: WebSocket | null = null;
  public isConnected = false;

  constructor(
    private readonly url = "ws://127.0.0.1:8765",
    private readonly onFrame: (frame: LiveFrame) => void,
  ) {}

  public connect(): void {
    try {
      this.ws = new WebSocket(this.url);
      this.ws.onopen = () => {
        this.isConnected = true;
        console.log("Connected to AttoFlow Live WebSocket Server");
      };
      this.ws.onmessage = (event) => {
        try {
          const frame: LiveFrame = JSON.parse(event.data);
          this.onFrame(frame);
        } catch (e) {
          console.error("Error parsing live frame:", e);
        }
      };
      this.ws.onclose = () => {
        this.isConnected = false;
        console.log("AttoFlow Live WebSocket disconnected, retrying in 3s...");
        setTimeout(() => this.connect(), 3000);
      };
      this.ws.onerror = (err) => {
        console.error("Live WebSocket error:", err);
      };
    } catch (e) {
      console.error("Failed to connect live websocket:", e);
    }
  }

  public disconnect(): void {
    if (this.ws) {
      this.ws.close();
      this.ws = null;
      this.isConnected = false;
    }
  }
}
