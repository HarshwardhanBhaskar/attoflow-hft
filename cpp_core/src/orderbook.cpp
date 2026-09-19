#include "attoflow/orderbook.hpp"
#include <algorithm>

namespace attoflow {

LimitOrderBook::LimitOrderBook(std::string symbol, double tick_size, double lot_size)
    : symbol_(std::move(symbol)), tick_size_(tick_size), lot_size_(lot_size) {}

LimitOrderBook::~LimitOrderBook() = default;

BBO LimitOrderBook::get_bbo() const noexcept {
    BBO bbo;
    if (!bids_.empty()) {
        const auto& [price, level] = *bids_.begin();
        bbo.best_bid_price = price;
        bbo.best_bid_qty = level.total_qty;
    }
    if (!asks_.empty()) {
        const auto& [price, level] = *asks_.begin();
        bbo.best_ask_price = price;
        bbo.best_ask_qty = level.total_qty;
    }
    return bbo;
}

L2Snapshot LimitOrderBook::get_depth(size_t max_levels) const {
    L2Snapshot snap;
    snap.bids.reserve(std::min(max_levels, bids_.size()));
    snap.asks.reserve(std::min(max_levels, asks_.size()));

    size_t count = 0;
    for (const auto& [price, level] : bids_) {
        if (count++ >= max_levels) break;
        snap.bids.push_back({price, level.total_qty, level.order_count});
    }

    count = 0;
    for (const auto& [price, level] : asks_) {
        if (count++ >= max_levels) break;
        snap.asks.push_back({price, level.total_qty, level.order_count});
    }

    return snap;
}

double LimitOrderBook::get_queue_ahead(uint64_t order_id) const {
    auto it = order_lookup_.find(order_id);
    if (it == order_lookup_.end()) return -1.0;

    const Order* target = it->second;
    double ahead = 0.0;
    const Order* curr = nullptr;

    if (target->side == Side::BUY) {
        auto level_it = bids_.find(target->price);
        if (level_it != bids_.end()) curr = level_it->second.head;
    } else {
        auto level_it = asks_.find(target->price);
        if (level_it != asks_.end()) curr = level_it->second.head;
    }

    while (curr && curr != target) {
        ahead += curr->leaves_qty;
        curr = curr->next;
    }

    return ahead;
}

bool LimitOrderBook::add_order(Order* order, ExecutionCallback on_fill) {
    if (!order || order->qty <= 0.0) return false;
    order->leaves_qty = order->qty;
    order->filled_qty = 0.0;

    // Check for Post-Only (GTX) violation
    if (order->type == OrderType::POST_ONLY) {
        if (order->side == Side::BUY && !asks_.empty() && order->price >= asks_.begin()->first) {
            order->status = OrderStatus::REJECTED;
            return false;
        }
        if (order->side == Side::SELL && !bids_.empty() && order->price <= bids_.begin()->first) {
            order->status = OrderStatus::REJECTED;
            return false;
        }
    }

    // Matching against opposite side
    if (order->side == Side::BUY) {
        match_buy(order, on_fill);
    } else {
        match_sell(order, on_fill);
    }

    // If order still has remaining quantity and is a LIMIT or POST_ONLY order, rest on the book
    if (order->leaves_qty > 1e-8 && order->type != OrderType::MARKET) {
        order->status = OrderStatus::ACCEPTED;
        if (order->side == Side::BUY) {
            auto& level = bids_[order->price];
            level.price = order->price;
            level.append(order);
        } else {
            auto& level = asks_[order->price];
            level.price = order->price;
            level.append(order);
        }
        order_lookup_[order->order_id] = order;
    } else if (order->is_filled()) {
        order->status = OrderStatus::FILLED;
    } else {
        order->status = OrderStatus::CANCELED; // Unfilled market order remainder
    }

    return true;
}

void LimitOrderBook::match_buy(Order* taker, ExecutionCallback& on_fill) {
    while (taker->leaves_qty > 1e-8 && !asks_.empty()) {
        auto ask_it = asks_.begin();
        double best_ask = ask_it->first;

        // For limit order, price must cross
        if (taker->type == OrderType::LIMIT && taker->price < best_ask) {
            break;
        }

        PriceLevel& level = ask_it->second;
        Order* maker = level.head;

        while (maker && taker->leaves_qty > 1e-8) {
            double match_qty = std::min(taker->leaves_qty, maker->leaves_qty);
            taker->leaves_qty -= match_qty;
            taker->filled_qty += match_qty;
            maker->leaves_qty -= match_qty;
            maker->filled_qty += match_qty;
            level.total_qty -= match_qty;

            if (on_fill) {
                // Taker execution report
                on_fill(ExecutionReport{
                    taker->order_id, maker->order_id, best_ask, match_qty, Side::BUY, 0, false
                });
                // Maker execution report
                on_fill(ExecutionReport{
                    maker->order_id, taker->order_id, best_ask, match_qty, Side::SELL, 0, true
                });
            }

            Order* next_maker = maker->next;
            if (maker->is_filled()) {
                maker->status = OrderStatus::FILLED;
                level.remove(maker);
                order_lookup_.erase(maker->order_id);
            } else {
                maker->status = OrderStatus::PARTIALLY_FILLED;
            }
            maker = next_maker;
        }

        if (level.order_count == 0 || level.total_qty <= 1e-8) {
            asks_.erase(ask_it);
        }
    }
}

void LimitOrderBook::match_sell(Order* taker, ExecutionCallback& on_fill) {
    while (taker->leaves_qty > 1e-8 && !bids_.empty()) {
        auto bid_it = bids_.begin();
        double best_bid = bid_it->first;

        // For limit order, price must cross
        if (taker->type == OrderType::LIMIT && taker->price > best_bid) {
            break;
        }

        PriceLevel& level = bid_it->second;
        Order* maker = level.head;

        while (maker && taker->leaves_qty > 1e-8) {
            double match_qty = std::min(taker->leaves_qty, maker->leaves_qty);
            taker->leaves_qty -= match_qty;
            taker->filled_qty += match_qty;
            maker->leaves_qty -= match_qty;
            maker->filled_qty += match_qty;
            level.total_qty -= match_qty;

            if (on_fill) {
                // Taker execution report
                on_fill(ExecutionReport{
                    taker->order_id, maker->order_id, best_bid, match_qty, Side::SELL, 0, false
                });
                // Maker execution report
                on_fill(ExecutionReport{
                    maker->order_id, taker->order_id, best_bid, match_qty, Side::BUY, 0, true
                });
            }

            Order* next_maker = maker->next;
            if (maker->is_filled()) {
                maker->status = OrderStatus::FILLED;
                level.remove(maker);
                order_lookup_.erase(maker->order_id);
            } else {
                maker->status = OrderStatus::PARTIALLY_FILLED;
            }
            maker = next_maker;
        }

        if (level.order_count == 0 || level.total_qty <= 1e-8) {
            bids_.erase(bid_it);
        }
    }
}

bool LimitOrderBook::cancel_order(uint64_t order_id) {
    auto it = order_lookup_.find(order_id);
    if (it == order_lookup_.end()) return false;

    Order* order = it->second;
    if (order->side == Side::BUY) {
        auto level_it = bids_.find(order->price);
        if (level_it != bids_.end()) {
            level_it->second.remove(order);
            if (level_it->second.order_count == 0) {
                bids_.erase(level_it);
            }
        }
    } else {
        auto level_it = asks_.find(order->price);
        if (level_it != asks_.end()) {
            level_it->second.remove(order);
            if (level_it->second.order_count == 0) {
                asks_.erase(level_it);
            }
        }
    }

    order->status = OrderStatus::CANCELED;
    order_lookup_.erase(it);
    return true;
}

} // namespace attoflow
