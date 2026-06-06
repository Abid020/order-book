#include <gtest/gtest.h>
#include "book/order_book.hpp"

static itch::AddOrderMsg make_add(uint64_t ref, char side, uint32_t price, uint32_t shares) {
    itch::AddOrderMsg m{};
    m.order_ref = ref;
    m.side      = side;
    m.price     = price;
    m.shares    = shares;
    return m;
}

static itch::OrderDeleteMsg make_delete(uint64_t ref) {
    itch::OrderDeleteMsg m{};
    m.order_ref = ref;
    return m;
}

static itch::OrderCancelMsg make_cancel(uint64_t ref, uint32_t shares) {
    itch::OrderCancelMsg m{};
    m.order_ref        = ref;
    m.cancelled_shares = shares;
    return m;
}

static itch::OrderExecutedMsg make_execute(uint64_t ref, uint32_t shares) {
    itch::OrderExecutedMsg m{};
    m.order_ref       = ref;
    m.executed_shares = shares;
    return m;
}

// ─────────────────────────────────────────────
//  Add order
// ─────────────────────────────────────────────

TEST(OrderBook, AddBidAppearsInBook) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'B', 1000, 100));

    EXPECT_EQ(ob.best_bid(), 1000u);
    EXPECT_EQ(ob.bids().at(1000).total_quantity, 100u);
    EXPECT_EQ(ob.order_count(), 1u);
}

TEST(OrderBook, AddAskAppearsInBook) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'S', 1005, 200));

    EXPECT_EQ(ob.best_ask(), 1005u);
    EXPECT_EQ(ob.asks().at(1005).total_quantity, 200u);
}

TEST(OrderBook, MultipleBidsAtSameLevelAggregated) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'B', 1000, 100));
    ob.add_order(make_add(2, 'B', 1000, 200));

    EXPECT_EQ(ob.bids().at(1000).total_quantity, 300u);
    EXPECT_EQ(ob.bids().at(1000).order_refs.size(), 2u);
    EXPECT_EQ(ob.order_count(), 2u);
}

TEST(OrderBook, BestBidIsHighestPrice) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'B', 990,  100));
    ob.add_order(make_add(2, 'B', 1000, 100));
    ob.add_order(make_add(3, 'B', 995,  100));

    EXPECT_EQ(ob.best_bid(), 1000u);
}

TEST(OrderBook, BestAskIsLowestPrice) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'S', 1010, 100));
    ob.add_order(make_add(2, 'S', 1005, 100));
    ob.add_order(make_add(3, 'S', 1015, 100));

    EXPECT_EQ(ob.best_ask(), 1005u);
}

// ─────────────────────────────────────────────
//  FIFO ordering
// ─────────────────────────────────────────────

TEST(OrderBook, FIFOOrderPreserved) {
    book::OrderBook ob;
    ob.add_order(make_add(10, 'B', 1000, 100));
    ob.add_order(make_add(20, 'B', 1000, 200));
    ob.add_order(make_add(30, 'B', 1000, 300));

    const auto& refs = ob.bids().at(1000).order_refs;
    EXPECT_EQ(refs[0], 10u);
    EXPECT_EQ(refs[1], 20u);
    EXPECT_EQ(refs[2], 30u);
}

// ─────────────────────────────────────────────
//  Delete order
// ─────────────────────────────────────────────

TEST(OrderBook, DeleteOrderRemovesFromBook) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'B', 1000, 100));
    ob.delete_order(make_delete(1));

    EXPECT_TRUE(ob.bids().empty());
    EXPECT_EQ(ob.order_count(), 0u);
}

TEST(OrderBook, DeleteOneOfTwoOrdersAtLevel) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'B', 1000, 100));
    ob.add_order(make_add(2, 'B', 1000, 200));
    ob.delete_order(make_delete(1));

    EXPECT_EQ(ob.bids().at(1000).total_quantity, 200u);
    EXPECT_EQ(ob.bids().at(1000).order_refs.size(), 1u);
    EXPECT_EQ(ob.order_count(), 1u);
}

TEST(OrderBook, DeleteNonExistentOrderIsNoOp) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'B', 1000, 100));
    ob.delete_order(make_delete(999));

    EXPECT_EQ(ob.order_count(), 1u);
}

// ─────────────────────────────────────────────
//  Cancel order
// ─────────────────────────────────────────────

TEST(OrderBook, CancelReducesQuantity) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'B', 1000, 100));
    ob.cancel_order(make_cancel(1, 40));

    EXPECT_EQ(ob.bids().at(1000).total_quantity, 60u);
    EXPECT_EQ(ob.order_count(), 1u);
}

TEST(OrderBook, CancelEntireQuantityRemovesOrder) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'B', 1000, 100));
    ob.cancel_order(make_cancel(1, 100));

    EXPECT_TRUE(ob.bids().empty());
    EXPECT_EQ(ob.order_count(), 0u);
}

// ─────────────────────────────────────────────
//  Execute order
// ─────────────────────────────────────────────

TEST(OrderBook, ExecuteReducesQuantity) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'S', 1005, 100));
    ob.execute_order(make_execute(1, 60));

    EXPECT_EQ(ob.asks().at(1005).total_quantity, 40u);
    EXPECT_EQ(ob.order_count(), 1u);
}

TEST(OrderBook, ExecuteFullQuantityRemovesOrder) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'S', 1005, 100));
    ob.execute_order(make_execute(1, 100));

    EXPECT_TRUE(ob.asks().empty());
    EXPECT_EQ(ob.order_count(), 0u);
}

// ─────────────────────────────────────────────
//  Replace order
// ─────────────────────────────────────────────

TEST(OrderBook, ReplaceOrderUpdatesBookCorrectly) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'B', 1000, 100));

    itch::OrderReplaceMsg r{};
    r.orig_order_ref = 1;
    r.new_order_ref  = 2;
    r.price          = 1005;
    r.shares         = 150;
    ob.replace_order(r);

    EXPECT_TRUE(ob.bids().find(1000) == ob.bids().end());
    EXPECT_EQ(ob.best_bid(), 1005u);
    EXPECT_EQ(ob.bids().at(1005).total_quantity, 150u);
    EXPECT_EQ(ob.order_count(), 1u);
}

// ─────────────────────────────────────────────
//  Spread
// ─────────────────────────────────────────────

TEST(OrderBook, SpreadIsCorrect) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'B', 1000, 100));
    ob.add_order(make_add(2, 'S', 1005, 100));

    EXPECT_EQ(ob.best_ask() - ob.best_bid(), 5u);
}

// ─────────────────────────────────────────────
//  Clear
// ─────────────────────────────────────────────

TEST(OrderBook, ClearEmptiesBook) {
    book::OrderBook ob;
    ob.add_order(make_add(1, 'B', 1000, 100));
    ob.add_order(make_add(2, 'S', 1005, 100));
    ob.clear();

    EXPECT_TRUE(ob.bids().empty());
    EXPECT_TRUE(ob.asks().empty());
    EXPECT_EQ(ob.order_count(), 0u);
}

TEST(OrderBook, Placeholder) { SUCCEED(); }
