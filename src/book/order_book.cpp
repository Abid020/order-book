#include "order_book.hpp"

#include <stdexcept>
#include <algorithm>

namespace book {

void OrderBook::add_order(const itch::AddOrderMsg& m)
{
    Order order{
        m.order_ref,
        m.price,
        m.shares,
        m.side
    };

    orders_[m.order_ref] = order;

    if (m.side == 'B') {
        PriceLevel& level = bids_[m.price];
        level.total_quantity += m.shares;
        level.order_refs.push_back(m.order_ref);
    } else {
        PriceLevel& level = asks_[m.price];
        level.total_quantity += m.shares;
        level.order_refs.push_back(m.order_ref);
    }
}

void OrderBook::cancel_order(const itch::OrderCancelMsg& m) {
    auto it = orders_.find(m.order_ref);
    if (it == orders_.end()) return;
    Order& order       = it->second;
    uint32_t cancelled = std::min(m.cancelled_shares, order.shares);
    order.shares      -= cancelled;
    reduce_level(order, cancelled);
    if (order.shares == 0)
        orders_.erase(it);
}

void OrderBook::delete_order(const itch::OrderDeleteMsg& m)
{
    auto it = orders_.find(m.order_ref);
    if (it == orders_.end()) return;

    remove_from_level(it->second);
    orders_.erase(it);
}

void OrderBook::execute_order(const itch::OrderExecutedMsg& m) {
    auto it = orders_.find(m.order_ref);
    if (it == orders_.end()) return;
    Order& order      = it->second;
    uint32_t executed = std::min(m.executed_shares, order.shares);
    order.shares     -= executed;
    if (order.shares == 0) // can be made slightly more efficient with inline lambdas
        remove_from_level(order);
    else
        reduce_level(order, executed);
    if (order.shares == 0)
        orders_.erase(it);
}

void OrderBook::replace_order(const itch::OrderReplaceMsg& m)
{
    auto it = orders_.find(m.orig_order_ref);
    if (it == orders_.end()) return;

    char side = it->second.side;
    remove_from_level(it->second);
    orders_.erase(it);

    itch::AddOrderMsg add{};
    add.order_ref = m.new_order_ref;
    add.price     = m.price;
    add.shares    = m.shares;
    add.side      = side;
    add_order(add);
}

void OrderBook::remove_from_level(const Order& order) {
    auto remove = [&](auto& levels) {
        auto it = levels.find(order.price);
        if (it == levels.end()) return;
        auto& refs = it->second.order_refs;
        refs.erase(
            std::remove(refs.begin(), refs.end(), order.order_ref),
            refs.end()
        );
        it->second.total_quantity -= order.shares;
        if (it->second.total_quantity == 0)
            levels.erase(it);
    };
    order.side == 'B' ? remove(bids_) : remove(asks_);
}

void OrderBook::reduce_level(const Order& order, uint32_t qty) {
    auto fn = [&](auto& levels) {
        auto it = levels.find(order.price);
        if (it == levels.end()) return;
        it->second.total_quantity -= qty;
        if (it->second.total_quantity == 0)
            levels.erase(it);
    };
    order.side == 'B' ? fn(bids_) : fn(asks_);
}

uint32_t OrderBook::best_bid() const
{
    if (bids_.empty()) return 0;
    return bids_.begin()->first;
}

uint32_t OrderBook::best_ask() const
{
    if (asks_.empty()) return 0;
    return asks_.begin()->first;
}

uint64_t OrderBook::total_bid_quantity() const
{
    uint64_t total = 0;
    for (const auto& [price, level] : bids_)
        total += level.total_quantity;
    return total;
}

uint64_t OrderBook::total_ask_quantity() const
{
    uint64_t total = 0;
    for (const auto& [price, level] : asks_)
        total += level.total_quantity;
    return total;
}

void OrderBook::clear()
{
    bids_.clear();
    asks_.clear();
    orders_.clear();
}

} // namespace book