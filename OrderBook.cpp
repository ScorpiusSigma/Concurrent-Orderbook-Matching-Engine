#include "OrderBook.hpp"

OrderBook::OrderBook(): bridge(0) {
    // Initialize dummy node
    buy_front = new Order(0, 0, 0, "D", Buy, 0, true);
    sell_front = new Order(0, 0, 0, "D", Sell, 0, true);
}

OrderBook::~OrderBook() {
    // Properly delete all orders to prevent memory leaks
    Order* current_buy_front = buy_front;
    while (current_buy_front != nullptr) {
        Order* next = current_buy_front->next;
        delete current_buy_front;
        current_buy_front = next;
    }

    Order* current_sell_front = sell_front;
    while (current_sell_front != nullptr) {
        Order* next = current_sell_front->next;
        delete current_sell_front;
        current_sell_front = next;
    }
}

// This is to push buy orders -- phase concurrency here
void OrderBook::push_buy_order(Order* order) {
    std::unique_lock<std::mutex> lockCurrent(buy_front->mut);
    Order* current = buy_front;

    while (true) {
        if (current->next == nullptr || (order->price > current->next->price)) {
            order->next = current->next;
            current->next = order;
            break; // Exit the loop after inserting
        } else {
            Order* nextOrder = current->next;
            std::unique_lock<std::mutex> temp(nextOrder->mut);
            lockCurrent.unlock(); // Unlock the current before moving to the next
            current = nextOrder;
            lockCurrent = std::move(temp); // Transfer ownership of the lock
        }
    }

    auto output_time = getCurrentTimestamp();
    Output::OrderAdded(order->orderID, order->instrument.c_str(), order->price, order->qty, order->side == Sell, output_time);
    lockCurrent.unlock();
}

// This is to push sell orders -- phase concurrency here
void OrderBook::push_sell_order(Order* order) {
    std::unique_lock<std::mutex> lockCurrent(sell_front->mut);
    Order *current = sell_front;

    while (true) {
        if (current->next == nullptr || (order->price < current->next->price)) {
            order->next = current->next;
            current->next = order;
            break;
        } else {
            Order* nextOrder = current->next;
            std::unique_lock<std::mutex> temp(nextOrder->mut);
            lockCurrent.unlock(); // Unlock the current before moving to the next
            current = nextOrder;
            lockCurrent = std::move(temp); // Transfer ownership of the lock
        }
    }

    auto output_time = getCurrentTimestamp();
    Output::OrderAdded(order->orderID, order->instrument.c_str(), order->price, order->qty, order->side == Sell, output_time);
    lockCurrent.unlock();
}

void OrderBook::execute_buy_orders(Order* order) {
    while (true) {
        std::lock_guard<std::mutex> obLock(this->mut); // Locks orderbook so no other orders can use it
        if (this->bridge.load(std::memory_order_acq_rel) >= 0) { // Checks if the bridge is allowing buy orders
            this->bridge.fetch_add(1, std::memory_order_acq_rel); // Increment for buy order entry
            break;
        }
    }
    
    std::unique_lock<std::mutex> lockSell(this->sell_front->mut);
    Order* prev_sell = sell_front;
    Order* current_sell = prev_sell->next;
    uint32_t quantity_order = order->qty;
    while (quantity_order > 0 && current_sell != nullptr) {
        if (current_sell->price > order->price) break;
        
        if (current_sell->qty > quantity_order) {
            current_sell->qty -= quantity_order;
            
            auto output_time = getCurrentTimestamp();
            Output::OrderExecuted(current_sell->orderID, order->orderID, ++current_sell->executionNum, current_sell->price, quantity_order, output_time);

            quantity_order = 0;
        } else {
            quantity_order -= current_sell->qty;

            auto output_time = getCurrentTimestamp();
            Output::OrderExecuted(current_sell->orderID, order->orderID, ++current_sell->executionNum, current_sell->price, current_sell->qty, output_time);

            prev_sell->next = current_sell->next;
            current_sell->isDeleted = true;
            current_sell = prev_sell->next;
        }
    }
    lockSell.unlock();

    if (quantity_order > 0) {
        order->qty = quantity_order;
        push_buy_order(order);
    }
    
    // Decrement for buy order exit
    this->bridge.fetch_sub(1, std::memory_order_acq_rel);
}

void OrderBook::execute_sell_orders(Order* order) {
    while (true) {
        std::lock_guard<std::mutex> obLock(this->mut); // Locks orderbook so no other orders can use it
        if (this->bridge.load(std::memory_order_acq_rel) <= 0) { // Checks if the bridge is allowing sell orders
            this->bridge.fetch_sub(1, std::memory_order_acq_rel); // Decrement for sell order entry
            break;
        }
    }
    
    std::unique_lock<std::mutex> lockBuy(this->buy_front->mut);
    Order* prev_buy = buy_front;
    Order* current_buy = prev_buy->next;
    uint32_t quantity_order = order->qty;
    while (quantity_order > 0 && current_buy != nullptr) {
        if (current_buy->price < order->price) break;
        
        if (current_buy->qty > quantity_order) {
            current_buy->qty -= quantity_order;
            
            auto output_time = getCurrentTimestamp();
            Output::OrderExecuted(current_buy->orderID, order->orderID, ++current_buy->executionNum, current_buy->price, quantity_order, output_time);

            quantity_order = 0;
        } else {
            quantity_order -= current_buy->qty;

            auto output_time = getCurrentTimestamp();
            Output::OrderExecuted(current_buy->orderID, order->orderID, ++current_buy->executionNum, current_buy->price, current_buy->qty, output_time);

            prev_buy->next = current_buy->next;
            current_buy->isDeleted = true;
            current_buy = prev_buy->next;
        }
    }
    lockBuy.unlock();

    if (quantity_order > 0) {
        order->qty = quantity_order;
        push_sell_order(order);
    }

    // Increment for sell order exit
    this->bridge.fetch_add(1, std::memory_order_acq_rel);
}

void OrderBook::delete_orders(Order* order, uint32_t orderId) {
    std::lock_guard<std::mutex> obLock(this->mut);
    Order* current = (order->side == Buy) ? this->buy_front : this->sell_front;
    while (!order->isDeleted) {
        std::unique_lock<std::mutex> lockCurrent(current->mut);
        Order* next = current->next;

        if (next == nullptr) break;
        if ((next != nullptr) && (next->orderID == orderId)) {
            // This is to prevent the next from changing
            std::unique_lock<std::mutex> lockComparator(next->mut);

            Order* nextOrder = next->next;
            current->next = nextOrder;
            
            auto output_time = getCurrentTimestamp();
            Output::OrderDeleted(orderId, true, output_time);
            
            lockComparator.unlock();
            next->isDeleted = true;
            lockCurrent.unlock();

            return;
        } else {
            current = current->next;
            lockCurrent.unlock();
        }
    }
    
    auto output_time = getCurrentTimestamp();
    Output::OrderDeleted(orderId, false, output_time);
    return;
}