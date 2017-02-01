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
#include "PlotSizes.hpp"

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
		if (MinerConfig::getConfig().output.error.request)
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
	
	auto url = "/burst?requestType=submitNonce&nonce=" + std::to_string(nonce) + "&accountId=" + std::to_string(accountId);

	if (!MinerConfig::getConfig().getPassphrase().empty())
		url += "&secretPhrase=" + MinerConfig::getConfig().getPassphrase();

	std::string responseData;

	HTTPRequest request{HTTPRequest::HTTP_POST, url, HTTPRequest::HTTP_1_1};
	request.set("X-Capacity", std::to_string(PlotSizes::getTotal()));
	request.set("X-PlotsHash", MinerConfig::getConfig().getConfig().getPlotsHash());
	request.set("X-Miner", Settings::Project.nameAndVersionAndOs);
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
