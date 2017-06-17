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
#include <Poco/Net/HTMLForm.h>

namespace Burst
{
	namespace RequestHandlerHelper
	{
		bool sendIndexContent(const TemplateVariables& indexVariables, const TemplateVariables& contentVariables,
			const std::string& contentPage, std::string& output)
		{
			Poco::FileInputStream fileIndex, fileContent;
			output.clear();

			try
			{
				fileIndex.open("public/index.html", std::ios::in);
				output = std::string{ std::istreambuf_iterator<char>{fileIndex}, {} };
				indexVariables.inject(output);
			}
			catch (Poco::Exception& exc)
			{
				log_error(MinerLogger::server, "Could not open public/index.html!");
				log_exception(MinerLogger::server, exc);

				if (fileIndex)
					fileIndex.close();

				return false;
			}

			try
			{
				fileContent.open(contentPage, std::ios::in);
				std::string strContent(std::istreambuf_iterator<char>{fileContent}, {});

				TemplateVariables contentFramework;
				contentFramework.variables.emplace("content", [&strContent]() { return strContent; });

				contentFramework.inject(output);
				contentVariables.inject(output);
			}
			catch (Poco::Exception& exc)
			{
				log_error(MinerLogger::server, "Could not open '%s'!", contentPage);
				log_exception(MinerLogger::server, exc);

				if (fileContent)
					fileContent.close();

				return false;
			}

			fileIndex.close();
			fileContent.close();

			return true;
		}

		bool checkCredentials(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
		{
			auto credentialsOk = false;

			if (MinerConfig::getConfig().getServerUser().empty() &&
				MinerConfig::getConfig().getServerPass().empty())
				return true;

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
				return false;
			}

			return true;
		}
	}
}

void Burst::TemplateVariables::inject(std::string& source) const
{
	for (const auto& var : variables)
		Poco::replaceInPlace(source, "%" + var.first + "%", var.second());
}

Burst::NotFoundHandler::NotFoundHandler(const TemplateVariables& variables)
	: variables_(&variables)
{}

void Burst::NotFoundHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	TemplateVariables contentVariables;
	std::string output;

	contentVariables.variables.emplace("includes", []() { return ""; });

	if (RequestHandlerHelper::sendIndexContent(*variables_, contentVariables, "public/404.html", output))
	{
		response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
		response.setChunkedTransferEncoding(true);
		auto& out = response.send();
		out << output;
	}
	else
	{
		response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
		response.send();
	}
}

Burst::RootHandler::RootHandler(const TemplateVariables& variables)
	: variables_{&variables}
{}

void Burst::RootHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	std::string output;
	TemplateVariables contentVariables;
	contentVariables.variables.emplace("includes", []() { return "<script src='js/miner.js'></script>"; });

	if (RequestHandlerHelper::sendIndexContent(*variables_, contentVariables, "public/block.html", output))
	{
		response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
		response.setChunkedTransferEncoding(true);
		auto& out = response.send();
		out << output;
	}
	else
	{
		response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
		response.send();
	}
}

Burst::PlotfilesHandler::PlotfilesHandler(const TemplateVariables& variables)
	: variables_{ &variables }
{}

void Burst::PlotfilesHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	std::string output;
	std::string plotFiles;

	auto jsonPlotDirs = createJsonPlotDirs();

	std::stringstream sstr;
	jsonPlotDirs.stringify(sstr);

	TemplateVariables contentVariables;

	contentVariables.variables.emplace("includes", [&sstr]()
	{
		std::stringstream sstrContent;
		sstrContent << "<script src='js/plotfiles.js'></script>";
		sstrContent << std::endl;
		sstrContent << "<script>var plotdirs = " << sstr.str() << ";</script>";
		return sstrContent.str();
	});

	if (RequestHandlerHelper::sendIndexContent(*variables_, contentVariables, "public/plotfiles.html", output))
	{
		response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
		response.setChunkedTransferEncoding(true);
		auto& out = response.send();
		out << output;
	}
	else
	{
		response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
		response.send();
	}
}

Burst::RescanPlotfilesHandler::RescanPlotfilesHandler(MinerServer& server)
	: server_{&server}
{}

void Burst::RescanPlotfilesHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	if (!RequestHandlerHelper::checkCredentials(request, response))
		return;

	log_information(MinerLogger::server, "Got request for rescanning the plotdirs...");

	MinerConfig::getConfig().rescanPlotfiles();

	// we send the new settings (size could be changed)
	server_->sendToWebsockets(createJsonConfig());

	// then we send all plot dirs and files
	server_->sendToWebsockets(createJsonPlotDirsRescan());

	// respond
	response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
	response.setContentLength(0);
	response.send();
}

Burst::ShutdownHandler::ShutdownHandler(Miner& miner, MinerServer& server)
	: miner_{&miner}, server_{&server}
{}

void Burst::ShutdownHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	poco_ndc("ShutdownHandler::handleRequest");

	if (!RequestHandlerHelper::checkCredentials(request, response))
		return;

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
	json.set("baseTarget", std::to_string(miner_->getBaseTarget()));
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
		std::string plotsHash = "";

		try
		{
			if (request.has(X_PlotsHash))
			{
				auto plotsHashEncoded = request.get(X_PlotsHash);
				Poco::URI::decode(plotsHashEncoded, plotsHash);
				PlotSizes::set(plotsHash, Poco::NumberParser::parseUnsigned64(request.get(X_Capacity)));
			}
		}
		catch (Poco::Exception&)
		{
			log_debug(MinerLogger::server, "The X-PlotsHash from the other miner is not a number! %s", request.get(X_PlotsHash));
		}

		Poco::URI uri{ request.getURI() };

		Poco::UInt64 accountId = 0;
		Poco::UInt64 nonce = 0;
		Poco::UInt64 deadline = 0;
		std::string plotfile = "";

		for (const auto& param : uri.getQueryParameters())
		{
			if (param.first == "accountId")
				accountId = Poco::NumberParser::parseUnsigned64(param.second);
			else if (param.first == "nonce")
				nonce = Poco::NumberParser::parseUnsigned64(param.second);
		}

		if (request.has(X_Plotfile))
		{
			auto plotfileEncoded = request.get(X_Plotfile);
			Poco::URI::decode(plotfileEncoded, plotfile);
		}

		if (request.has(X_Deadline))
			deadline = Poco::NumberParser::parseUnsigned64(request.get(X_Deadline));

		auto account = miner_->getAccount(accountId);

		if (account == nullptr)
			account = std::make_shared<Account>(accountId);

		if (plotfile.empty())
			plotfile = !plotsHash.empty() ? plotsHash : "unknown";

		log_information(MinerLogger::server, "Got nonce forward request (%s)\n"
			"\tnonce: %Lu\n"
			"\taccount: %s\n"
			"\tin: %s",
			deadlineFormat(deadline), nonce,
			account->getName().empty() ? account->getAddress() : account->getName(),
			plotfile
		);

		if (accountId != 0 && nonce != 0 && deadline != 0)
		{
			auto future = miner_->submitNonceAsync(std::make_tuple(nonce, accountId, deadline,
				miner_->getBlockheight(), plotfile));

			future.wait();

			auto forwardResult = future.data();

			response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
			response.setChunkedTransferEncoding(true);
			auto& responseData = response.send();

			responseData << forwardResult.json;
		}
		else
		{
			// sum up the capacity
			request.set("X-Capacity", std::to_string(PlotSizes::getTotal()));
			
			// forward the request to the pool
			ForwardHandler{MinerConfig::getConfig().createSession(HostType::Pool)}.handleRequest(request, response);
		}
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

	log_information(MinerLogger::server, "Forwarding request\n\t%s", request.getURI());

	try
	{
		Request forwardRequest{std::move(session_)};
		auto forwardResponse = forwardRequest.send(request);

		log_debug(MinerLogger::server, "Request forwarded, waiting for response...");

		std::string data;

		if (forwardResponse.receive(data))
		{
			log_debug(MinerLogger::server, "Got response, sending back...\n\t%s", data);

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

Burst::SettingsHandler::SettingsHandler(const TemplateVariables& variables, MinerServer& server)
	: variables_(&variables), server_(&server)
{}

void Burst::SettingsHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	TemplateVariables contentVariables;
	std::string output;

	contentVariables.variables.emplace("includes", []() { return "<script src='js/settings.js'></script>"; });

	if (RequestHandlerHelper::sendIndexContent(*variables_, contentVariables, "public/settings.html", output))
	{
		response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
		response.setChunkedTransferEncoding(true);
		auto& out = response.send();
		out << output;
	}
	else
	{
		response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
		response.send();
	}
}

Burst::RedirectHandler::RedirectHandler(std::string redirect_url)
	: redirect_url_(std::move(redirect_url))
{}

void Burst::RedirectHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	response.redirect(redirect_url_);
}

Burst::SettingsChangeHandler::SettingsChangeHandler(MinerServer& server)
	: server_(&server)
{}

void Burst::SettingsChangeHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	if (request.getMethod() == "POST")
	{
		Poco::Net::HTMLForm post_body(request, request.stream());
		
		for (auto& key_val : post_body)
		{
			const auto& key = key_val.first;
			const auto& value = key_val.second;

			if (key == "mining-info-url")
				MinerConfig::getConfig().setUrl(value, HostType::MiningInfo);
			else if (key == "submission-url")
				MinerConfig::getConfig().setUrl(value, HostType::Pool);
			else if (key == "wallet-url")
				MinerConfig::getConfig().setUrl(value, HostType::Wallet);
		}
	}

	RedirectHandler redirect("/settings");
	redirect.handleRequest(request, response);
}
