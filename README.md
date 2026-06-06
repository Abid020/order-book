This project aims to solve the order book problem, using the NASDAQ ITCH5.0 API.

Each price is individually tracked through an order, with bids and asks stored in lock free queues.

sorted order_ref makes lookup O(1)
