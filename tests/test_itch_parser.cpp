#include <gtest/gtest.h>
#include "feed/itch_parser.hpp"
#include <cstring>
#include <arpa/inet.h>

// ─────────────────────────────────────────────
//  Helper — builds a raw ITCH message buffer
//  from host byte order values, ready to feed
//  into the parser
// ─────────────────────────────────────────────

static void write_u16(uint8_t* p, uint16_t v) {
    v = htons(v);
    std::memcpy(p, &v, 2);
}
static void write_u32(uint8_t* p, uint32_t v) {
    v = htonl(v);
    std::memcpy(p, &v, 4);
}
static void write_u64(uint8_t* p, uint64_t v) {
    uint32_t hi = htonl(static_cast<uint32_t>(v >> 32));
    uint32_t lo = htonl(static_cast<uint32_t>(v & 0xFFFFFFFF));
    std::memcpy(p,     &hi, 4);
    std::memcpy(p + 4, &lo, 4);
}
static void write_u48(uint8_t* p, uint64_t v) {
    p[0] = (v >> 40) & 0xFF;
    p[1] = (v >> 32) & 0xFF;
    p[2] = (v >> 24) & 0xFF;
    p[3] = (v >> 16) & 0xFF;
    p[4] = (v >>  8) & 0xFF;
    p[5] =  v        & 0xFF;
}

// ─────────────────────────────────────────────
//  SystemEvent
// ─────────────────────────────────────────────

TEST(ITCHParser, SystemEvent) {
    // Message layout:
    //   [0-1]  length = 11
    //   [2]    type   = 'S'
    //   [3-4]  stock_locate
    //   [5-6]  tracking_number
    //   [7-14] timestamp (8 bytes — we use 8 for simplicity)
    //   [15]   event_code

    uint8_t buf[16] = {};
    write_u16(buf,      11);      // length
    buf[2] = 'S';                 // type
    write_u16(buf + 3,  7);       // stock_locate
    write_u16(buf + 5,  42);      // tracking_number
    write_u48(buf + 7,  999999);  // timestamp_ns
    buf[15] = 'O';                // event_code — market open

    // Extend buffer to 16 bytes
    uint8_t full[16] = {};
    std::memcpy(full, buf, sizeof(buf));
    full[15] = 'O';

    itch::Callbacks cb;
    bool called = false;
    cb.on_system_event = [&](const itch::SystemEventMsg& m) {
        called = true;
        EXPECT_EQ(m.stock_locate,    7);
        EXPECT_EQ(m.tracking_number, 42);
        EXPECT_EQ(m.timestamp_ns,    999999ULL);
        EXPECT_EQ(m.event_code,      'O');
    };

    itch::Parser parser(std::move(cb));
    parser.feed(full, sizeof(full));
    EXPECT_TRUE(called);
}

// ─────────────────────────────────────────────
//  AddOrder
// ─────────────────────────────────────────────

TEST(ITCHParser, AddOrder) {
    // Message layout (type 'A', length = 35):
// buf[0-1]   MoldUDP64 length      write_u16(buf, MSG_LEN)
// buf[2]     type = 'A'            buf[2] = 'A'
// buf[3-4]   stock_locate          write_u16(buf + 3, 1)
// buf[5-6]   tracking_number       write_u16(buf + 5, 0)
// buf[7-12]  timestamp (6 bytes)   write_u48(buf + 7, ...)
// buf[13-20] order_ref (8 bytes)   write_u64(buf + 13, ...)
// buf[21]    side                  buf[21] = 'B'
// buf[22-25] shares                write_u32(buf + 22, 100)
// buf[26-33] stock (8 bytes)       memcpy(buf + 26, ...)
// buf[34-37] price                 write_u32(buf + 34, ...)

    constexpr std::size_t MSG_LEN = 36;
    constexpr std::size_t BUF_LEN = 2 + MSG_LEN;
    uint8_t buf[BUF_LEN] = {};

    write_u16(buf,       MSG_LEN);
    buf[2] = 'A';
    write_u16(buf + 3,   1);           // stock_locate
    write_u16(buf + 5,   0);           // tracking_number
    write_u48(buf + 7,   123456789ULL);// timestamp_ns
    write_u64(buf + 13,  987654321ULL);// order_ref
    buf[21] = 'B';                     // side — bid
    write_u32(buf + 22,  100u);         // shares
    std::memcpy(buf + 26, "AAPL    ", 8); // stock (space padded)
    write_u32(buf + 34,  1500500u);     // price = $150.05 × 10000

    itch::Callbacks cb;
    bool called = false;
    cb.on_add_order = [&](const itch::AddOrderMsg& m) {
        called = true;
        EXPECT_EQ(m.stock_locate,    1);
        EXPECT_EQ(m.timestamp_ns,    123456789ULL);
        EXPECT_EQ(m.order_ref,       987654321ULL);
        EXPECT_EQ(m.side,            'B');
        EXPECT_EQ(m.shares,          100u);
        EXPECT_EQ(std::string(m.stock), "AAPL    ");
        EXPECT_EQ(m.price,           1500500u);
    };

    itch::Parser parser(std::move(cb));
    parser.feed(buf, BUF_LEN);
    EXPECT_TRUE(called);
}

// ─────────────────────────────────────────────
//  OrderCancel
// ─────────────────────────────────────────────

TEST(ITCHParser, OrderCancel) {
    constexpr std::size_t MSG_LEN = 23;
    constexpr std::size_t BUF_LEN = 2 + MSG_LEN;
    uint8_t buf[BUF_LEN] = {};

    write_u16(buf,       MSG_LEN);
    buf[2] = 'X';
    write_u16(buf + 3,   5);
    write_u16(buf + 5,   0);
    write_u48(buf + 7,   111222333ULL);
    write_u64(buf + 13,  555666777ULL); // order_ref
    write_u32(buf + 21,  50);           // cancelled_shares

    itch::Callbacks cb;
    bool called = false;
    cb.on_order_cancel = [&](const itch::OrderCancelMsg& m) {
        called = true;
        EXPECT_EQ(m.order_ref,        555666777ULL);
        EXPECT_EQ(m.cancelled_shares, 50u);
    };

    itch::Parser parser(std::move(cb));
    parser.feed(buf, BUF_LEN);
    EXPECT_TRUE(called);
}

// ─────────────────────────────────────────────
//  OrderDelete
// ─────────────────────────────────────────────

TEST(ITCHParser, OrderDelete) {
    constexpr std::size_t MSG_LEN = 19;
    constexpr std::size_t BUF_LEN = 2 + MSG_LEN;
    uint8_t buf[BUF_LEN] = {};

    write_u16(buf,       MSG_LEN);
    buf[2] = 'D';
    write_u16(buf + 3,   2);
    write_u16(buf + 5,   0);
    write_u48(buf + 7,   999ULL);
    write_u64(buf + 13,  112233ULL); // order_ref

    itch::Callbacks cb;
    bool called = false;
    cb.on_order_delete = [&](const itch::OrderDeleteMsg& m) {
        called = true;
        EXPECT_EQ(m.order_ref, 112233ULL);
    };

    itch::Parser parser(std::move(cb));
    parser.feed(buf, BUF_LEN);
    EXPECT_TRUE(called);
}

// ─────────────────────────────────────────────
//  Unknown message type is silently skipped
// ─────────────────────────────────────────────

TEST(ITCHParser, UnknownTypeSkipped) {
    uint8_t buf[5] = {};
    write_u16(buf, 3);
    buf[2] = 'Z';  // unknown type
    buf[3] = 0xFF;
    buf[4] = 0xFF;

    itch::Callbacks cb;  // no callbacks registered
    itch::Parser parser(std::move(cb));

    // Should not crash or throw
    EXPECT_NO_THROW(parser.feed(buf, sizeof(buf)));
}

// ─────────────────────────────────────────────
//  Truncated buffer — parser must not overread
// ─────────────────────────────────────────────

TEST(ITCHParser, TruncatedBufferSafe) {
    uint8_t buf[4] = {};
    write_u16(buf, 100);  // claims 100 bytes but buffer is only 4
    buf[2] = 'A';

    itch::Callbacks cb;
    bool called = false;
    cb.on_add_order = [&](const itch::AddOrderMsg&) { called = true; };

    itch::Parser parser(std::move(cb));
    parser.feed(buf, sizeof(buf));
    EXPECT_FALSE(called);  // must not have fired
}
