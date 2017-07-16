// ==========================================================================
// 
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2017 Creepsky (creepsky@gmail.com)
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

#include "RequestHandler.hpp"
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/FileStream.h>
#include "logging/MinerLogger.hpp"
#include "MinerUtil.hpp"
#include <Poco/JSON/Object.h>
#include "MinerServer.hpp"
#include "mining/Miner.hpp"
#include <Poco/NestedDiagnosticContext.h>
#include "network/Request.hpp"
#include "mining/MinerConfig.hpp"
#include <Poco/Net/HTTPClientSession.h>
#include "plots/PlotSizes.hpp"
#include <Poco/Logger.h>
#include <Poco/Base64Decoder.h>
#include <Poco/StreamCopier.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Net/HTMLForm.h>

Burst::TemplateVariables::TemplateVariables(std::unordered_map<std::string, Variable> variables)
	: variables(variables)
{}

void Burst::TemplateVariables::inject(std::string& source) const
{
	for (const auto& var : variables)
		Poco::replaceInPlace(source, "%" + var.first + "%", var.second());
}

Burst::TemplateVariables Burst::TemplateVariables::operator+(const TemplateVariables& rhs)
{
	TemplateVariables combined(rhs.variables);
	combined.variables.insert(variables.begin(), variables.end());
	return combined;
}

Burst::RequestHandler::LambdaRequestHandler::LambdaRequestHandler(Lambda lambda)
	: lambda_(std::move(lambda))
{}

void Burst::RequestHandler::LambdaRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	lambda_(request, response);
}

void Burst::RequestHandler::loadTemplate(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
	const std::string& templatePage, const std::string& contentPage, TemplateVariables& variables)
{
	Poco::FileInputStream fileIndex, fileContent;
	std::string output;

	// open the index page 
	try
	{
		// open the template file 
		fileIndex.open("public/" + templatePage, std::ios::in);
		// read the content of the file and load it into a string 
		output = std::string{ std::istreambuf_iterator<char>{fileIndex},{} };
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::server, "Could not open public/index.html!");
		log_exception(MinerLogger::server, exc);

		if (fileIndex)
			fileIndex.close();

		return notFound(request, response);
	}

	// open the embedded page 
	try
	{
		// load it into a string 
		fileContent.open("public/" + contentPage, std::ios::in);
		std::string strContent(std::istreambuf_iterator<char>{fileContent}, {});

		// replace variables inside it 
		TemplateVariables contentFramework;
		contentFramework.variables.emplace("content", [&strContent]() { return strContent; });

		contentFramework.inject(output);
		variables.inject(output);
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::server, "Could not open 'public/%s'!", contentPage);
		log_exception(MinerLogger::server, exc);

		if (fileContent)
			fileContent.close();

		return notFound(request, response);
	}

	// not necessary, but good style 
	fileIndex.close();
	fileContent.close();

	response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
	response.setChunkedTransferEncoding(true);
	
	auto& responseStream = response.send();
	responseStream << output << std::flush;
}

bool Burst::RequestHandler::loadAssetByPath(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, const std::string& path)
{
	try
	{
		Poco::Path pathObject{ "public/" + path };
		Poco::FileInputStream file{ pathObject.toString(), std::ios::in };
		std::string str(std::istreambuf_iterator<char>{file}, {});

		std::string mimeType = "text/plain";

		auto ext = pathObject.getExtension();

		if (ext == "css")
			mimeType = "text/css";
		else if (ext == "js")
			mimeType = "text/javascript";
		else if (ext == "png")
			mimeType = "image/png";
		else if (ext == "html")
			mimeType = "text/html; charset=utf-8";

		response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
		response.setChunkedTransferEncoding(true);
		response.setContentType(mimeType);
		auto& out = response.send();

		out << str;
		return true;
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::server, "Webserver could not open 'public/%s'!", request.getURI());
		log_exception(MinerLogger::server, exc);
		return false;
	}
}

bool Burst::RequestHandler::loadAsset(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	return loadAssetByPath(request, response, request.getURI());
}

void Burst::RequestHandler::redirect(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
	const std::string& redirectUri)
{
	response.redirect(redirectUri);
}

void Burst::RequestHandler::forward(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, HostType hostType)
{
	auto session = MinerConfig::getConfig().createSession(hostType);

	if (session == nullptr)
		return;

	log_information(MinerLogger::server, "Forwarding request\n\t%s", request.getURI());

	try
	{
		// HTTPRequest has a private copy constructor, so we need to copy
		// all fields one by one
		Poco::Net::HTTPRequest forwardingRequest;
		forwardingRequest.setURI(request.getURI());
		forwardingRequest.setMethod(request.getMethod());
		forwardingRequest.setContentLength(request.getContentLength());
		forwardingRequest.setTransferEncoding(request.getTransferEncoding());
		forwardingRequest.setChunkedTransferEncoding(request.getChunkedTransferEncoding());
		forwardingRequest.setKeepAlive(request.getKeepAlive());
		forwardingRequest.setVersion(request.getVersion());

		Request forwardRequest{ std::move(session) };
		auto forwardResponse = forwardRequest.send(forwardingRequest);

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

void Burst::RequestHandler::badRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	response.send();
}

void Burst::RequestHandler::rescanPlotfiles(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
	Burst::MinerServer& server)
{
	// first check the credentials
	if (!checkCredentials(request, response))
		return;

	log_information(MinerLogger::server, "Got request for rescanning the plotdirs...");

	// rescan the plot files...
	MinerConfig::getConfig().rescanPlotfiles();

	// we send the new settings (size could be changed)
	server.sendToWebsockets(createJsonConfig());

	// then we send all plot dirs and files
	server.sendToWebsockets(createJsonPlotDirsRescan());

	// respond to the sender
	response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
	response.setContentLength(0);
	response.send();
}

void Burst::RequestHandler::addWebsocket(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
                                                  Burst::MinerServer& server)
{
	try
	{
		server.addWebsocket(std::make_unique<Poco::Net::WebSocket>(request, response));
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::server, "Could not accept incoming websocket request.");
		log_exception(MinerLogger::server, exc);
	}
}

bool Burst::RequestHandler::checkCredentials(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	auto credentialsOk = false;

	// if there are no credentials in the config, user dont need to
	// enter them
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
		std::string user = "";
		//
		Poco::Base64Decoder base64(encoded);
		Poco::StreamCopier::copyStream(base64, decoded);
		// the whole credentials string (<user>:<password>)
		auto credentials = decoded.str();

		Poco::StringTokenizer tokenizer{ credentials, ":" };

		if (tokenizer.count() == 2)
		{
			user = tokenizer[0];
			auto password = tokenizer[1];

			// compare given credentials with the credentials inside the config
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

	// not authenticated, request again
	if (!credentialsOk)
	{
		response.requireAuthentication("creepMiner");
		response.send();
		return false;
	}

	// authenticated
	return true;
}

void Burst::RequestHandler::shutdown(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, Miner& miner, MinerServer& server)
{
	poco_ndc("ShutdownHandler::handleRequest");

	if (!checkCredentials(request, response))
		return;

	log_system(MinerLogger::server, "Shutting down miner...");

	// first we shut down the miner
	miner.stop();

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
	server.stop();

	log_system(MinerLogger::server, "Goodbye");
}

void Burst::RequestHandler::submitNonce(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, MinerServer& server, Miner& miner)
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

				// send new settings to websockets
				server.sendToWebsockets(createJsonConfig());
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

		auto account = miner.getAccount(accountId);

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
			auto future = miner.submitNonceAsync(std::make_tuple(nonce, accountId, deadline,
				miner.getBlockheight(), plotfile));

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
			forward(request, response, HostType::Pool);
		}
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::server, "Could not forward nonce! %s", exc.displayText());
		log_current_stackframe(MinerLogger::server);
	}
}

void Burst::RequestHandler::miningInfo(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, Miner& miner)
{
	poco_ndc(MiningInfoHandler::handleRequest);

	Poco::JSON::Object json;
	json.set("baseTarget", std::to_string(miner.getBaseTarget()));
	json.set("generationSignature", miner.getGensigStr());
	json.set("targetDeadline", miner.getTargetDeadline());
	json.set("height", miner.getBlockheight());

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

void Burst::RequestHandler::changeSettings(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
	Miner& miner)
{
	if (request.getMethod() == "POST")
	{
		Poco::Net::HTMLForm post_body(request, request.stream());

		for (auto& key_val : post_body)
		{
			const auto& key = key_val.first;
			const auto& value = key_val.second;

			using np = Poco::NumberParser;

			try
			{
				if (key == "mining-info-url")
					MinerConfig::getConfig().setUrl(value, HostType::MiningInfo);
				else if (key == "submission-url")
					MinerConfig::getConfig().setUrl(value, HostType::Pool);
				else if (key == "wallet-url")
					MinerConfig::getConfig().setUrl(value, HostType::Wallet);
				else if (key == "intensity")
					miner.setMiningIntensity(np::parseUnsigned(value));
				else if (key == "buffer-size")
					Miner::setMaxBufferSize(np::parseUnsigned64(value));
				else if (key == "plot-readers")
					miner.setMaxPlotReader(np::parseUnsigned(value));
				else if (key == "submission-max-retry")
					MinerConfig::getConfig().setMaxSubmissionRetry(np::parseUnsigned64(value));
				else if (key == "target-deadline")
					MinerConfig::getConfig().setTargetDeadline(value);
				else if (key == "timeout")
					MinerConfig::getConfig().setTimeout(static_cast<float>(np::parseFloat(value)));
				else if (key == "log-dir")
					MinerConfig::getConfig().setLogDir(value);
				else if (Poco::icompare(key, 4, std::string("cmb_")) == 0)
				{
					auto logger_name = Poco::replace(key, "cmb_", "");
					auto logger_priority = static_cast<Poco::Message::Priority>(np::parse(value));
					MinerLogger::setChannelPriority(logger_name, logger_priority);
				}
				else
					log_warning(MinerLogger::server, "unknown settings-key: %s", key);
			}
			catch (Poco::Exception& exc)
			{
				log_exception(MinerLogger::server, exc);
				log_current_stackframe(MinerLogger::server);
			}
		}

		log_system(MinerLogger::config, "Settings changed...");
		MinerConfig::getConfig().printConsole();

		if (MinerConfig::getConfig().save())
			log_success(MinerLogger::config, "Saved new settings!");
		else
			log_error(MinerLogger::config, "Could not save new settings!");
	}

	redirect(request, response, "/settings");
}

void Burst::RequestHandler::changePlotDirs(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
	MinerServer& server, bool remove)
{
	poco_ndc("PlotDirHandler::handleRequest");

	std::stringstream sstream;
	Poco::StreamCopier::copyStream(request.stream(), sstream);
	auto path = sstream.str();

	if (path.empty())
		return;

	log_information(MinerLogger::server, "Got request for changing the plotdirs...");

	bool success;

	if (remove)
		success = MinerConfig::getConfig().removePlotDir(path);
	else
		success = MinerConfig::getConfig().addPlotDir(path);

	using hs = Poco::Net::HTTPResponse::HTTPStatus;

	server.sendToWebsockets(createJsonPlotDirsRescan());

	response.setStatus(success ? hs::HTTP_OK : hs::HTTP_BAD_REQUEST);
	response.setContentLength(0);
	response.send();
}

void Burst::RequestHandler::notFound(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
	response.send();
}
