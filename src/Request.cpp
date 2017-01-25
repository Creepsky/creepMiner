#include "Request.hpp"
#include "Socket.hpp"
#include <sstream>
#include "Response.hpp"
#include "nxt/nxt_address.h"
#include "MinerLogger.hpp"
#include "MinerUtil.hpp"
#include "MinerConfig.hpp"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include <Poco/Net/HTTPResponse.h>
#include <Poco/NestedDiagnosticContext.h>
#include "Poco/StreamCopier.h"
#include "Declarations.hpp"

using namespace Poco::Net;

Burst::Request::Request(std::unique_ptr<HTTPClientSession> session)
	: session_(std::move(session))
{}

Burst::Request::~Request()
{
	if (session_ != nullptr)
		session_->reset();
}

bool Burst::Request::canSend() const
{
	return session_ != nullptr;
}

Burst::Response Burst::Request::send(Poco::Net::HTTPRequest& request)
{
	poco_ndc(Request::send(Poco::Net::HTTPRequest&));
	
	if (!canSend())
		return {nullptr};
	
	try
	{
		session_->sendRequest(request);
	}
	catch(std::exception& exc)
	{		
		MinerLogger::writeStackframe(std::string("error on sending request: ") + exc.what());
		session_->reset();
		return {nullptr};
	}

	return {transferSession()};
}

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::Request::transferSession()
{
	return std::move(session_);
}

Burst::NonceRequest::NonceRequest(std::unique_ptr<Poco::Net::HTTPClientSession> socket)
	: request_(std::move(socket))
{}

Burst::NonceResponse Burst::NonceRequest::submit(uint64_t nonce, uint64_t accountId)
{
	poco_ndc(NonceRequest::submit);
	NxtAddress addr(accountId);

	auto requestData = "requestType=submitNonce&nonce=" + std::to_string(nonce) + "&accountId=" + std::to_string(accountId);
	auto url = "/burst?" + requestData;

	std::string responseData;

	HTTPRequest request{HTTPRequest::HTTP_POST, url, HTTPRequest::HTTP_1_1};
	request.set("X-Capacity", std::to_string(MinerConfig::getConfig().getTotalPlotsize() / 1024 / 1024 / 1024));
	request.set("X-Miner", "creepMINER " + versionToString() + " " + Settings::OsFamily);
	request.setKeepAlive(false);
	request.setContentLength(0);

	auto response = request_.send(request);

	if (response.canReceive())
		return{ response.transferSession() };

	return{ nullptr };
}

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::NonceRequest::transferSession()
{
	return request_.transferSession();
}
