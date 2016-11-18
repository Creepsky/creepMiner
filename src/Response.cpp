#include "Response.hpp"
#include "Socket.hpp"
#include "MinerUtil.h"
#include <iterator>

Burst::Response::Response(std::unique_ptr<Socket> socket)
	: socket_(std::move(socket))
{}

bool Burst::Response::canReceive() const
{
	return socket_ != nullptr && socket_->isConnected();
}

bool Burst::Response::receive(std::string& data) const
{
	if (!canReceive())
		return false;

	return socket_->receive(data);
}

std::unique_ptr<Burst::Socket> Burst::Response::close()
{
	return std::move(socket_);
}

Burst::HttpResponse::HttpResponse(const std::string& response)
	: response_(response)
{}

void Burst::HttpResponse::setResponse(const std::string& response)
{
	response_ = response;
}

const std::string& Burst::HttpResponse::getResponse() const
{
	return response_;
}

std::string Burst::HttpResponse::getStatus() const
{
	return getPart(0);
}

std::string Burst::HttpResponse::getContentLength() const
{
	return getPart(1);
}

std::string Burst::HttpResponse::getContentType() const
{
	return getPart(2);
}

std::string Burst::HttpResponse::getDate() const
{
	return getPart(3);
}

std::string Burst::HttpResponse::getMessage() const
{
	return getPart(4);
}

std::string Burst::HttpResponse::getPart(size_t index) const
{
	auto tokens = Burst::splitStr(response_, "\r\n");

	if (index >= tokens.size())
		return "";

	return tokens[index];
}
