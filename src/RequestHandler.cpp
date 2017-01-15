#include "RequestHandler.hpp"
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/FileStream.h>
#include "MinerLogger.hpp"
#include "MinerUtil.hpp"
#include <Poco/JSON/Object.h>
#include "MinerServer.hpp"
#include "Miner.hpp"

void Burst::TemplateVariables::inject(std::string& source) const
{
	for (const auto& var : variables)
		Poco::replaceInPlace(source, "%" + var.first + "%", var.second());
}

void Burst::NotFoundHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
	response.send();
}

Burst::RootHandler::RootHandler(const TemplateVariables& variables)
	: variables_{&variables}
{}

void Burst::RootHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
	response.setChunkedTransferEncoding(true);
	auto& out = response.send();

	Poco::FileInputStream file{ "public/index.html", std::ios::in };
	std::string str(std::istreambuf_iterator<char>{file}, {});
	
	variables_->inject(str);

	out << str;
}

Burst::ShutdownHandler::ShutdownHandler(Miner& miner, MinerServer& server)
	: miner_{&miner}, server_{&server}
{}

void Burst::ShutdownHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	MinerLogger::write("Shutting down miner...", TextType::System);

	// first we shut down the miner
	miner_->stop();

	// then we send a response to the client
	std::stringstream ss;
	createJsonShutdown().stringify(ss);
	auto str = ss.str();

	response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
	response.setContentLength(str.size());
	auto& output = response.send();
	
	output << ss.str();
	output.flush();

	// finally we shut down the server
	server_->stop();

	MinerLogger::write("Goodbye", TextType::System);
}

Burst::AssetHandler::AssetHandler(const TemplateVariables& variables)
	: variables_{&variables}
{}

void Burst::AssetHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	Poco::FileInputStream file{"public/" + request.getURI(), std::ios::in};
	std::string str(std::istreambuf_iterator<char>{file}, {});

	response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
	response.setChunkedTransferEncoding(true);
	auto& out = response.send();

	out << str;
}

void Burst::BadRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	response.setContentLength(0);
	response.send();
}

Burst::WebSocketHandler::WebSocketHandler(MinerServer* server)
	: server_{server}
{}

void Burst::WebSocketHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	try
	{
		if (server_ == nullptr)
			return;

		server_->addWebsocket(std::make_unique<Poco::Net::WebSocket>(request, response));
	}
	catch (...)
	{}
}
