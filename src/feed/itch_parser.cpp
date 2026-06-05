#include "itch_parser.hpp"
#include <cstring>

namespace itch {

//  ITCH message framing (as per MoldUDP64):
//    [2 bytes] message length  (does NOT include the 2 length bytes)
//    [1 byte]  message type
//    [N bytes] payload


void Parser::feed(const uint8_t* incoming, std::size_t len) {
    // prepend any m_leftover bytes from last call
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), m_leftover.begin(), m_leftover.end());
    buf.insert(buf.end(), incoming, incoming + len);

    // parse returns how many bytes were consumed
    std::size_t consumed = parse(buf.data(), buf.size());

    // save anything the parser couldn't complete
    m_leftover.assign(buf.begin() + consumed, buf.end());
}

std::size_t Parser::parse(const uint8_t* buf, std::size_t len) {
    const uint8_t* cursor = buf;
    const uint8_t* end    = buf + len;

    while (cursor + 2 <= end) {
        uint16_t msg_len = read_u16(cursor);

        // Guard: don't process if full message isn't in buffer yet
        if (cursor + 2 + msg_len > end)
            break;

        char type = static_cast<char>(cursor[2]);
        processItchMsg(cursor + 2, type);   // pass pointer to type byte

        cursor += 2 + msg_len;
    }

    return static_cast<std::size_t>(cursor - buf);
}


void Parser::processItchMsg(const uint8_t* msg, char type) {
    switch (type) {
        case 'S': parse_system_event(msg);   break;
        case 'A': parse_add_order(msg);      break;
        case 'F': parse_add_order(msg);      break; // MPID variant, same fields
        case 'E': parse_order_executed(msg); break;
        case 'X': parse_order_cancel(msg);   break;
        case 'D': parse_order_delete(msg);   break;
        case 'U': parse_order_replace(msg);  break;
        case 'P': parse_trade(msg);          break;
        default:  break;  // silently skip unknown types
    }
}

/////////////////////////////////////////////////////////////
//  Per-type parsers
//  p points at the type byte, so fields start at p+1
/////////////////////////////////////////////////////////////

void Parser::parse_system_event(const uint8_t* p) {
    if (!cb_.on_system_event) return;
    auto m = make_msg<SystemEventMsg>(p);  // 6-byte timestamp — see note below
    m.event_code      = static_cast<char>(p[11]);
    cb_.on_system_event(m);
}

void Parser::parse_add_order(const uint8_t* p) {
    if (!cb_.on_add_order) return;
    auto m = make_msg<AddOrderMsg>(p);
    m.order_ref       = read_u64(p + 11);
    m.side            = static_cast<char>(p[19]);
    m.shares          = read_u32(p + 20);
    std::memcpy(m.stock, p + 24, 8);
    m.stock[8]        = '\0';
    m.price           = read_u32(p + 32);
    cb_.on_add_order(m);
}

void Parser::parse_order_executed(const uint8_t* p) {
    if (!cb_.on_order_executed) return;
    auto m = make_msg<OrderExecutedMsg>(p);
    m.order_ref       = read_u64(p + 11);
    m.executed_shares = read_u32(p + 19);
    m.match_number    = read_u64(p + 23);
    cb_.on_order_executed(m);
}

void Parser::parse_order_cancel(const uint8_t* p) {
    if (!cb_.on_order_cancel) return;
    auto m = make_msg<OrderCancelMsg>(p);
    m.order_ref         = read_u64(p + 11);
    m.cancelled_shares  = read_u32(p + 19);
    cb_.on_order_cancel(m);
}

void Parser::parse_order_delete(const uint8_t* p) {
    if (!cb_.on_order_delete) return;
    auto m = make_msg<OrderDeleteMsg>(p);
    m.order_ref       = read_u64(p + 11);
    cb_.on_order_delete(m);
}

void Parser::parse_order_replace(const uint8_t* p) {
    if (!cb_.on_order_replace) return;
    auto m = make_msg<OrderReplaceMsg>(p);
    m.orig_order_ref  = read_u64(p + 11);
    m.new_order_ref   = read_u64(p + 19);
    m.shares          = read_u32(p + 27);
    m.price           = read_u32(p + 31);
    cb_.on_order_replace(m);
}

void Parser::parse_trade(const uint8_t* p) {
    if (!cb_.on_trade) return;
    auto m = make_msg<TradeMsg>(p);
    m.order_ref       = read_u64(p + 11);
    m.side            = static_cast<char>(p[19]);
    m.shares          = read_u32(p + 20);
    std::memcpy(m.stock, p + 24, 8);
    m.stock[8]        = '\0';
    m.price           = read_u32(p + 32);
    m.match_number    = read_u64(p + 36);
    cb_.on_trade(m);
}

} // namespace itch