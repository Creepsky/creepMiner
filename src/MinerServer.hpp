#pragma once

#include <memory>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include "RequestHandler.hpp"

namespace Poco
{
	class Notification;

	namespace Net
	{
		class HTTPServer;
		class WebSocket;
	}

	namespace JSON
	{
		class Object;
	}
}

namespace Burst
{
	class Miner;
	struct BlockDataChangedNotification;
	class MinerData;

	class MinerServer
	{
	public:
		explicit MinerServer(Miner& miner);
		~MinerServer();
		
		void run(uint16_t port = 9999);
		void stop();
		
		void connectToMinerData(MinerData& minerData);

		void addWebsocket(std::unique_ptr<Poco::Net::WebSocket> websocket);
		void sendToWebsockets(const std::string& data);
		void sendToWebsockets(const Poco::JSON::Object& json);

	private:
		void blockDataChanged(BlockDataChangedNotification* notification);
		bool sendToWebsocket(Poco::Net::WebSocket& websocket, const std::string& data) const;

		Miner* miner_;
		MinerData* minerData_;
		uint16_t port_;
		std::unique_ptr<Poco::Net::HTTPServer> server_;
		std::vector<std::unique_ptr<Poco::Net::WebSocket>> websockets_;
		Poco::FastMutex mutex_;
		TemplateVariables variables_;
		Poco::ThreadPool threadPool_;

		struct RequestFactory : Poco::Net::HTTPRequestHandlerFactory
		{
			explicit RequestFactory(MinerServer& server);

			MinerServer* server_;
			Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
		};
	};
}
