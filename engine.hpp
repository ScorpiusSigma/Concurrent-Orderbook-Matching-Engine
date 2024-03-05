// This file contains declarations for the main Engine class. You will
// need to add declarations to this file as you develop your Engine.

#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <chrono>
#include <mutex>
#include <unordered_map>
#include "io.hpp"
#include "OrderBook.hpp"
#include <optional> // Include for std::optional

struct Engine
{
public:
	void accept(ClientConnection conn);

private:
	std::unordered_map<std::string, OrderBook*> exchange {};
	std::unordered_map<uint32_t, Order*> idOrderMapper {};

	std::mutex exchange_mutex {};

	OrderBook* get_orderbook(Order* order);
	std::optional<std::tuple<OrderBook*, Order*>>  get_orderbook_from_orderid(uint32_t orderID);

	void connection_thread(ClientConnection conn);
};

inline std::chrono::microseconds::rep getCurrentTimestamp() noexcept
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

#endif