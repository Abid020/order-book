#pragma once

#include "book_event.hpp"
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
        : book_(book) {}

    // Build callbacks that push onto the queue
    itch::Callbacks make_callbacks() {
        itch::Callbacks cb;

        cb.on_add_order = [this](const itch::AddOrderMsg& m) {
            while (!queue_.push(BookEvent{m})) {}  // spin if full
        };
        cb.on_order_cancel = [this](const itch::OrderCancelMsg& m) {
            while (!queue_.push(BookEvent{m})) {}
        };
        cb.on_order_delete = [this](const itch::OrderDeleteMsg& m) {
            while (!queue_.push(BookEvent{m})) {}
        };
        cb.on_order_executed = [this](const itch::OrderExecutedMsg& m) {
            while (!queue_.push(BookEvent{m})) {}
        };
        cb.on_order_replace = [this](const itch::OrderReplaceMsg& m) {
            while (!queue_.push(BookEvent{m})) {}
        };

        return cb;
    }

    // Drain the queue and apply events to the book
    void process_all() {
        while (auto event = queue_.pop()) {
            std::visit([this](const auto& msg) {
                apply(msg);
            }, *event);
        }
    }

    // Process a single event if available
    bool process_one() {
        auto event = queue_.pop();
        if (!event) return false;
        std::visit([this](const auto& msg) {
            apply(msg);
        }, *event);
        return true;
    }

    bool empty() const { return queue_.empty(); }

private:
    book::OrderBook&              book_;
    SpscQueue<BookEvent, Capacity> queue_;

    // std::visit dispatches to the right overload
    void apply(const itch::AddOrderMsg&      m) { book_.add_order(m);     }
    void apply(const itch::OrderCancelMsg&   m) { book_.cancel_order(m);  }
    void apply(const itch::OrderDeleteMsg&   m) { book_.delete_order(m);  }
    void apply(const itch::OrderExecutedMsg& m) { book_.execute_order(m); }
    void apply(const itch::OrderReplaceMsg&  m) { book_.replace_order(m); }
};

} // namespace queue