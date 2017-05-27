#pragma once

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/WebSocket.h>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Poco
{
	namespace JSON
	{
		class Object;
	}

	namespace Net
	{
		class HTTPServerRequest;
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
		NotFoundHandler(const TemplateVariables& variables);
		~NotFoundHandler() override = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

	private:
		const TemplateVariables* variables_;
	};

	class RootHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		RootHandler(const TemplateVariables& variables);
		~RootHandler() override = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

	private:
		const TemplateVariables* variables_;
	};

	class PlotfilesHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		PlotfilesHandler(const TemplateVariables& variables);
		~PlotfilesHandler() override = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

	private:
		const TemplateVariables* variables_;
	};

	class RescanPlotfilesHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		RescanPlotfilesHandler(MinerServer& server);
		~RescanPlotfilesHandler() override = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

	private:
		MinerServer* server_;
	};

	class ShutdownHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		ShutdownHandler(Miner& miner, MinerServer& server);
		~ShutdownHandler() override = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

	private:
		Miner* miner_;
		MinerServer* server_;
	};

	class AssetHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		AssetHandler(const TemplateVariables& variables);
		~AssetHandler() override = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

	private:
		const TemplateVariables* variables_;
	};

	class BadRequestHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		~BadRequestHandler() override = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;
	};

	class WebSocketHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		explicit WebSocketHandler(MinerServer* server);
		~WebSocketHandler() override = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

	private:
		MinerServer* server_;
	};

	class MiningInfoHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		explicit MiningInfoHandler(Miner& miner);
		~MiningInfoHandler() override = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

	private:
		Miner* miner_;
	};

	class SubmitNonceHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		explicit SubmitNonceHandler(Miner& miner);
		~SubmitNonceHandler() override = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

	private:
		Miner* miner_;
	};

	class ForwardHandler : public Poco::Net::HTTPRequestHandler
	{
	public:
		explicit ForwardHandler(std::unique_ptr<Poco::Net::HTTPClientSession> session);
		~ForwardHandler() override = default;
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

	private:
		std::unique_ptr<Poco::Net::HTTPClientSession> session_;
	};
}
