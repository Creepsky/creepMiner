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
