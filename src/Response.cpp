#include "Response.hpp"
#include "Socket.hpp"
#include "MinerUtil.hpp"
#include "MinerConfig.hpp"
#include "rapidjson/document.h"
#include "MinerLogger.hpp"
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

bool Burst::Response::receive(std::string& data)
{
	if (!canReceive())
		return false;

	try
	{
		HTTPResponse response;
		std::stringstream sstream;
		std::istream* responseStream;

		responseStream = &session_->receiveResponse(response);
		
		data = {std::istreambuf_iterator<char>(*responseStream), {}};

		return true;
	}
	catch (Poco::Exception& exc)
	{
		MinerLogger::write(std::string("error on receiving response: ") + exc.what(), TextType::Debug);
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

Burst::NonceResponse::NonceResponse(std::unique_ptr<Poco::Net::HTTPClientSession> session)
	: response_(std::move(session))
{}

bool Burst::NonceResponse::canReceive() const
{
	return response_.canReceive();
}

Burst::NonceConfirmation Burst::NonceResponse::getConfirmation()
{
	std::string response;
	NonceConfirmation confirmation{ 0, SubmitResponse::None };

	if (response_.receive(response))
	{
		rapidjson::Document body;
		body.Parse<0>(response.c_str());

		if (body.GetParseError() == nullptr)
		{
			if (body.HasMember("deadline"))
			{
				confirmation.deadline = body["deadline"].GetUint64();
				confirmation.errorCode = SubmitResponse::Submitted;
			}
			else if (body.HasMember("errorCode"))
			{
				MinerLogger::write(std::string("error: ") + body["errorDescription"].GetString(), TextType::Error);
				// we send true so we dont send it again and again
				confirmation.errorCode = SubmitResponse::Error;
			}
			else if (body.HasMember("result"))
			{
				MinerLogger::write(std::string("error: ") + body["result"].GetString(), TextType::Error);
				confirmation.errorCode = SubmitResponse::Error;
			}
			else
			{
				MinerLogger::write(response, TextType::Error);
				confirmation.errorCode = SubmitResponse::Error;
			}
		}
		else
		{
			confirmation.errorCode = SubmitResponse::Error;
		}
	}

	return confirmation;
}

bool Burst::NonceResponse::isDataThere()
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
	static std::string empty = "";

	if (index >= tokens_.size())
		return empty;

	return tokens_[index];
}
