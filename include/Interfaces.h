#pragma once
#include <vector>
#include <utility>
class OrderBook;
struct Order;
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute(OrderBook& book) = 0;
};
class IMatchingStrategy {
public:
    virtual ~IMatchingStrategy() = default;
    virtual std::pair<Order*, Order*> Match(OrderBook& book, ICommand* incoming_order) = 0;
};
