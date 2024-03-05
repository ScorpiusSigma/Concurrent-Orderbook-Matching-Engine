#include <iostream>
#include <thread>
#include <unordered_map>
#include <optional> // Include for std::optional
#include "io.hpp"
#include "engine.hpp"
#include "OrderBook.cpp"
#include "OrderBook.hpp"

OrderBook* Engine::get_orderbook(Order* order) {
	std::lock_guard<std::mutex> lock(exchange_mutex);

	std::string instrument = order->instrument;
	if (!exchange.contains(instrument)) {
		OrderBook *ob = new OrderBook();
		exchange.emplace(instrument, ob);
	}
	idOrderMapper.emplace(order->orderID, order);

	return exchange[instrument];
}

std::optional<std::tuple<OrderBook*, Order*>> Engine::get_orderbook_from_orderid(uint32_t orderID) {
    std::unique_lock<std::mutex> lock(exchange_mutex);

    if (idOrderMapper.contains(orderID)) {
        auto& order = idOrderMapper.at(orderID); // Use 'at' to safely access the element
		lock.unlock();
        return std::make_tuple(get_orderbook(order), order); // Assuming get_orderbook returns a pointer to OrderBook
    } else {
		lock.unlock();
        return {}; // Return an empty std::optional
    }
}

void Engine::accept(ClientConnection connection)
{
	auto thread = std::thread(&Engine::connection_thread, this, std::move(connection));
	thread.detach();
}

void Engine::connection_thread(ClientConnection connection)
{
	while(true)
	{
		ClientCommand input {};
		switch(connection.readInput(input))
		{
			case ReadResult::Error: SyncCerr {} << "Error reading input" << std::endl;
			case ReadResult::EndOfFile: return;
			case ReadResult::Success: break;
		}

		std::string instrument = input.instrument;
		uint32_t price = input.price;
		uint32_t size = input.count;
		uint32_t order_id = input.order_id;

		// Functions for printing output actions in the prescribed format are
		// provided in the Output class:
		switch(input.type)
		{
			case input_cancel: {
				// Attempt to retrieve the order book and side from the order ID
				auto res = get_orderbook_from_orderid(order_id);

				if (!res.has_value()) {
					// If there's no order book associated with the order ID, log the deletion attempt as unsuccessful
					auto output_time = getCurrentTimestamp();
					Output::OrderDeleted(input.order_id, false, output_time);
				} else {
					auto& [orderbook, order] = *res; // Destructure
					auto output_time = getCurrentTimestamp();
					
					if (order->isDeleted) Output::OrderDeleted(input.order_id, false, output_time);
					else orderbook->delete_orders(order, input.order_id);
				}
				break;
			}

			case input_buy: {
				Order* order = new Order(order_id, price, size, instrument, Buy, 0, false);
				get_orderbook(order)->execute_buy_orders(order);
				break;
			}

			case input_sell: {
				Order* order = new Order(order_id, price, size, instrument, Sell, 0, false);
				get_orderbook(order)->execute_sell_orders(order);
				break;
			}

			default: {
				SyncCerr {}
				    << "Got order: " << static_cast<char>(input.type) << " " << input.instrument << " x " << input.count << " @ "
				    << input.price << " ID: " << input.order_id << std::endl;

				// Remember to take timestamp at the appropriate time, or compute
				// an appropriate timestamp!
				auto output_time = getCurrentTimestamp();
				Output::OrderAdded(input.order_id, input.instrument, input.price, input.count, input.type == input_sell, output_time);
				break;
			}
		}
	}
}