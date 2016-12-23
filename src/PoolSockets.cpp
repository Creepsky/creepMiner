#include "PoolSockets.hpp"
#include "Socket.hpp"
#include "MinerConfig.hpp"

Burst::PoolSockets::PoolSockets(size_t maxSockets)
	: maxSockets_(maxSockets)
{}

Burst::PoolSockets::PoolSockets(const PoolSockets& rhs)
{
	maxSockets_ = rhs.maxSockets_;
}

Burst::PoolSockets::PoolSockets(PoolSockets&& rhs)
{
	maxSockets_ = rhs.maxSockets_;
	sockets_ = std::move(rhs.sockets_);
}

Burst::PoolSockets::~PoolSockets()
{
	clear();
}

Burst::PoolSockets& Burst::PoolSockets::operator=(const PoolSockets& rhs)
{
	if (this != &rhs)
	{
		maxSockets_ = rhs.maxSockets_;
	}

	return *this;
}

Burst::PoolSockets& Burst::PoolSockets::operator=(PoolSockets&& rhs)
{
	if (this != &rhs)
	{
		maxSockets_ = rhs.maxSockets_;
		sockets_ = std::move(rhs.sockets_);
	}

	return *this;
}

std::unique_ptr<Burst::Socket> Burst::PoolSockets::getSocket()
{
	if (getSize() == 0)
		return MinerConfig::getConfig().createSocket();

	auto socket = std::move(sockets_.front());
	sockets_.pop_front();

	return socket;
}

size_t Burst::PoolSockets::getSize() const
{
	return sockets_.size();
}

size_t Burst::PoolSockets::getMax() const
{
	return maxSockets_;
}

void Burst::PoolSockets::fill()
{
	while (getSize() < getMax())
		sockets_.emplace_back(MinerConfig::getConfig().createSocket());
}

void Burst::PoolSockets::clear()
{
	for (auto& socket : sockets_)
		socket->disconnect();

	sockets_.clear();
}
