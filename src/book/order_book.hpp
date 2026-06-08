#pragma once

#include <cstdint>
#include <map>
#include <deque>
#include <unordered_map>
#include <functional>
#include "feed/itch_parser.hpp"

namespace book {

struct Order {
    uint64_t order_ref;
    uint32_t price;
    uint32_t shares;
    char     side;
};

struct PriceLevel {
    uint64_t             total_quantity;
    std::deque<uint64_t> order_refs; // FIFO arrival
};

class OrderBook {
public:
    void add_order    (const itch::AddOrderMsg&     m);
    void cancel_order (const itch::OrderCancelMsg&  m);
    void delete_order (const itch::OrderDeleteMsg&  m);
    void execute_order(const itch::OrderExecutedMsg& m);
    void replace_order(const itch::OrderReplaceMsg& m);

    const std::map<uint32_t, PriceLevel, std::greater<uint32_t>>& bids() const { return bids_; }
    const std::map<uint32_t, PriceLevel>&                         asks() const { return asks_; }

    uint32_t best_bid() const;
    uint32_t best_ask() const;
    uint64_t total_bid_quantity() const;
    uint64_t total_ask_quantity() const;

    std::size_t order_count() const { return orders_.size(); }
    void clear();

private:
    std::map<uint32_t, PriceLevel, std::greater<uint32_t>> bids_;
    std::map<uint32_t, PriceLevel>                         asks_;
    std::unordered_map<uint64_t, Order>                    orders_;

    void remove_from_level(const Order& order);
    void reduce_level(const Order& order, uint32_t qty);
};

} // namespace book
