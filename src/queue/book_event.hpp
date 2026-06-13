#pragma once

#include <variant>
#include "feed/itch_parser.hpp"

namespace queue {

// ─────────────────────────────────────────────
//  BookEvent — a tagged union of all message
//  types the order book needs to process.
//
//  std::variant holds exactly one of these
//  types at a time. The consumer uses
//  std::visit to dispatch to the right
//  order book method.
// ─────────────────────────────────────────────

using BookEvent = std::variant
    itch::AddOrderMsg,
    itch::OrderCancelMsg,
    itch::OrderDeleteMsg,
    itch::OrderExecutedMsg,
    itch::OrderReplaceMsg
>;

} // namespace queue