// ==========================================================================
// 
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2018 Creepsky (creepsky@gmail.com)
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301  USA
// 
// ==========================================================================

#include "Request.hpp"
#include "Response.hpp"
#include "nxt/nxt_address.h"
#include "logging/MinerLogger.hpp"
#include "mining/MinerConfig.hpp"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include <Poco/Net/HTTPResponse.h>
#include <Poco/NestedDiagnosticContext.h>
#include "Poco/StreamCopier.h"
#include "Declarations.hpp"
#include "plots/PlotSizes.hpp"
#include <Poco/Net/HTMLForm.h>
#include "mining/Deadline.hpp"

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

	request.set("User-Agent", Settings::Project.nameAndVersion);

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
	uri.addQueryParameter("blockheight", std::to_string(deadline.getBlock()));
	
	if (!MinerConfig::getConfig().getPassphrase().empty())
		uri.addQueryParameter("secretPhrase", MinerConfig::getConfig().getPassphrase());
	
	std::string plotFileStr;
	const auto& plotFileStrDecoded = deadline.getPlotFile();
	Poco::URI::encode(plotFileStrDecoded, "", plotFileStr);

	HTTPRequest request{HTTPRequest::HTTP_POST, uri.getPathAndQuery(), HTTPRequest::HTTP_1_1};
	request.set(X_Capacity, std::to_string(deadline.getTotalPlotsize()));
	request.set(X_Miner, deadline.getMiner());
	request.set(X_Deadline, std::to_string(deadline.getDeadline()));
	request.set(X_Plotfile, plotFileStr);
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
