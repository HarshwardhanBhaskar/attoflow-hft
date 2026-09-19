#pragma once

#include <cstdint>
#include <string>

namespace attoflow {

enum class Side : uint8_t {
    BUY = 1,
    SELL = 2
};

enum class OrderType : uint8_t {
    LIMIT = 1,
    MARKET = 2,
    POST_ONLY = 3 // GTX
};

enum class OrderStatus : uint8_t {
    NEW = 1,
    ACCEPTED = 2,
    PARTIALLY_FILLED = 3,
    FILLED = 4,
    CANCELED = 5,
    REJECTED = 6
};

struct alignas(64) Order {
    uint64_t order_id{0};
    uint64_t client_order_id{0};
    double price{0.0};
    double qty{0.0};
    double leaves_qty{0.0};
    double filled_qty{0.0};
    uint64_t timestamp_ns{0};
    Side side{Side::BUY};
    OrderType type{OrderType::LIMIT};
    OrderStatus status{OrderStatus::NEW};
    
    // Linked list pointers for cache-friendly O(1) queue priority
    Order* prev{nullptr};
    Order* next{nullptr};

    [[nodiscard]] inline bool is_filled() const noexcept {
        return leaves_qty <= 1e-8;
    }
};

struct ExecutionReport {
    uint64_t order_id{0};
    uint64_t match_order_id{0};
    double exec_price{0.0};
    double exec_qty{0.0};
    Side side{Side::BUY};
    uint64_t match_timestamp_ns{0};
    bool maker{false};
};

} // namespace attoflow
