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
#include <Poco/Exception.h>
#include <Poco/Net/SecureServerSocket.h>

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

	std::unique_ptr<ServerSocket> socket;

	if (MinerConfig::getConfig().getServerCertificatePath().empty())
		socket = std::make_unique<ServerSocket>();
	else
		socket = std::make_unique<SecureServerSocket>();

	try
	{
		socket->bind(port_, true);
		socket->listen();
	}
	catch (Exception& exc)
	{
		log_fatal(MinerLogger::server, "Error while creating local http server on port %hu!", port_);
		log_exception(MinerLogger::server, exc);
		return;
	}

	auto params = new HTTPServerParams;

	params->setMaxQueued(MinerConfig::getConfig().getMaxConnectionsQueued());
	params->setMaxThreads(MinerConfig::getConfig().getMaxConnectionsActive());
	params->setServerName(Settings::Project.name);
	params->setSoftwareVersion(Settings::Project.nameAndVersion);

	threadPool_.addCapacity(MinerConfig::getConfig().getMaxConnectionsActive());

	if (server_ != nullptr)
		server_->stopAll(true);

	server_ = std::make_unique<HTTPServer>(new RequestFactory{*this}, threadPool_, *socket, params);

	if (server_ != nullptr)
	{
		try
		{
			server_->start();
		}
		catch (Poco::Exception& exc)
		{
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

void Burst::MinerServer::sendToWebsockets(std::string& data)
{
	poco_ndc(MinerServer::sendToWebsockets);
	ScopedLock<Mutex> lock{mutex_};
	newDataEvent(this, data);
}

void Burst::MinerServer::sendToWebsockets(const JSON::Object& json)
{
	std::stringstream sstream;
	json.stringify(sstream);
	auto jsonString = sstream.str();
	sendToWebsockets(jsonString);
}

void Burst::MinerServer::onMinerDataChangeEvent(const void* sender, const Poco::JSON::Object& data)
{
	auto send = true;

	// pre-filter progress
	if (data.has("type") &&
		data.get("type") == "progress")
	{
		auto progressRead = data.get("value").extract<float>();
		auto progressVerification = data.get("valueVerification").extract<float>();

		send = static_cast<int>(progressRead) != static_cast<int>(progressRead_) ||
			static_cast<int>(progressVerification) != static_cast<int>(progressVerification_);
		
		progressRead_ = progressRead;
		progressVerification_ = progressVerification;
	}

	if (send)
		sendToWebsockets(data);
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

	if (request.find("Upgrade") != request.end() && icompare(request["Upgrade"], "websocket") == 0)
		return new RequestHandler::WebsocketRequestHandler(*server_, *server_->minerData_);

	std::stringstream sstream;
	sstream << "Request: " << request.getURI() << std::endl;
	sstream << "Ip: " << request.clientAddress().toString() << std::endl;
	sstream << "Method: " << request.getMethod() << std::endl;
	
	for (const auto& header : request)
		sstream << header.first << ':' << header.second << std::endl;
	
	log_debug(MinerLogger::server, "Request: %s", request.getURI());
	log_file_only(MinerLogger::server, Poco::Message::PRIO_INFORMATION, TextType::Information, sstream.str());

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
				auto variables = server_->variables_ + TemplateVariables({ { "includes", []() { return std::string("<script src='js/block.js'></script>"); } } });
				RequestHandler::loadSecuredTemplate(req ,res, "index.html", "block.html", variables);
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

				RequestHandler::loadSecuredTemplate(req, res, "index.html", "plotfiles.html", variables);
			});
		}

		// shutdown everything
		if (path_segments.front() == "shutdown")
			return new LambdaRequestHandler([&](req_t& req, res_t& res)
			{
				RequestHandler::shutdown(req, res, *server_->miner_, *server_);
			});

		// restart
		if (path_segments.front() == "restart")
			return new LambdaRequestHandler([&](req_t& req, res_t& res)
		{
			RequestHandler::restart(req, res, *server_->miner_, *server_);
		});

		// rescan plot files
		if (path_segments.front() == "rescanPlotfiles")
			return new LambdaRequestHandler([&](req_t& req, res_t& res) { RequestHandler::rescanPlotfiles(req, res, *server_->miner_); });

		// show/change settings
		if (path_segments.front() == "settings")
		{
			// no body -> show
			if (path_segments.size() == 1)
				return new LambdaRequestHandler([&](req_t& req, res_t& res)
				{
					auto variables = server_->variables_ + TemplateVariables({ { "includes", []() { return std::string("<script src='js/settings.js'></script>"); } } });
					RequestHandler::loadSecuredTemplate(req, res, "index.html", "settings.html", variables);
				});

			// with body -> change
			if (path_segments.size() > 1)
				return new LambdaRequestHandler([&](req_t& req, res_t& res)
				{
					RequestHandler::changeSettings(req, res, *server_->miner_);
				});
		}

		// check plot file
		if (path_segments.front() == "checkPlotFile")
			if (path_segments.size() > 1) {
				if (path_segments[1] == "all") {
					return new LambdaRequestHandler([&](req_t& req, res_t& res)
					{
						RequestHandler::checkAllPlotfiles(req, res, *server_->miner_, *server_);
					});
				}
				using Poco::replace;
				std::string plotPath = "";
				Poco::URI::decode(replace(request.getURI(), "/" + path_segments[0] + "/", plotPath), plotPath, false);
				return new LambdaRequestHandler([&, pPath = move(plotPath)](req_t& req, res_t& res)
				{
					RequestHandler::checkPlotfile(req, res, *server_->miner_, *server_, pPath);
				});
			}

		// plot new file
		if (path_segments.front() == "plotter")
			if (path_segments.size() == 5) {
				using Poco::replace;
				std::string plotPath = "";
				Poco::URI::decode(replace(request.getURI(), "/" + path_segments[0] + "/" + path_segments[1] + "/" + path_segments[2] + "/" + path_segments[3] + "/", plotPath), plotPath, false);
				return new LambdaRequestHandler([&, pPath = move(plotPath), account = move(path_segments[1]), sNonce = move(path_segments[2]), nNonces = move(path_segments[3])](req_t& req, res_t& res)
				{
					RequestHandler::plotNewFile(req, res, *server_->miner_, *server_, pPath, account, sNonce, nNonces);
				});
			}

		// show/change plot files
		if (path_segments.front() == "plotdir")
			if (path_segments.size() > 1)
				return new LambdaRequestHandler([&](req_t& req, res_t& res)
				{
					RequestHandler::changePlotDirs(req, res, *server_, path_segments[1] == "remove" ? true : false);
				});

		if (path_segments.front() == "login")
			return new LambdaRequestHandler([&](req_t& req, res_t& res)
			{
				if (req.getMethod() == "POST")
				{
					if (RequestHandler::checkCredentials(req, res))
						return RequestHandler::redirect(req, res, "/");
				}
				else
				{
					auto variables = server_->variables_ + TemplateVariables({ { "includes", []() { return std::string(); } } });
					RequestHandler::loadTemplate(req, res, "index.html", "login.html", variables);
				}
			});

		if (path_segments.front() == "logout")
			return new LambdaRequestHandler([&](req_t& req, res_t& res) { RequestHandler::logout(req, res); });

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
