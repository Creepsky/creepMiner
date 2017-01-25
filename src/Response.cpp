#include <Poco/JSON/Parser.h>
#include <Poco/NestedDiagnosticContext.h>
#include "Response.hpp"
#include "Socket.hpp"
#include "MinerUtil.hpp"
#include "MinerConfig.hpp"
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
	poco_ndc(Response::receive);
	
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
		MinerLogger::writeStackframe(std::string("error on receiving response: ") + exc.what());
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
	poco_ndc(NonceResponse::getConfirmation);
	
	std::string response;
	NonceConfirmation confirmation{ 0, SubmitResponse::None };

	if (response_.receive(response))
	{
		try
		{
			Poco::JSON::Parser parser;
			auto root = parser.parse(response).extract<Poco::JSON::Object::Ptr>();

			if (root->has("deadline"))
			{
				confirmation.deadline = root->get("deadline");
				confirmation.errorCode = SubmitResponse::Submitted;
			}
			else if (root->has("errorCode"))
			{
				MinerLogger::write(std::string("error: ") + root->get("errorDescription").convert<std::string>(),
								   TextType::Error);
				// we send true so we dont send it again and again
				confirmation.errorCode = SubmitResponse::Error;
			}
			else if (root->has("result"))
			{
				MinerLogger::write(std::string("error: ") + root->get("result").convert<std::string>(), TextType::Error);
				confirmation.errorCode = SubmitResponse::Error;
			}
			else
			{
				MinerLogger::write(response, TextType::Error);
				confirmation.errorCode = SubmitResponse::Error;
			}
		}
		catch (Poco::Exception& exc)
		{
			std::vector<std::string> lines = {
				"error while waiting for confirmation!",
				exc.what()
			};
			
			MinerLogger::writeStackframe(lines);
			
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
