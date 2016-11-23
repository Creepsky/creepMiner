#pragma once

#include <deque>
#include <memory>
#include "Socket.hpp"

namespace Burst
{
	class PoolSockets
	{
	public:
		PoolSockets() = default;
		explicit PoolSockets(size_t maxSockets);
		PoolSockets(const PoolSockets& rhs);
		PoolSockets(PoolSockets&& rhs);
		~PoolSockets();

		PoolSockets& operator=(const PoolSockets& rhs);
		PoolSockets& operator=(PoolSockets&& rhs);

		std::unique_ptr<Socket> getSocket();
		size_t getSize() const;
		size_t getMax() const;

		void fill();
		void clear();

	private:
		size_t maxSockets_ = 0;
		std::deque<std::unique_ptr<Socket>> sockets_;
	};
}
