#pragma once

#include "m_bookevent.hpp"
#include "spsc_queue.hpp"
#include "book/order_book.hpp"
#include "feed/itch_parser.hpp"

namespace queue {

// ─────────────────────────────────────────────
//  QueueProcessor
//
//  Owns the queue and provides:
//  1. make_callbacks() — returns itch::Callbacks
//     that push events onto the queue.
//     Call this on the parser thread.
//
//  2. process_all() — drains the queue and
//     applies each event to the order book.
//     Call this on the book thread.
// ─────────────────────────────────────────────

template<std::size_t Capacity = 65536>
class QueueProcessor {
public:
    explicit QueueProcessor(book::OrderBook& book)
        : m_book(book) {}

    // Build callbacks that push onto the queue
    itch::Callbacks make_callbacks() {
        itch::Callbacks cb;

        cb.on_add_order = [this](const itch::AddOrderMsg& m) {
            while (!m_queue.push(BookEvent{m})) {}  // spin if full
        };
        cb.on_order_cancel = [this](const itch::OrderCancelMsg& m) {
            while (!m_queue.push(BookEvent{m})) {}
        };
        cb.on_order_delete = [this](const itch::OrderDeleteMsg& m) {
            while (!m_queue.push(BookEvent{m})) {}
        };
        cb.on_order_executed = [this](const itch::OrderExecutedMsg& m) {
            while (!m_queue.push(BookEvent{m})) {}
        };
        cb.on_order_replace = [this](const itch::OrderReplaceMsg& m) {
            while (! .push(BookEvent{m})) {}
        };

        return cb;
    }

    // Drain the queue and apply events to the book
    void process_all() {
        while (auto event = m_queue.pop()) {
            std::visit([this](const auto& msg) {
                apply(msg);
            }, *event);
        }
    }

    // Process a single event if available
    bool process_one() {
        auto event = m_queue.pop();
        if (!event) return false;
        std::visit([this](const auto& msg) {
            apply(msg);
        }, *event);
        return true;
    }

    bool empty() const { return m_queue.empty(); }

private:
    book::OrderBook&              m_book;
    SpscQueue<BookEvent, Capacity> m_queue;

    // std::visit dispatches to the right overload
    void apply(const itch::AddOrderMsg&      m) { m_book.add_order(m);     }
    void apply(const itch::OrderCancelMsg&   m) { m_book.cancel_order(m);  }
    void apply(const itch::OrderDeleteMsg&   m) { m_book.delete_order(m);  }
    void apply(const itch::OrderExecutedMsg& m) { m_book.execute_order(m); }
    void apply(const itch::OrderReplaceMsg&  m) { m_book.replace_order(m); }
};

} // namespace queue