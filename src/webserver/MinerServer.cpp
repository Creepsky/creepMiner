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

#include "MinerServer.hpp"
#include <memory>
#include <Poco/Net/HTTPServer.h>
#include "MinerUtil.hpp"
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include "RequestHandler.hpp"
#include <Poco/JSON/Object.h>
#include <Poco/File.h>
#include "logging/MinerLogger.hpp"
#include <Poco/URI.h>
#include "mining/Miner.hpp"
#include "mining/MinerConfig.hpp"
#include <Poco/Path.h>
#include <Poco/NestedDiagnosticContext.h>
#include <Poco/String.h>
#include <Poco/Delegate.h>
#include <Poco/Net/NetException.h>
#include <Poco/Exception.h>

using namespace Poco;
using namespace Net;

Burst::MinerServer::MinerServer(Miner& miner)
	: miner_{&miner},
	  minerData_(nullptr),
	  port_{0}
{
	auto ip = MinerConfig::getConfig().getServerUrl().getCanonical();
	auto port = std::to_string(MinerConfig::getConfig().getServerUrl().getPort());

	variables_.variables.emplace(std::make_pair("title", []() { return Settings::Project.nameAndVersion; }));
	variables_.variables.emplace(std::make_pair("ip", [ip]() { return ip; }));
	variables_.variables.emplace(std::make_pair("port", [port]() { return port; }));
	variables_.variables.emplace(std::make_pair("nullDeadline", []() { return deadlineFormat(0); }));
}

Burst::MinerServer::~MinerServer()
{
	if (minerData_ != nullptr)
		minerData_->blockDataChangedEvent -= Poco::delegate(this, &MinerServer::onMinerDataChangeEvent);
}

void Burst::MinerServer::run(uint16_t port)
{
	poco_ndc(MinerServer::run);
	
	port_ = port;

	ServerSocket socket;

	try
	{
		socket.bind(port_, true);
		socket.listen();
	}
	catch (Poco::Exception& exc)
	{
		log_fatal(MinerLogger::server, "Error while creating local http server on port %hu!", port_);
		log_exception(MinerLogger::server, exc);
		return;
	}

	auto params = new HTTPServerParams;

	params->setMaxQueued(100);
	params->setMaxThreads(16);
	params->setServerName(Settings::Project.name);
	params->setSoftwareVersion(Settings::Project.nameAndVersion);

	if (server_ != nullptr)
		server_->stopAll(true);

	server_ = std::make_unique<HTTPServer>(new RequestFactory{*this}, threadPool_, socket, params);

	if (server_ != nullptr)
	{
		try
		{
			server_->start();
		}
		catch (Poco::Exception& exc)
		{
			server_.release();

			log_fatal(MinerLogger::server, "Could not start local server: %s", exc.displayText());
			log_current_stackframe(MinerLogger::server);
		}
	}
}

void Burst::MinerServer::stop()
{
	poco_ndc(MinerServer::stop);
	
	if (server_ != nullptr)
	{
		server_->stopAll(true);
		threadPool_.stopAll();
	}
}

void Burst::MinerServer::connectToMinerData(MinerData& minerData)
{
	minerData_ = &minerData;
	minerData_->blockDataChangedEvent += Poco::delegate(this, &MinerServer::onMinerDataChangeEvent);
}

void Burst::MinerServer::addWebsocket(std::unique_ptr<Poco::Net::WebSocket> websocket)
{
	poco_ndc(MinerServer::addWebsocket);
	
	ScopedLock<Mutex> lock{mutex_};
	auto blockData = minerData_->getBlockData();
	bool error;

	{
		std::stringstream ss;
		createJsonConfig().stringify(ss);
		error = !sendToWebsocket(*websocket, ss.str());
	}

	if (blockData != nullptr)
	{
		blockData->forEntries([this, &websocket](const Poco::JSON::Object& entry)
		{
			std::stringstream ss;
			entry.stringify(ss);
			return sendToWebsocket(*websocket, ss.str());
		});
	}

	if (error)
		websocket.release();
	else
		websockets_.emplace_back(move(websocket));
	
	// check the status of all connected websockets by sending a "ping" message
	// if the message can't be delivered, the websocket gets dropped
	sendToWebsockets("ping");
}

void Burst::MinerServer::sendToWebsockets(const std::string& data)
{
	poco_ndc(MinerServer::sendToWebsockets);
	ScopedLock<Mutex> lock{mutex_};
	
	auto ws = websockets_.begin();

	while (ws != websockets_.end())
	{
		if (sendToWebsocket(**ws, data))
			++ws;
		else
			ws = websockets_.erase(ws);
	}
}

void Burst::MinerServer::sendToWebsockets(const JSON::Object& json)
{
	std::stringstream ss;
	json.stringify(ss);
	sendToWebsockets(ss.str());
}

void Burst::MinerServer::onMinerDataChangeEvent(const void* sender, const Poco::JSON::Object& data)
{
	sendToWebsockets(data);
}

bool Burst::MinerServer::sendToWebsocket(WebSocket& websocket, const std::string& data)
{
	poco_ndc(MinerServer::sendToWebsocket(WebSocket&, const std::string&));
	
	try
	{
		auto n = websocket.sendFrame(data.data(), static_cast<int>(data.size()));
		if (n != static_cast<int>(data.size()))
			log_warning(MinerLogger::server, "Could not fully send: %s", data);
		return true;
	}
	catch (Poco::Net::ConnectionAbortedException&)
	{
		return false;
	}
	catch (Poco::IOException&)
	{
		return false;
	}
	catch (Poco::Exception& exc)
	{
		log_warning(MinerLogger::server, "Could not send the data to the websocket!\n\tdata: %s", data);
		log_exception(MinerLogger::server, exc);
		return false;
	}
}

Burst::MinerServer::RequestFactory::RequestFactory(MinerServer& server)
	: server_{&server}
{}

Poco::Net::HTTPRequestHandler* Burst::MinerServer::RequestFactory::createRequestHandler(const Poco::Net::HTTPServerRequest& request)
{
	using RequestHandler::LambdaRequestHandler;
	using req_t = Poco::Net::HTTPServerRequest;
	using res_t = Poco::Net::HTTPServerResponse;

	poco_ndc(MinerServer::RequestFactory::createRequestHandler);

	if (request.find("Upgrade") != request.end() &&
		icompare(request["Upgrade"], "websocket") == 0)
		return new LambdaRequestHandler([&](req_t& req, res_t& res)
		{
			RequestHandler::addWebsocket(req, res, *server_);
		});

	log_debug(MinerLogger::server, "Request: %s", request.getURI());

	try
	{
		using namespace Net;

		URI uri {request.getURI()};
		std::vector<std::string> path_segments;

		uri.getPathSegments(path_segments);

		// root
		if (path_segments.empty())
			return new LambdaRequestHandler([&](req_t& req, res_t& res)
			{
				auto variables = server_->variables_ + TemplateVariables({ { "includes", []() { return std::string("<script src='js/miner.js'></script>"); } } });
				RequestHandler::loadTemplate(req ,res, "index.html", "block.html", variables);
			});

		// plotfiles
		if (path_segments.front() == "plotfiles")
		{
			return new LambdaRequestHandler([&](req_t& req, res_t& res)
			{
				auto variables = server_->variables_ + TemplateVariables({
					{
						"includes", []() { 
							auto jsonPlotDirs = createJsonPlotDirs();

							std::stringstream sstr;
							jsonPlotDirs.stringify(sstr);

							std::stringstream sstrContent;
							sstrContent << "<script src='js/plotfiles.js'></script>";
							sstrContent << std::endl;
							sstrContent << "<script>var plotdirs = " << sstr.str() << ";</script>";
							return sstrContent.str();
						}
					}
				});

				RequestHandler::loadTemplate(req, res, "index.html", "plotfiles.html", variables);
			});
		}

		// shutdown everything
		if (path_segments.front() == "shutdown")
			return new LambdaRequestHandler([&](req_t& req, res_t& res)
			{
				RequestHandler::shutdown(req, res, *server_->miner_, *server_);
			});

		// rescan plot files
		if (path_segments.front() == "rescanPlotfiles")
			return new LambdaRequestHandler([&](req_t& req, res_t& res) { RequestHandler::rescanPlotfiles(req, res, *server_); });

		// show/change settings
		if (path_segments.front() == "settings")
		{
			// no body -> show
			if (path_segments.size() == 1)
				return new LambdaRequestHandler([&](req_t& req, res_t& res)
				{
					auto variables = server_->variables_ + TemplateVariables({ { "includes", []() { return std::string("<script src='js/settings.js'></script>"); } } });
					RequestHandler::loadTemplate(req, res, "index.html", "settings.html", variables);
				});

			// with body -> change
			if (path_segments.size() > 1)
				return new LambdaRequestHandler([&](req_t& req, res_t& res)
				{
					RequestHandler::changeSettings(req, res, *server_->miner_);
				});
		}

		// show/change plot files
		if (path_segments.front() == "plotdir")
			if (path_segments.size() > 1)
				return new LambdaRequestHandler([&](req_t& req, res_t& res)
				{
					RequestHandler::changePlotDirs(req, res, *server_, path_segments[1] == "remove" ? true : false);
				});

		// forward function
		if (path_segments.front() == "burst")
		{
			static const std::string getMiningInfo = "requestType=getMiningInfo";
			static const std::string submitNonce = "requestType=submitNonce";

			// send back local mining infos
			if (uri.getQuery().compare(0, getMiningInfo.size(), getMiningInfo) == 0)
				return new LambdaRequestHandler([&](req_t& req, res_t& res)
				{
					RequestHandler::miningInfo(req, res, *server_->miner_);
				});

			// forward nonce with combined capacity
			if (uri.getQuery().compare(0, submitNonce.size(), submitNonce) == 0)
				return new LambdaRequestHandler([&](req_t& req, res_t& res)
				{
					RequestHandler::submitNonce(req, res, *server_, *server_->miner_);
				});

			// just forward whatever the request is to the wallet
			// why wallet? because the only requests to a pool are getMiningInfo and submitNonce and we handled them already
			return new LambdaRequestHandler([&](req_t& req, res_t& res)
			{
				RequestHandler::forward(req, res, HostType::Wallet);
			});
		}

		Path path {"public"};
		path.append(uri.getPath());

		if (Poco::File {path}.exists())
			return new LambdaRequestHandler(RequestHandler::loadAsset);
		
		return new LambdaRequestHandler(RequestHandler::notFound);
	}
	catch (...)
	{
		return new LambdaRequestHandler(RequestHandler::badRequest);
	}
}
