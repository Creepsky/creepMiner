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

#include <Poco/JSON/Parser.h>
#include <Poco/NestedDiagnosticContext.h>
#include "Response.hpp"
#include "MinerUtil.hpp"
#include "logging/MinerLogger.hpp"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPResponse.h"

using namespace Poco::Net;

Burst::Response::Response(std::unique_ptr<Poco::Net::HTTPClientSession> session)
	: session_(std::move(session))
{}

Burst::Response::~Response()
{
	if (session_ != nullptr)
		session_->reset();
}

bool Burst::Response::canReceive() const
{
	return session_ != nullptr;
}

bool Burst::Response::receive(std::string& data) const
{
	int status;
	return receive(data, status);
}

bool Burst::Response::receive(std::string& data, int& status) const
{
	poco_ndc(Response::receive);
	
	if (!canReceive())
		return false;

	try
	{
		HTTPResponse response;
		const auto responseStream = &session_->receiveResponse(response);
		data = {std::istreambuf_iterator<char>(*responseStream), {}};
		status = response.getStatus();
		return status == HTTPResponse::HTTP_OK;
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::socket,
			"Error on receiving response!\n%s",
			exc.displayText()
		);

		log_current_stackframe(MinerLogger::socket);

		session_->reset();
		return false;
	}
}

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::Response::transferSession()
{
	return std::move(session_);
}

const Poco::Exception* Burst::Response::getLastError() const
{
	return session_->networkException();
}

bool Burst::Response::isDataThere() const
{
	return session_->socket().poll(Poco::Timespan{60, 0},
		Poco::Net::Socket::SELECT_READ);
}

Burst::NonceConfirmation Burst::NonceConfirmation::createWrongBlock(Poco::UInt64 currentHeight, Poco::UInt64 yourHeight,
	Poco::UInt64 nonce, Poco::UInt64 deadline)
{
	NonceConfirmation c;
	c.json = Poco::format(
		R"({ "result" : "Your submitted deadline is for another block!", "nonce" : %Lu, "blockheight" : %Lu, "currentBlockheight" : %Lu })",
		nonce, yourHeight, currentHeight);
	c.errorText = "Your submitted deadline is for another block!";
	c.errorCode = SubmitResponse::WrongBlock;
	c.errorNumber = 1005;
	c.deadline = deadline;
	return c;
}

Burst::NonceConfirmation Burst::NonceConfirmation::createTooHigh(Poco::UInt64 nonce, Poco::UInt64 deadline,
	Poco::UInt64 targetDeadline)
{
	NonceConfirmation c;
	c.json = Poco::format(
		R"({ "result" : "Your deadline was too high!", "nonce": %Lu, "deadline" : %Lu, "targetDeadline" : %Lu, "" })",
		nonce, deadline, targetDeadline);
	c.errorText = "Your deadline was too high!";
	c.errorCode = SubmitResponse::TooHigh;
	c.errorNumber = 1008;
	c.deadline = deadline;
	return c;
}

Burst::NonceConfirmation Burst::NonceConfirmation::createNotBest(Poco::UInt64 nonce, Poco::UInt64 deadline,
	Poco::UInt64 best)
{
	NonceConfirmation c;
	c.json = Poco::format(
		R"({ "result" : "Your deadline was not the best one!", "nonce": %Lu, "deadline" : %Lu, "bestDeadline" : %Lu })",
		nonce, deadline, best);
	c.errorText = "Your deadline was not the best one!";
	c.errorCode = SubmitResponse::NotBest;
	c.errorNumber = -1;
	c.deadline = deadline;
	return c;
}

Burst::NonceConfirmation Burst::NonceConfirmation::createError(Poco::UInt64 nonce, Poco::UInt64 deadline,
	const std::string& errorMessage)
{
	NonceConfirmation c;
	c.json = Poco::format(
		R"({ "result" : "Error occured: %s!", "nonce": %Lu, "deadline" : %Lu, "error" : %s })",
		errorMessage, nonce, deadline, errorMessage);
	c.errorText = errorMessage;
	c.errorCode = SubmitResponse::Error;
	c.errorNumber = -1;
	c.deadline = deadline;
	return c;
}

Burst::NonceConfirmation Burst::NonceConfirmation::createSuccess(Poco::UInt64 nonce, Poco::UInt64 deadline,
	const std::string& deadlineText)
{
	NonceConfirmation c;
	c.json = Poco::format(
		R"({ "result" : "success", "deadline" : %Lu, "deadlineText" : "%s", "deadlineString" : "%s" })",
		deadline, deadlineText, deadlineText);
	c.errorText = "Deadline confirmed!";
	c.errorCode = SubmitResponse::Error;
	c.errorNumber = -1;
	c.deadline = deadline;
	return c;
}

Burst::NonceResponse::NonceResponse(std::unique_ptr<HTTPClientSession> session)
	: response_(std::move(session))
{}

bool Burst::NonceResponse::canReceive() const
{
	return response_.canReceive();
}

Burst::NonceConfirmation Burst::NonceResponse::getConfirmation() const
{
	poco_ndc(NonceResponse::getConfirmation);
	
	std::string response;
	int status;
	NonceConfirmation confirmation{0, SubmitResponse::None, "", 0, ""};

	if (response_.receive(response, status))
	{
		try
		{
			Poco::JSON::Parser parser;
			auto root = parser.parse(response).extract<Poco::JSON::Object::Ptr>();

			confirmation.json = response;

			if (root->has("deadline"))
			{
                confirmation.deadline = static_cast<Poco::UInt64>(root->get("deadline"));
				confirmation.errorCode = SubmitResponse::Confirmed;
			}
			else if (root->has("errorCode"))
			{
				log_warning(MinerLogger::nonceSubmitter, "Error: %s", root->get("errorDescription").convert<std::string>());
				// we send true so we dont send it again and again
				confirmation.errorCode = SubmitResponse::Error;
			}
			else if (root->has("result"))
			{
				log_warning(MinerLogger::nonceSubmitter, "Error: %s", root->get("result").convert<std::string>());
				confirmation.errorCode = SubmitResponse::Error;
			}
			else
			{
				log_warning(MinerLogger::nonceSubmitter, response);
				confirmation.errorCode = SubmitResponse::Error;
			}
		}
		catch (Poco::Exception& exc)
		{			
			log_error(MinerLogger::socket,
				"Error while waiting for confirmation!\n%s",
				exc.displayText()
			);
			
			log_current_stackframe(MinerLogger::socket);

			confirmation.errorCode = SubmitResponse::Error;
		}
	}
	else
	{
		confirmation.errorCode = SubmitResponse::Error;
		confirmation.json = response;
		Poco::JSON::Parser jsonParser;

		try
		{
			auto json = jsonParser.parse(response).extract<Poco::JSON::Object::Ptr>();
			if (json->has("errorDescription"))
				confirmation.errorText = json->getValue<std::string>("errorDescription");
			else if (json->has("result"))
				confirmation.errorText = json->getValue<std::string>("result");

			if (json->has("errorCode"))
				confirmation.errorNumber = json->getValue<int>("errorCode");
			else
				confirmation.errorNumber = -1;
		}
		catch (...)
		{}
	}

	return confirmation;
}

bool Burst::NonceResponse::isDataThere() const
{
	return response_.isDataThere();
}

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::NonceResponse::transferSession()
{
	return response_.transferSession();
}

const Poco::Exception* Burst::NonceResponse::getLastError() const
{
	return response_.getLastError();
}

Burst::HttpResponse::HttpResponse(const std::string& response)
{
	setResponse(response);
}

void Burst::HttpResponse::setResponse(const std::string& response)
{
	response_ = response;
	tokens_ = splitStr(response_, "\r\n");
}

const std::string& Burst::HttpResponse::getResponse() const
{
	return response_;
}

const std::string& Burst::HttpResponse::getStatus() const
{
	return getPart(0);
}

const std::string& Burst::HttpResponse::getContentLength() const
{
	return getPart(1);
}

const std::string& Burst::HttpResponse::getContentType() const
{
	return getPart(2);
}

const std::string& Burst::HttpResponse::getDate() const
{
	return getPart(tokens_.size() - 2);
}

const std::string& Burst::HttpResponse::getMessage() const
{
	return getPart(tokens_.size() - 1);
}

const std::string& Burst::HttpResponse::getPart(size_t index) const
{
	static std::string empty;

	if (index >= tokens_.size())
		return empty;

	return tokens_[index];
}
