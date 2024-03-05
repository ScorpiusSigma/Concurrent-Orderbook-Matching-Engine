#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include <mutex>
#include <atomic>
#include <condition_variable> 

enum OrderSide
{
	Buy,
	Sell
};

class Order {
public:
    std::mutex mut;
    Order* next = nullptr;
    uint32_t orderID;
    uint32_t price;
    uint32_t qty;
    std::string instrument;
    OrderSide side;
    uint32_t executionNum;
    bool isDummy;
    bool isDeleted;

    Order(uint32_t id, uint32_t p, uint32_t q, std::string instrument, OrderSide side, uint32_t executionNum, bool dummy) : orderID(id), price(p), qty(q), instrument(instrument), side(side), executionNum(executionNum), isDummy(dummy), isDeleted(false) {}
};

class OrderBook {
private:
    Order* buy_front; // This is the buy list
    Order* sell_front; // This is the sell list
    std::mutex mut;
    std::atomic<long> bridge;
    std::condition_variable condVar;

public:
    OrderBook();
    ~OrderBook();
    void push_buy_order(Order* order);
    void push_sell_order(Order* order);
    void execute_buy_orders(Order* order);
    void execute_sell_orders(Order* order);
    void delete_orders(Order* order, uint32_t orderId);
    // Additional methods as needed...
};

#endif // ORDERBOOK_HPP
