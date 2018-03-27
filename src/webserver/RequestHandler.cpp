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

#include "RequestHandler.hpp"
#include <fstream>
#include <Poco/Net/WebSocket.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/FileStream.h>
#include "logging/MinerLogger.hpp"
#include "MinerUtil.hpp"
#include <Poco/JSON/Object.h>
#include "MinerServer.hpp"
#include "mining/Miner.hpp"
#include <Poco/NestedDiagnosticContext.h>
#include "network/Request.hpp"
#include "mining/MinerConfig.hpp"
#include "plots/PlotSizes.hpp"
#include <Poco/Logger.h>
#include <Poco/Base64Decoder.h>
#include <Poco/StreamCopier.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Net/HTMLForm.h>
#include "plots/PlotGenerator.hpp"
#include <regex>
#include <utility>
#include <Poco/Net/NetException.h>
#include <Poco/Delegate.h>

const std::string cookieUserName = "creepminer-webserver-user";
const std::string cookiePassName = "creepminer-webserver-pass";

Burst::TemplateVariables::TemplateVariables(std::unordered_map<std::string, Variable> variables)
	: variables(std::move(variables))
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

Burst::RequestHandler::WebsocketRequestHandler::WebsocketRequestHandler(MinerServer& server, MinerData& data)
	: server_{server}, data_{data}
{
	server_.newDataEvent += Poco::delegate(this, &WebsocketRequestHandler::onNewData);
}

Burst::RequestHandler::WebsocketRequestHandler::~WebsocketRequestHandler()
{
	server_.newDataEvent -= Poco::delegate(this, &WebsocketRequestHandler::onNewData);
}

void Burst::RequestHandler::WebsocketRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request,
	Poco::Net::HTTPServerResponse& response)
{
	using namespace Poco;
	using namespace Net;

	try
	{
		WebSocket ws(request, response);
		log_debug(MinerLogger::server, "WebSocket connection established.");

		try
		{
			// send the config
			std::stringstream sstream;
			createJsonConfig().stringify(sstream);
			const auto configString = sstream.str();
			ws.sendFrame(configString.data(), static_cast<int>(configString.size()));

			// send the plot dir data
			sstream.str("");
			createJsonPlotDirsRescan().stringify(sstream);
			const auto plotDirString = sstream.str();
			ws.sendFrame(plotDirString.data(), static_cast<int>(plotDirString.size()));

			data_.getBlockData()->forEntries([&](const JSON::Object& json)
			{
				sstream.str("");
				JSON::Stringifier::condense(json, sstream);
				const auto jsonString = sstream.str();
				ws.sendFrame(jsonString.data(), static_cast<int>(jsonString.size()));
				return true;
			});
		}
		catch (Exception& exc)
		{
			log_exception(MinerLogger::server, exc);
			return;
		}
		
		char buffer[1024];
		int flags;
		auto close = false;
		ws.setReceiveTimeout(Timespan{10000}); // 10 ms
		do
		{
			try
			{
				close = ws.receiveFrame(buffer, sizeof buffer, flags) == 0;
			}
			catch (TimeoutException&)
			{
				// do nothing
			}
			catch (Exception& exc)
			{
				log_exception(MinerLogger::server, exc);
				close = true;
			}

			if (!close)
			{
				std::lock_guard<std::mutex> lock(mutex_);
				if (!queue_.empty())
				{
					auto data = queue_.front();
					queue_.pop_front();
					const auto s = ws.sendFrame(data.data(), static_cast<int>(data.size()));
					if (s != static_cast<int>(data.size()))
						log_warning(MinerLogger::server, "Could not fully send: %s", data);
				}
			}
		}
		while (!close && (flags & WebSocket::FRAME_OP_BITMASK) != WebSocket::FRAME_OP_CLOSE);
		ws.shutdown();
		log_debug(MinerLogger::server, "WebSocket connection closed.");
	}
	catch (WebSocketException& exc)
	{
		log_exception(MinerLogger::server, exc);
	}
}

void Burst::RequestHandler::WebsocketRequestHandler::onNewData(std::string& data)
{
	std::lock_guard<std::mutex> lock(mutex_);
	queue_.emplace_back(data);
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

void Burst::RequestHandler::loadSecuredTemplate(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
	const std::string& templatePage, const std::string& contentPage, TemplateVariables& variables)
{
	if (!checkCredentials(request, response))
		return;

	loadTemplate(request, response, templatePage, contentPage, variables);
}

bool Burst::RequestHandler::loadAssetByPath(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, const std::string& path)
{
	try
	{
		Poco::Path pathObject{ "public/" + path };
		Poco::FileInputStream file{ pathObject.toString(), std::ios::in };
		const std::string str(std::istreambuf_iterator<char>{file}, {});

		std::string mimeType = "text/plain";

		const auto ext = pathObject.getExtension();

		if (ext == "css")
			mimeType = "text/css";
		else if (ext == "js")
			mimeType = "text/javascript";
		else if (ext == "png")
			mimeType = "image/png";
		//else if (ext == "html")
		//	mimeType = "text/html; charset=utf-8";

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

namespace Burst
{
	void clearAuthCookies(Poco::Net::HTTPServerResponse& response)
	{
		Poco::Net::HTTPCookie cookieUser(cookieUserName);
		Poco::Net::HTTPCookie cookiePass(cookiePassName);

		cookieUser.setMaxAge(0);
		cookiePass.setMaxAge(0);

		response.addCookie(cookieUser);
		response.addCookie(cookiePass);
	}
}

bool Burst::RequestHandler::login(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	Poco::Net::HTMLForm post_body(request, request.stream());

	const std::string defaultCredential;
	const auto& plainUserPost = post_body.get(cookieUserName, defaultCredential);
	const auto& plainPassPost = post_body.get(cookiePassName, defaultCredential);

	const auto credentialsOk =
		check_HMAC_SHA1(plainUserPost, MinerConfig::getConfig().getServerUser(), MinerConfig::webserverUserPassphrase) &&
		check_HMAC_SHA1(plainPassPost, MinerConfig::getConfig().getServerPass(), MinerConfig::webserverPassPassphrase);

	// save the hashed username and password in a clientside cookie
	if (credentialsOk)
	{
		response.addCookie({ cookieUserName, hash_HMAC_SHA1(plainUserPost, MinerConfig::webserverUserPassphrase) });
		response.addCookie({ cookiePassName, hash_HMAC_SHA1(plainPassPost, MinerConfig::webserverPassPassphrase) });
	}

	return credentialsOk;
}

void Burst::RequestHandler::logout(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	clearAuthCookies(response);
	redirect(request, response, "/");
}

bool Burst::RequestHandler::isLoggedIn(Poco::Net::HTTPServerRequest& request)
{
	auto credentialsOk = false;

	// if there are no credentials in the config, user dont need to
	// enter them
	if (MinerConfig::getConfig().getServerUser().empty() &&
		MinerConfig::getConfig().getServerPass().empty())
		return true;

	// the user is already logged in
	// the credentials are in the cookie
	Poco::Net::NameValueCollection cookies;
	request.getCookies(cookies);
	//
	const std::string emptyValue;

	auto hashedUserCookie = cookies.get(cookieUserName, emptyValue);
	auto hashedPassCookie = cookies.get(cookiePassName, emptyValue);

	if (!hashedUserCookie.empty() || !hashedPassCookie.empty())
		credentialsOk =
			hashedUserCookie == MinerConfig::getConfig().getServerUser() &&
			hashedPassCookie == MinerConfig::getConfig().getServerPass();
	
	return credentialsOk;
}

void Burst::RequestHandler::redirect(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
	const std::string& redirectUri)
{
	response.redirect(redirectUri);
}

void Burst::RequestHandler::forward(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, HostType hostType)
{
	auto forward = MinerConfig::getConfig().isForwardingEverything();

	if (!forward)
	{
		const Poco::URI uri(request.getURI());
		const auto pathAndQuery = uri.getPathAndQuery();

		const auto& whitelist = MinerConfig::getConfig().getForwardingWhitelist();

		for (auto i = whitelist.begin(); i != whitelist.end() && !forward; ++i)
			forward = regex_match(pathAndQuery, std::regex(*i));

		if (!forward)
		{
			log_information(MinerLogger::server, "Filtered bad request: %s", pathAndQuery);
			return RequestHandler::badRequest(request, response);
		}
	}

	auto session = MinerConfig::getConfig().createSession(hostType);

	if (session == nullptr)
		return;

	log_information(MinerLogger::server, "Forwarding request:\n\t%s", request.getURI());

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
	Miner& miner)
{
	// first check the credentials
	if (!checkCredentials(request, response))
		return;

	log_information(MinerLogger::server, "Got request for rescanning the plotdirs...");

	miner.rescanPlotfiles();

	// respond to the sender
	response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
	response.setContentLength(0);
	response.send();
}

void Burst::RequestHandler::checkPlotfile(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, MinerServer& server, std::string plotPathEnc)
{
	// first check the credentials
	if (!checkCredentials(request, response))
		return;

	response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
	response.setContentLength(0);
	response.send();

	log_information(MinerLogger::server, "Got request to check a file for corruption...");

	std::string plotPath = "";
	Poco::URI::decode(plotPathEnc, plotPath, false);

	float plotIntegrity = PlotGenerator::checkPlotfileIntegrity(plotPath);

	std::string plotID = getAccountIdFromPlotFile(plotPath) + "_" + getStartNonceFromPlotFile(plotPath) + "_" + getNonceCountFromPlotFile(plotPath) + "_" + getStaggerSizeFromPlotFile(plotPath);

	//response
	Poco::JSON::Object json;
	json.set("type", "plotcheck-result");
	json.set("plotID", plotID);
	json.set("plotIntegrity", std::to_string(plotIntegrity));

	server.sendToWebsockets(json);

}

bool Burst::RequestHandler::checkCredentials(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	auto credentialsOk = isLoggedIn(request);

	// the user is not logged in yet, but wants to
	if (!credentialsOk && request.getMethod() == "POST")
	{
		credentialsOk = login(request, response);

		if (!credentialsOk)
			log_information(MinerLogger::server, "%s webserver request.\n"
				"\trequest: %s\n"
				"\tfrom: %s",
				credentialsOk ? std::string("Authorized") : std::string("Unauthorized"),
				request.getURI(),
				request.clientAddress().toString());
	}

	// not authenticated, request again
	if (!credentialsOk)
	{
		redirect(request, response, "/login");
		return false;
	}

	// authenticated
	return true;
}

void Burst::RequestHandler::shutdown(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, Miner& miner, MinerServer& server)
{
	poco_ndc("RequestHandler::shutdown");

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

void Burst::RequestHandler::restart(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
	Miner& miner, MinerServer& server)
{
	poco_ndc("RequestHandler::restart");

	if (!checkCredentials(request, response))
		return;

	miner.restart();

	response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
	response.setContentLength(0);
	response.send();
}

void Burst::RequestHandler::submitNonce(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, MinerServer& server, Miner& miner)
{
	poco_ndc(SubmitNonceHandler::handleRequest);

	try
	{
		auto miningInfoOk = false;

		while (!miningInfoOk)
		{
			if (miner.hasBlockData())
				miningInfoOk = true;
			else
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		Poco::URI uri{request.getURI()};
		Poco::UInt64 capacity = 0;
		Poco::UInt64 accountId = 0;
		Poco::UInt64 nonce = 0;
		Poco::UInt64 deadline = 0;
		std::string plotfile;
		Poco::UInt64 blockheight = 0;
		std::string minerName;

		for (const auto& param : uri.getQueryParameters())
		{
			if (param.first == "accountId")
				accountId = Poco::NumberParser::parseUnsigned64(param.second);
			else if (param.first == "nonce")
				nonce = Poco::NumberParser::parseUnsigned64(param.second);
			else if (param.first == "blockheight")
				blockheight = Poco::NumberParser::parseUnsigned64(param.second);
			else if (param.first == "deadline")
				deadline = Poco::NumberParser::parseUnsigned64(param.second) / miner.getBaseTarget();
		}

		if (request.has(X_Capacity))
			capacity = Poco::NumberParser::parseUnsigned64(request.get(X_Capacity));

		if (request.has(X_Plotfile))
		{
			const auto& plotfileEncoded = request.get(X_Plotfile);
			Poco::URI::decode(plotfileEncoded, plotfile);
		}
		
		if (request.has(X_Deadline))
			deadline = Poco::NumberParser::parseUnsigned64(request.get(X_Deadline));

		auto account = miner.getAccount(accountId);

		if (account == nullptr)
			account = std::make_shared<Account>(accountId);

		if (plotfile.empty())
			plotfile = "unknown";
		
		if (blockheight == 0)
			blockheight = miner.getBlockheight();

		if ((deadline == 0 || MinerConfig::getConfig().isCalculatingEveryDeadline()) &&
			blockheight == miner.getBlockheight())
			deadline = PlotGenerator::generateAndCheck(accountId, nonce, miner);

		if (request.has(X_Miner) && MinerConfig::getConfig().isForwardingMinerName())
			minerName = request.get(X_Miner);

		log_information(MinerLogger::server, "Got nonce forward request (%s)\n"
			"\tnonce:   %s\n"
			"\taccount: %s\n"
			"\theight:  %s\n"
			"\tin:      %s",
			blockheight == miner.getBlockheight() ? deadlineFormat(deadline) : "for last block!",
			numberToString(nonce),
			account->getAddress(),
			numberToString(blockheight), plotfile
		);

		if (MinerConfig::getConfig().isCumulatingPlotsizes())
			PlotSizes::set(request.clientAddress().host(), capacity * 1024 * 1024 * 1024, false);

		if (blockheight != miner.getBlockheight())
		{
			const auto responseString = Poco::format(
				R"({ "result" : "Your submitted deadline is for another block!", "nonce" : %Lu, "blockheight" : %Lu, "currentBlockheight" : %Lu })",
				nonce, blockheight, miner.getBlockheight());

			response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
			response.setContentLength(responseString.size());
			auto& responseData = response.send();
			responseData << responseString << std::flush;
		}
		else if (accountId != 0 && nonce != 0 && deadline != 0)
		{
			const auto forwardResult = miner.submitNonce(nonce, accountId, deadline, miner.getBlockheight(), plotfile, false,
			                                             minerName, capacity);
			response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
			response.setContentLength(forwardResult.json.size());
			auto& responseData = response.send();
			responseData << forwardResult.json << std::flush;
		}
		else
		{
			// sum up the capacity
			request.set("X-Capacity", std::to_string(PlotSizes::getTotal(PlotSizes::Type::Combined)));

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
	json.set("targetDeadline", MinerConfig::getConfig().getTargetDeadline());
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
		if (!checkCredentials(request, response))
			return;

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
					MinerConfig::getConfig().setMaxSubmissionRetry(np::parseUnsigned(value));
				else if (key == "submit-probability")
					MinerConfig::getConfig().setSubmitProbability(np::parseFloat(value));
				else if (key == "max-historical-blocks")
					MinerConfig::getConfig().setMaxHistoricalBlocks(np::parseUnsigned(value));
				else if (key == "target-deadline")
					MinerConfig::getConfig().setTargetDeadline(value, TargetDeadlineType::Local);
				else if (key == "timeout")
					MinerConfig::getConfig().setTimeout(static_cast<float>(np::parseFloat(value)));
				else if (key == "log-dir")
					MinerConfig::getConfig().setLogDir(value);
				else if (Poco::icompare(key, std::string("cmb_").size(), std::string("cmb_")) == 0)
				{
					const auto logger_name = Poco::replace(key, "cmb_", "");
					const auto logger_priority = static_cast<Poco::Message::Priority>(np::parse(value));
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

	if (!checkCredentials(request, response))
		return;

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
