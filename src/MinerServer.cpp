#include "MinerServer.hpp"
#include <memory>
#include <Poco/Net/HTTPServer.h>
#include "MinerUtil.hpp"
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include "RequestHandler.hpp"
#include <Poco/JSON/Object.h>
#include <Poco/File.h>
#include "MinerLogger.hpp"
#include <Poco/URI.h>
#include "Miner.hpp"
#include "MinerConfig.hpp"
#include <Poco/Path.h>
#include <Poco/NestedDiagnosticContext.h>
#include <Poco/String.h>
#include <Poco/Delegate.h>
#include <Poco/Net/NetException.h>

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
	
	ScopedLock<FastMutex> lock{mutex_};
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
	ScopedLock<FastMutex> lock{mutex_};
	
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
	poco_ndc(MinerServer::RequestFactory::createRequestHandler);

	if (request.find("Upgrade") != request.end() &&
		icompare(request["Upgrade"], "websocket") == 0)
		return new WebSocketHandler{server_};

	log_debug(MinerLogger::server, "Request: %s", request.getURI());

	try
	{
		URI uri {request.getURI()};
		std::vector<std::string> path_segments;

		uri.getPathSegments(path_segments);

		// root
		if (path_segments.empty())
			return new RootHandler{server_->variables_};

		// plotfiles
		if (path_segments.front() == "plotfiles")
			return new PlotfilesHandler{server_->variables_};

		// shutdown everything
		if (path_segments.front() == "shutdown")
			return new ShutdownHandler{*server_->miner_, *server_};

		// rescan plot files
		if (path_segments.front() == "rescanPlotfiles")
			return new RescanPlotfilesHandler(*server_);

		// show/change settings
		if (path_segments.front() == "settings")
		{
			// no body -> show
			if (path_segments.size() == 1)
				return new SettingsHandler(server_->variables_, *server_);

			// with body -> change
			if (path_segments.size() > 1)
				return new SettingsChangeHandler(*server_, *server_->miner_);
		}

		// show/change plot files
		if (path_segments.front() == "plotdir")
			if (path_segments.size() > 1)
				return new PlotDirHandler(path_segments[1] == "remove" ? true:  false, *server_->miner_, *server_);

		// forward function
		if (path_segments.front() == "burst")
		{
			static const std::string getMiningInfo = "requestType=getMiningInfo";
			static const std::string submitNonce = "requestType=submitNonce";

			// send back local mining infos
			if (uri.getQuery().compare(0, getMiningInfo.size(), getMiningInfo) == 0)
				return new MiningInfoHandler{*server_->miner_};

			// forward nonce with combined capacity
			if (uri.getQuery().compare(0, submitNonce.size(), submitNonce) == 0)
				return new SubmitNonceHandler{*server_->miner_};

			// just forward whatever the request is to the wallet
			// why wallet? because the only requests to a pool are getMiningInfo and submitNonce and we handled them already
			return new ForwardHandler{MinerConfig::getConfig().createSession(HostType::Wallet)};
		}

		Path path {"public"};
		path.append(uri.getPath());

		if (Poco::File {path}.exists())
			return new AssetHandler{server_->variables_};

		return new NotFoundHandler{server_->variables_};
	}
	catch (...)
	{
		return new BadRequestHandler;
	}
}
