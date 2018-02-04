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
		void onMinerDataChangeEvent(const void* sender, const Poco::JSON::Object& data);
		static bool sendToWebsocket(Poco::Net::WebSocket& websocket, const std::string& data);

		Miner* miner_;
		MinerData* minerData_;
		uint16_t port_;
		std::unique_ptr<Poco::Net::HTTPServer> server_;
		std::vector<std::unique_ptr<Poco::Net::WebSocket>> websockets_;
		Poco::Mutex mutex_;
		TemplateVariables variables_;
		Poco::ThreadPool threadPool_;
		float progressRead_ = 0.f, progressVerification_ = 0.f;

		struct RequestFactory : Poco::Net::HTTPRequestHandlerFactory
		{
			explicit RequestFactory(MinerServer& server);

			MinerServer* server_;
			Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
		};
	};
}
