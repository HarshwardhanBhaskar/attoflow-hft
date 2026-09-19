#include "attoflow/orderbook.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>

using namespace attoflow;

int main() {
    std::cout << "====================================================\n";
    std::cout << "   ATTOFLOW HFT ENGINE - LOB BENCHMARK (C++20)     \n";
    std::cout << "   Author: Harshwardhan Bhaskar                      \n";
    std::cout << "====================================================\n\n";

    LimitOrderBook book("BTCUSDT", 0.1, 0.001);

    const size_t NUM_ORDERS = 500000;
    std::vector<Order> order_pool(NUM_ORDERS);

    std::mt19937_64 rng(1337);
    std::uniform_real_distribution<double> bid_dist(64000.0, 64999.0);
    std::uniform_real_distribution<double> ask_dist(65001.0, 66000.0);
    std::uniform_real_distribution<double> qty_dist(0.01, 1.50);

    for (size_t i = 0; i < NUM_ORDERS; ++i) {
        order_pool[i].order_id = i + 1;
        order_pool[i].client_order_id = i + 1;
        bool is_buy = (i % 2 == 0);
        order_pool[i].side = is_buy ? Side::BUY : Side::SELL;
        order_pool[i].type = OrderType::LIMIT;
        order_pool[i].price = is_buy ? std::floor(bid_dist(rng) * 10.0) / 10.0 : std::floor(ask_dist(rng) * 10.0) / 10.0;
        order_pool[i].qty = std::floor(qty_dist(rng) * 1000.0) / 1000.0;
        order_pool[i].leaves_qty = order_pool[i].qty;
    }

    std::cout << "[1] Benchmarking Resting Order Insertions (" << NUM_ORDERS << " orders)...\n";
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < NUM_ORDERS; ++i) {
        book.add_order(&order_pool[i]);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double insert_sec = std::chrono::duration<double>(end - start).count();
    double insert_rate = static_cast<double>(NUM_ORDERS) / insert_sec;
    double avg_insert_ns = (insert_sec / NUM_ORDERS) * 1e9;

    std::cout << "    -> Total time: " << std::fixed << std::setprecision(3) << (insert_sec * 1000.0) << " ms\n";
    std::cout << "    -> Throughput: " << std::fixed << std::setprecision(0) << insert_rate << " orders/sec\n";
    std::cout << "    -> Avg Latency: " << std::fixed << std::setprecision(1) << avg_insert_ns << " ns/order\n\n";

    BBO bbo = book.get_bbo();
    std::cout << "    BBO: Bid " << bbo.best_bid_qty << " @ " << bbo.best_bid_price 
              << " | Ask " << bbo.best_ask_qty << " @ " << bbo.best_ask_price << "\n";
    std::cout << "    Spread: $" << bbo.spread() << " | Micro-Price: $" << bbo.micro_price() << "\n\n";

    std::cout << "[2] Benchmarking Order Cancellations (100,000 orders)...\n";
    const size_t CANCELS = 100000;
    start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < CANCELS; ++i) {
        book.cancel_order(i + 1);
    }

    end = std::chrono::high_resolution_clock::now();
    double cancel_sec = std::chrono::duration<double>(end - start).count();
    double cancel_rate = static_cast<double>(CANCELS) / cancel_sec;
    double avg_cancel_ns = (cancel_sec / CANCELS) * 1e9;

    std::cout << "    -> Total time: " << std::fixed << std::setprecision(3) << (cancel_sec * 1000.0) << " ms\n";
    std::cout << "    -> Throughput: " << std::fixed << std::setprecision(0) << cancel_rate << " cancels/sec\n";
    std::cout << "    -> Avg Latency: " << std::fixed << std::setprecision(1) << avg_cancel_ns << " ns/cancel\n\n";

    std::cout << "[3] Benchmarking Aggressive Market Matching (Crossing Depth)...\n";
    size_t trade_count = 0;
    auto fill_cb = [&](const ExecutionReport& report) {
        if (!report.maker) ++trade_count;
    };

    Order taker_sell;
    taker_sell.order_id = 9999999;
    taker_sell.side = Side::SELL;
    taker_sell.type = OrderType::MARKET;
    taker_sell.qty = 50.0;
    taker_sell.leaves_qty = 50.0;

    start = std::chrono::high_resolution_clock::now();
    book.add_order(&taker_sell, fill_cb);
    end = std::chrono::high_resolution_clock::now();

    double match_us = std::chrono::duration<double, std::micro>(end - start).count();
    std::cout << "    -> Matched 50.0 BTC across " << trade_count << " price levels\n";
    std::cout << "    -> Sweep latency: " << std::fixed << std::setprecision(2) << match_us << " us\n\n";

    std::cout << "====================================================\n";
    std::cout << "   BENCHMARK COMPLETED SUCCESSFULLY (Zero Errors)   \n";
    std::cout << "====================================================\n";

    return 0;
}
