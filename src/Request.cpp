#include "Request.hpp"
#include "Socket.hpp"
#include <sstream>
#include "Response.hpp"
#include "nxt/nxt_address.h"
#include "MinerLogger.h"
#include "MinerUtil.h"
#include "MinerConfig.h"

Burst::Request::Request(std::unique_ptr<Socket> socket)
	: socket_(std::move(socket))
{}

Burst::Request::~Request()
{
	if (socket_ != nullptr)
		socket_->disconnect();
}

bool Burst::Request::canSend() const
{
	return socket_ != nullptr && socket_->isConnected();
}

Burst::Response Burst::Request::sendPost(const std::string& url, const std::string& body, const std::string& header)
{
	if (!send(url, "POST", body, header))
		return Response{ nullptr };
	return { close() };
}

Burst::Response Burst::Request::sendGet(const std::string& url)
{
	// empty body and head for more performance
	static const std::string body = "";
	static const std::string head = "";

	if (!send(url, "GET", body, head))
		return { nullptr };
	return { close() };
}

std::unique_ptr<Burst::Socket> Burst::Request::close()
{
	return std::move(socket_);
}

bool Burst::Request::send(const std::string& url, const std::string& method, const std::string& body, const std::string& header) const
{
	if (!canSend())
		return false;

	// TODO: \r\n to std::endl
	std::stringstream request;
	request << method << " " << url << " HTTP/1.0" << "\r\n" << header << "\r\n\r\n" << body;
	
	return socket_->send(request.str());
}

Burst::NonceRequest::NonceRequest(std::unique_ptr<Socket> socket)
	: request_(std::move(socket))
{}

Burst::NonceResponse Burst::NonceRequest::submit(uint64_t nonce, uint64_t accountId, uint64_t& deadline)
{
	NxtAddress addr(accountId);

	auto requestData = "requestType=submitNonce&nonce=" + std::to_string(nonce) + "&accountId=" + std::to_string(accountId) + "&secretPhrase=cryptoport";
	auto url = "/burst?" + requestData;
	std::string responseData;

	std::stringstream requestHead;

	requestHead << "Connection: close" << "\r\n" <<
		"X-Capacity: " << MinerConfig::getConfig().getTotalPlotsize() / 1024 / 1024 / 1024 << "\r\n"
		"X-Miner: creepsky-uray-" + versionToString();

	auto response = request_.sendPost(url, "", requestHead.str());

	if (response.canReceive())
		return {response.close()};

	return {nullptr};
}

std::unique_ptr<Burst::Socket> Burst::NonceRequest::close()
{
	return request_.close();
}
