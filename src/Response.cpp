#include "Response.hpp"
#include "Socket.hpp"
#include "MinerUtil.h"
#include "MinerConfig.h"
#include "rapidjson/document.h"
#include "MinerLogger.h"

Burst::Response::Response(std::unique_ptr<Socket> socket)
	: socket_(std::move(socket))
{}

Burst::Response::~Response()
{
	if (socket_ != nullptr)
		socket_->disconnect();
}

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

Burst::NonceResponse::NonceResponse(std::unique_ptr<Socket> socket)
	: response_(std::move(socket))
{}

bool Burst::NonceResponse::canReceive() const
{
	return response_.canReceive();
}

Burst::NonceConfirmation Burst::NonceResponse::getConfirmation() const
{
	std::string response;
	NonceConfirmation confirmation{ 0, SubmitResponse::None };

	if (response_.receive(response))
	{
		HttpResponse httpResponse(response);
		rapidjson::Document body;
		body.Parse<0>(httpResponse.getMessage().c_str());

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
			else
			{
				MinerLogger::write(response, TextType::Error);
				confirmation.errorCode = SubmitResponse::Error;
			}
		}
		else
			confirmation.errorCode = SubmitResponse::None;
	}

	return confirmation;
}

std::unique_ptr<Burst::Socket> Burst::NonceResponse::close()
{
	return response_.close();
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
