#pragma once

#include <vector>
#include <cstdint>
#include <cstring>
#include <functional>
#include <arpa/inet.h>  // ntohs, ntohl

namespace itch {

// ─────────────────────────────────────────────
//  Byte-swap utilities
//  ITCH is big-endian, x86 is little-endian
// ─────────────────────────────────────────────

inline uint8_t read_u8(const uint8_t* p) {
    return *p;
}
inline uint16_t read_u16(const uint8_t* p) {
    uint16_t v; std::memcpy(&v, p, 2); return ntohs(v);
}
inline uint32_t read_u32(const uint8_t* p) {
    uint32_t v; std::memcpy(&v, p, 4); return ntohl(v);
}
inline uint64_t read_u64(const uint8_t* p) {
    uint64_t v; std::memcpy(&v, p, 8);
    return (static_cast<uint64_t>(ntohl(v & 0xFFFFFFFF)) << 32)
           | ntohl(v >> 32);
}
inline uint64_t read_u48(const uint8_t* p) {
    uint32_t hi; uint16_t lo;
    std::memcpy(&hi, p,     4);
    std::memcpy(&lo, p + 4, 2);
    return (static_cast<uint64_t>(ntohl(hi)) << 16)
         |  static_cast<uint64_t>(ntohs(lo));
}
// ─────────────────────────────────────────────
//  Message type codes  (ITCH 5.0 spec section 4)
// ─────────────────────────────────────────────

enum class MsgType : uint8_t {
    SystemEvent       = 'S',
    AddOrder          = 'A',
    AddOrderMPID      = 'F',
    OrderExecuted     = 'E',
    OrderExecutedPrice= 'C',
    OrderCancel       = 'X',
    OrderDelete       = 'D',
    OrderReplace      = 'U',
    Trade             = 'P',
    Unknown           = '\0'
};

// ─────────────────────────────────────────────
//  Message structs
//  All fields already converted to host byte order
//  by the parser — consumers never see raw bytes
// ─────────────────────────────────────────────

struct MsgHeader {
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint64_t timestamp_ns;
};

struct SystemEventMsg : MsgHeader {
    char     event_code;     // 'O' open, 'C' close, 'A' accepting orders
};

struct AddOrderMsg : MsgHeader{
    uint64_t order_ref;
    char     side;           // 'B' bid, 'S' ask
    uint32_t shares;
    char     stock[9];       // 8 chars + null terminator
    uint32_t price;          // price × 10,000  e.g. $150.05 = 1500500
};

struct OrderExecutedMsg : MsgHeader{
    uint64_t order_ref;
    uint32_t executed_shares;
    uint64_t match_number;
};

struct OrderCancelMsg : MsgHeader{
    uint64_t order_ref;
    uint32_t cancelled_shares;
};

struct OrderDeleteMsg : MsgHeader{
    uint64_t order_ref;
};

struct OrderReplaceMsg : MsgHeader{
    uint64_t orig_order_ref;
    uint64_t new_order_ref;
    uint32_t shares;
    uint32_t price;
};

struct TradeMsg : MsgHeader{
    uint64_t order_ref;
    char     side;
    uint32_t shares;
    char     stock[9];
    uint32_t price;
    uint64_t match_number;
};

// ─────────────────────────────────────────────
//  Callbacks — book and visualiser register
//  these to receive decoded messages
// ─────────────────────────────────────────────

struct Callbacks {
    std::function<void(const SystemEventMsg&)>   on_system_event;
    std::function<void(const AddOrderMsg&)>      on_add_order;
    std::function<void(const OrderExecutedMsg&)> on_order_executed;
    std::function<void(const OrderCancelMsg&)>   on_order_cancel;
    std::function<void(const OrderDeleteMsg&)>   on_order_delete;
    std::function<void(const OrderReplaceMsg&)>  on_order_replace;
    std::function<void(const TradeMsg&)>         on_trade;
};

// ─────────────────────────────────────────────
//  Parser
// ─────────────────────────────────────────────

class Parser {
public:
    explicit Parser(Callbacks cb) : cb_(std::move(cb)) {}

    void feed(const uint8_t* incoming, std::size_t len);

private:
    Callbacks cb_;

    // Each message constructs first three fields in the same way
    template<typename T>
    static T make_msg(const uint8_t* p) {
        T m{};
        m.stock_locate    = read_u16(p + 1);
        m.tracking_number = read_u16(p + 3);
        m.timestamp_ns    = read_u48(p + 5);
        return m;
    }

    // Feed raw bytes into the parser.
    // buf must point to a complete ITCH message stream.
    // Returns number of bytes consumed.
    std::size_t parse(const uint8_t* buf, std::size_t len);

    void processItchMsg(const uint8_t* msg, char type);

    void parse_system_event  (const uint8_t* p);
    void parse_add_order     (const uint8_t* p);
    void parse_order_executed(const uint8_t* p);
    void parse_order_cancel  (const uint8_t* p);
    void parse_order_delete  (const uint8_t* p);
    void parse_order_replace (const uint8_t* p);
    void parse_trade         (const uint8_t* p);

    std::vector<uint8_t> m_leftover;
};

} // namespace itch