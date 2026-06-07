#include "itch_parser.hpp"
#include "order_book.hpp"

int main() {
    book::OrderBook book;

    itch::Callbacks cb;
    cb.on_add_order     = [&](const itch::AddOrderMsg& m)      { book.add_order(m);     };
    cb.on_order_cancel  = [&](const itch::OrderCancelMsg& m)   { book.cancel_order(m);  };
    cb.on_order_delete  = [&](const itch::OrderDeleteMsg& m)   { book.delete_order(m);  };
    cb.on_order_executed= [&](const itch::OrderExecutedMsg& m) { book.execute_order(m); };
    cb.on_order_replace = [&](const itch::OrderReplaceMsg& m)  { book.replace_order(m); };

    itch::Parser parser(std::move(cb));
    return 0; 
}
