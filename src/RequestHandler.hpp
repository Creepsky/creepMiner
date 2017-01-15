#pragma once

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/WebSocket.h>
#include <memory>
#include <Poco/Observer.h>
#include <Poco/NotificationCenter.h>
#include <functional>
#include <Poco/Any.h>
#include <unordered_map>

namespace Poco {namespace JSON {
	class Object;
}
}

namespace Burst
{
	class Miner;
	class MinerServer;

	struct TemplateVariables
	{
		using Variable = std::function<std::string()>;
		std::unordered_map<std::string, Variable> variables;
		void inject(std::string& source) const;
	};

	class NotFoundHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		~NotFoundHandler() = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;
	};

	class RootHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		RootHandler(const TemplateVariables& variables);
		~RootHandler() = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

	private:
		const TemplateVariables* variables_;
	};

	class ShutdownHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		ShutdownHandler(Miner& miner, MinerServer& server);
		~ShutdownHandler() = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

	private:
		Miner* miner_;
		MinerServer* server_;
	};

	class AssetHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		AssetHandler(const TemplateVariables& variables);
		~AssetHandler() = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

	private:
		const TemplateVariables* variables_;
	};

	class BadRequestHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		~BadRequestHandler() = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;
	};

	class WebSocketHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		explicit WebSocketHandler(MinerServer* server);
		~WebSocketHandler() = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

	private:
		MinerServer* server_;
	};
}
