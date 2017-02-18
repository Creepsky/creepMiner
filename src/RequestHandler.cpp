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
#include <Poco/NestedDiagnosticContext.h>
#include "Request.hpp"
#include "MinerConfig.hpp"
#include <Poco/Net/HTTPClientSession.h>
#include "PlotSizes.hpp"
#include <Poco/Logger.h>
#include <Poco/Base64Decoder.h>
#include <Poco/StreamCopier.h>
#include <Poco/StringTokenizer.h>

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

	try
	{
		Poco::FileInputStream file{ "public/index.html", std::ios::in };
		std::string str(std::istreambuf_iterator<char>{file}, {});
		variables_->inject(str);

		out << str;
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::server, "Could not open public/index.html!");
		log_exception(MinerLogger::server, exc);
	}
}

Burst::ShutdownHandler::ShutdownHandler(Miner& miner, MinerServer& server)
	: miner_{&miner}, server_{&server}
{}

void Burst::ShutdownHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	poco_ndc("ShutdownHandler::handleRequest");

	auto credentialsOk = false;

	if (request.hasCredentials())
	{
		std::string scheme, authInfo;
		request.getCredentials(scheme, authInfo);

		// credentials are base64 encoded
		std::stringstream encoded(authInfo);
		std::stringstream decoded;
		std::string credentials, user = "", password;
		//
		Poco::Base64Decoder base64(encoded);
		Poco::StreamCopier::copyStream(base64, decoded);
		//
		credentials = decoded.str();

		Poco::StringTokenizer tokenizer{credentials, ":"};

		if (tokenizer.count() == 2)
		{
			user = tokenizer[0];
			password = tokenizer[1];

			credentialsOk = 
				check_HMAC_SHA1(user, MinerConfig::getConfig().getServerUser(), MinerConfig::WebserverPassphrase) &&
				check_HMAC_SHA1(password, MinerConfig::getConfig().getServerPass(), MinerConfig::WebserverPassphrase);
		}

		log_information(MinerLogger::server, "%s request to shutdown the miner.\n"
			"\tfrom: %s\n"
			"\tuser: %s",
			(credentialsOk ? std::string("Authorized") : std::string("Unauthorized")),
			request.clientAddress().toString(),
			user);
	}
	
	if (!credentialsOk)
	{
		response.requireAuthentication("creepMiner");
		response.send();
		return;
	}

	log_system(MinerLogger::server, "Shutting down miner...");

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

	log_system(MinerLogger::server, "Goodbye");
}

Burst::AssetHandler::AssetHandler(const TemplateVariables& variables)
	: variables_{&variables}
{}

void Burst::AssetHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	try
	{
		const auto relativePath = "public/" + request.getURI();
		Poco::Path path{ relativePath };
		Poco::FileInputStream file{relativePath, std::ios::in};
		std::string str(std::istreambuf_iterator<char>{file}, {});

		std::string mimeType = "text/plain";

		auto ext = path.getExtension();

		if (ext == "css")
			mimeType = "text/css";
		else if (ext == "js")
			mimeType = "text/javascript";
		else if (ext == "png")
			mimeType = "image/png";

		response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
		response.setChunkedTransferEncoding(true);
		response.setContentType(mimeType);
		auto& out = response.send();

		out << str;
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::server, "Webserver could not open 'public/%s'!", request.getURI());
		log_exception(MinerLogger::server, exc);
	}
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

Burst::MiningInfoHandler::MiningInfoHandler(Miner& miner)
	: miner_{&miner}
{ }

void Burst::MiningInfoHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	poco_ndc(MiningInfoHandler::handleRequest);

	Poco::JSON::Object json;
	json.set("baseTarget", miner_->getBaseTarget());
	json.set("generationSignature", miner_->getGensigStr());
	json.set("targetDeadline", miner_->getTargetDeadline());
	json.set("height", miner_->getBlockheight());

	try
	{
		std::stringstream ss;
		json.stringify(ss);
		auto jsonStr = ss.str();

		response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
		response.setContentLength(jsonStr.size());

		auto& output = response.send();
		output << jsonStr;
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::server, "Webserver could not send mining info! %s", exc.displayText());
		log_current_stackframe(MinerLogger::server);
	}
}

Burst::SubmitNonceHandler::SubmitNonceHandler(Miner& miner)
	: miner_{&miner}
{ }

void Burst::SubmitNonceHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	poco_ndc(SubmitNonceHandler::handleRequest);

	try
	{
		try
		{
			if (request.has("X-PlotsHash"))
				PlotSizes::set(request.get("X-PlotsHash"), Poco::NumberParser::parseUnsigned64(request.get("X-Capacity")));
		}
		catch (Poco::Exception&)
		{
			log_debug(MinerLogger::server, "The X-PlotsHash from the other miner is not a number! %s", request.get("X-PlotsHash"));
		}

		// sum up the capacity
		request.set("X-Capacity", std::to_string(PlotSizes::getTotal()));

		// forward the request to the pool
		ForwardHandler{MinerConfig::getConfig().createSession(HostType::Pool)}.handleRequest(request, response);
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::server, "Could not forward nonce! %s", exc.displayText());
		log_current_stackframe(MinerLogger::server);
	}
}

Burst::ForwardHandler::ForwardHandler(std::unique_ptr<Poco::Net::HTTPClientSession> session)
	: session_{std::move(session)}
{ }

void Burst::ForwardHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	if (session_ == nullptr)
		return;

	try
	{
		Request forwardRequest{std::move(session_)};
		auto forwardResponse = forwardRequest.send(request);

		std::string data;

		if (forwardResponse.receive(data))
		{
			response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
			response.setContentLength(data.size());

			auto& responseStream = response.send();
			responseStream << data;
		}
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::server, "Could not forward request to wallet!\n%s\n%s", exc.displayText(), request.getURI());
		log_current_stackframe(MinerLogger::server);
	}
}
