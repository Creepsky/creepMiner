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

using namespace Poco;
using namespace Net;

Burst::MinerServer::MinerServer(Miner& miner)
	: miner_{&miner}, minerData_(nullptr), port_{0}
{
	auto ip = MinerConfig::getConfig().getServerUrl().getCanonical();
	auto port = std::to_string(MinerConfig::getConfig().getServerUrl().getPort());

	variables_.variables.emplace(std::make_pair("title", []() { return Settings::Project.nameAndVersion; }));
	variables_.variables.emplace(std::make_pair("ip", [ip]() { return ip; }));
	variables_.variables.emplace(std::make_pair("port", [port]() { return port; }));
	variables_.variables.emplace(std::make_pair("nullDeadline", []() { return deadlineFormat(0); }));
}

Burst::MinerServer::~MinerServer()
{}

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
	catch (Poco::Exception&)
	{
		MinerLogger::write("error while creating local http server on port " + std::to_string(port_),
						   TextType::Error);
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
		catch (std::exception& exc)
		{
			server_.release();
			MinerLogger::writeStackframe(std::string("could not start local server: ") + exc.what());
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
	minerData.addObserverBlockDataChanged(*this, &MinerServer::blockDataChanged);
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
		for (auto data = blockData->entries.begin(); data != blockData->entries.end() && !error; ++data)
		{
			std::stringstream ss;
			data->stringify(ss);

			if (!sendToWebsocket(*websocket, ss.str()))
				error = true;
		}
	}

	if (error)
		websocket.release();
	else
		websockets_.emplace_back(move(websocket));		
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

void Burst::MinerServer::blockDataChanged(BlockDataChangedNotification* notification)
{
	poco_ndc(MinerServer::blockDataChanged);
	sendToWebsockets(*notification->blockData);
	notification->release();
}

bool Burst::MinerServer::sendToWebsocket(WebSocket& websocket, const std::string& data) const
{
	poco_ndc(MinerServer::sendToWebsocket(WebSocket&, const std::string&));
	
	try
	{
		auto n = websocket.sendFrame(data.data(), static_cast<int>(data.size()));
		if (n != static_cast<int>(data.size()))
			MinerLogger::write("could not fully send: " + data, TextType::Error);
		return true;
	}
	catch (std::exception& exc)
	{
		MinerLogger::write(std::string("could not send the data to the websocket!: ") + exc.what(),
			TextType::Debug);
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

	MinerLogger::write("request: " + request.getURI(), TextType::Debug);

	try
	{
		URI uri{request.getURI()};

		// root
		if (uri.getPath() == "/")
			return new RootHandler{server_->variables_};

		// shutdown everything
		if (uri.getPath() == "/shutdown")
			return new ShutdownHandler{*server_->miner_, *server_};

		// forward function
		if (uri.getPath() == "/burst")
		{
			// send back local mining infos
			if (startsWith<std::string>(uri.getQuery(), "requestType=getMiningInfo"))
				return new MiningInfoHandler{*server_->miner_};

			// forward nonce with combined capacity
			if (startsWith<std::string>(uri.getQuery(), "requestType=submitNonce"))
				return new SubmitNonceHandler{*server_->miner_};
			
			// just forward whatever the request is to the wallet
			// why wallet? because the only requests to a pool are getMiningInfo and submitNonce and we handled them already
			return new ForwardHandler{MinerConfig::getConfig().createSession(HostType::Wallet)};
		}

		Path path{"public"};
		path.append(uri.getPath());

		if (Poco::File{ path }.exists())
			return new AssetHandler{server_->variables_};

		return new NotFoundHandler;
	}
	catch (...)
	{
		return new BadRequestHandler;
	}
}
