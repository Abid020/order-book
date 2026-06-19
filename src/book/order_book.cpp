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

    m_orders[m.order_ref] = order;

    if (m.side == 'B') {
        PriceLevel& level = m_bids[m.price];
        level.total_quantity += m.shares;
        level.order_refs.push_back(m.order_ref);
    } else {
        PriceLevel& level = m_asks[m.price];
        level.total_quantity += m.shares;
        level.order_refs.push_back(m.order_ref);
    }
}

void OrderBook::cancel_order(const itch::OrderCancelMsg& m) {
    auto it = m_orders.find(m.order_ref);
    if (it == m_orders.end()) return;
    Order& order       = it->second;
    uint32_t cancelled = std::min(m.cancelled_shares, order.shares);
    order.shares      -= cancelled;
    reduce_level(order, cancelled);
    if (order.shares == 0)
        m_orders.erase(it);
}

void OrderBook::delete_order(const itch::OrderDeleteMsg& m)
{
    auto it = m_orders.find(m.order_ref);
    if (it == m_orders.end()) return;

    remove_from_level(it->second);
    m_orders.erase(it);
}

void OrderBook::execute_order(const itch::OrderExecutedMsg& m) {
    auto it = m_orders.find(m.order_ref);
    if (it == m_orders.end()) return;
    Order& order      = it->second;
    uint32_t executed = std::min(m.executed_shares, order.shares);
    order.shares     -= executed;
    if (order.shares == 0) // can be made slightly more efficient with inline lambdas
        remove_from_level(order);
    else
        reduce_level(order, executed);
    if (order.shares == 0)
        m_orders.erase(it);
}

void OrderBook::replace_order(const itch::OrderReplaceMsg& m)
{
    auto it = m_orders.find(m.orig_order_ref);
    if (it == m_orders.end()) return;

    char side = it->second.side;
    remove_from_level(it->second);
    m_orders.erase(it);

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
    order.side == 'B' ? remove(m_bids) : remove(m_asks);
}

void OrderBook::reduce_level(const Order& order, uint32_t qty) {
    auto fn = [&](auto& levels) {
        auto it = levels.find(order.price);
        if (it == levels.end()) return;
        it->second.total_quantity -= qty;
        if (it->second.total_quantity == 0)
            levels.erase(it);
    };
    order.side == 'B' ? fn(m_bids) : fn(m_asks);
}

uint32_t OrderBook::best_bid() const
{
    if (m_bids.empty()) return 0;
    return m_bids.begin()->first;
}

uint32_t OrderBook::best_ask() const
{
    if (m_asks.empty()) return 0;
    return m_asks.begin()->first;
}

uint64_t OrderBook::total_bid_quantity() const
{
    uint64_t total = 0;
    for (const auto& [price, level] : m_bids)
        total += level.total_quantity;
    return total;
}

uint64_t OrderBook::total_ask_quantity() const
{
    uint64_t total = 0;
    for (const auto& [price, level] : m_asks)
        total += level.total_quantity;
    return total;
}

void OrderBook::clear()
{
    m_bids.clear();
    m_asks.clear();
    m_orders.clear();
}

} // namespace book