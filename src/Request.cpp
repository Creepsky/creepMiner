#include "Request.hpp"
#include "Socket.hpp"
#include "Response.hpp"
#include "nxt/nxt_address.h"
#include "MinerLogger.hpp"
#include "MinerConfig.hpp"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include <Poco/Net/HTTPResponse.h>
#include <Poco/NestedDiagnosticContext.h>
#include "Poco/StreamCopier.h"
#include "Declarations.hpp"
#include "PlotSizes.hpp"
#include <Poco/Net/HTMLForm.h>
#include "Deadline.hpp"
#include <Poco/Base64Encoder.h>

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
	catch(Poco::Exception& exc)
	{
		log_error(MinerLogger::socket, "Error on sending request: %s", exc.displayText());
		log_current_stackframe(MinerLogger::socket);

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

Burst::NonceResponse Burst::NonceRequest::submit(const Deadline& deadline)
{
	poco_ndc(NonceRequest::submit);

	Poco::URI uri;

	uri.setPath("/burst");

	uri.addQueryParameter("requestType", "submitNonce");
	uri.addQueryParameter("nonce", std::to_string(deadline.getNonce()));
	uri.addQueryParameter("accountId", std::to_string(deadline.getAccountId()));
	
	if (!MinerConfig::getConfig().getPassphrase().empty())
		uri.addQueryParameter("secretPhrase", MinerConfig::getConfig().getPassphrase());
	
	std::string plotFileStr, plotsHashStr;

	auto plotFileStrDecoded = deadline.getPlotFile();
	auto plotsHashStrDecoded = MinerConfig::getConfig().getConfig().getPlotsHash();

	Poco::URI::encode(plotFileStrDecoded, "", plotFileStr);
	Poco::URI::encode(plotsHashStrDecoded, "", plotsHashStr);

	HTTPRequest request{HTTPRequest::HTTP_POST, uri.getPathAndQuery(), HTTPRequest::HTTP_1_1};
	request.set(X_Capacity, std::to_string(PlotSizes::getTotal()));
	request.set(X_PlotsHash, plotsHashStr);
	request.set(X_Miner, Settings::Project.nameAndVersionAndOs);
	request.set(X_Deadline, std::to_string(deadline.getDeadline()));
	request.set(X_Plotfile, plotFileStr);
	request.setKeepAlive(false);
	request.setChunkedTransferEncoding(true);
	request.setContentType("text/plain");

	auto response = request_.send(request);

	if (response.canReceive())
		return{ response.transferSession() };

	return{ nullptr };
}

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::NonceRequest::transferSession()
{
	return request_.transferSession();
}
