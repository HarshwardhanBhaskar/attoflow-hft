#pragma once

#include "order.hpp"
#include <map>
#include <unordered_map>
#include <vector>
#include <functional>
#include <chrono>
#include <optional>
#include <iostream>

namespace attoflow {

struct PriceLevel {
    double price{0.0};
    double total_qty{0.0};
    uint32_t order_count{0};
    Order* head{nullptr};
    Order* tail{nullptr};

    inline void append(Order* order) noexcept {
        order->prev = tail;
        order->next = nullptr;
        if (tail) {
            tail->next = order;
        } else {
            head = order;
        }
        tail = order;
        total_qty += order->leaves_qty;
        ++order_count;
    }

    inline void remove(Order* order) noexcept {
        if (order->prev) {
            order->prev->next = order->next;
        } else {
            head = order->next;
        }
        if (order->next) {
            order->next->prev = order->prev;
        } else {
            tail = order->prev;
        }
        total_qty -= order->leaves_qty;
        if (order_count > 0) --order_count;
        order->prev = nullptr;
        order->next = nullptr;
    }
};

struct BBO {
    double best_bid_price{0.0};
    double best_bid_qty{0.0};
    double best_ask_price{0.0};
    double best_ask_qty{0.0};

    [[nodiscard]] inline double mid_price() const noexcept {
        return (best_bid_price + best_ask_price) * 0.5;
    }

    [[nodiscard]] inline double spread() const noexcept {
        return best_ask_price - best_bid_price;
    }

    /// Book pressure / Micro-price fair value:
    /// P_fair = (P_bid * Q_ask + P_ask * Q_bid) / (Q_bid + Q_ask)
    [[nodiscard]] inline double micro_price() const noexcept {
        double total_q = best_bid_qty + best_ask_qty;
        if (total_q <= 1e-8) return mid_price();
        return (best_bid_price * best_ask_qty + best_ask_price * best_bid_qty) / total_q;
    }

    /// Order Book Imbalance ratio: (Q_bid - Q_ask) / (Q_bid + Q_ask) [-1.0, +1.0]
    [[nodiscard]] inline double imbalance() const noexcept {
        double total_q = best_bid_qty + best_ask_qty;
        if (total_q <= 1e-8) return 0.0;
        return (best_bid_qty - best_ask_qty) / total_q;
    }
};

struct DepthLevel {
    double price{0.0};
    double qty{0.0};
    uint32_t orders{0};
};

struct L2Snapshot {
    uint64_t timestamp_ns{0};
    std::vector<DepthLevel> bids;
    std::vector<DepthLevel> asks;
};

using ExecutionCallback = std::function<void(const ExecutionReport&)>;

class LimitOrderBook {
public:
    explicit LimitOrderBook(std::string symbol, double tick_size = 0.1, double lot_size = 0.001);
    ~LimitOrderBook();

    // Prevent copies for performance
    LimitOrderBook(const LimitOrderBook&) = delete;
    LimitOrderBook& operator=(const LimitOrderBook&) = delete;

    /// Add an order to the book and execute any crossing liquidity
    bool add_order(Order* order, ExecutionCallback on_fill = nullptr);

    /// Cancel an active resting order by ID
    bool cancel_order(uint64_t order_id);

    /// Get current Best Bid and Offer (BBO)
    [[nodiscard]] BBO get_bbo() const noexcept;

    /// Get L2 snapshot up to N depth levels
    [[nodiscard]] L2Snapshot get_depth(size_t max_levels = 20) const;

    /// Estimate queue volume ahead of a resting order
    [[nodiscard]] double get_queue_ahead(uint64_t order_id) const;

    /// Total active orders in the book
    [[nodiscard]] size_t order_count() const noexcept { return order_lookup_.size(); }

    [[nodiscard]] const std::string& symbol() const noexcept { return symbol_; }

private:
    void match_buy(Order* taker, ExecutionCallback& on_fill);
    void match_sell(Order* taker, ExecutionCallback& on_fill);

    std::string symbol_;
    double tick_size_;
    double lot_size_;

    // Bids descending (highest price first), Asks ascending (lowest price first)
    std::map<double, PriceLevel, std::greater<double>> bids_;
    std::map<double, PriceLevel, std::less<double>> asks_;

    // O(1) order lookup
    std::unordered_map<uint64_t, Order*> order_lookup_;
};

} // namespace attoflow
