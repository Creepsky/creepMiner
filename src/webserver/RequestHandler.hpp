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

#pragma once

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/WebSocket.h>
#include <memory>
#include <functional>
#include <unordered_map>
#include "mining/MinerConfig.hpp"

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

	/**
	 * \brief This class holds key value pairs (string -> string)
	 * that are all replaced inside a source string.
	 * 
	 * Keys always have the structure %KEY%, while values can be every possible string value.
	 */
	struct TemplateVariables
	{
		using Variable = std::function<std::string()>;
		std::unordered_map<std::string, Variable> variables;
		
		TemplateVariables() = default;
		TemplateVariables(std::unordered_map<std::string, Variable> variables);

		/**
		 * \brief Replaces all keys (%KEY%) inside a string with the set values.
		 * \param source The string, in which the keys get replaced.
		 */
		void inject(std::string& source) const;

		TemplateVariables operator+ (const TemplateVariables& rhs);
	};

	namespace RequestHandler
	{
		/**
		 * \brief A request handler, that carries and executes a lambda.
		 */
		class LambdaRequestHandler : public Poco::Net::HTTPRequestHandler
		{
		public:
			/**
			 * \brief Simple alias for a the lambda type.
			 */
			using Lambda = std::function<void(Poco::Net::HTTPServerRequest&, Poco::Net::HTTPServerResponse&)>;

			/**
			 * \brief Constructor.
			 * \param lambda The lambda function, that is called when processing a request.
			 */
			LambdaRequestHandler(Lambda lambda);

			/**
			 * \brief Handles an incoming HTTP request by expanding and executing the lambda.
			 * \param request The HTTP request.
			 * \param response The HTTP response.
			 */
			void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

		private:
			/**
			 * \brief The lambda. 
			 */
			Lambda lambda_;
		};

		void loadTemplate(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
		                  const std::string& templatePage, const std::string& contentPage,
		                  TemplateVariables& variables);

		/**
		 * \brief Loads an asset from a designated path.
		 * \param request The HTTP request.
		 * \param response The HTTP response.
		 * \param path The path of the asset.
		 * \return true, when the asset could be loaded, false otherwise.
		 */
		bool loadAssetByPath(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
		                     const std::string& path);

		/**
		 * \brief Loads an asset from a designated path by extracting the path from the request.
		 * \param request The HTTP request.
		 * \param response The HTTP response.
		 * \return true, when the asset could be loaded, false otherwise.
		 */
		bool loadAsset(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);
		
		/**
		 * \brief Redirects the request to another destination.
		 * \param request The HTTP request.
		 * \param response The HTTP response.
		 * \param redirectUri The URI, to which the request will be redirected.
		 */
		void redirect(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
		              const std::string& redirectUri);
		
		/**
		 * \brief Forwards a request to a destination and returns the response of it to the caller.
		 * \param request The HTTP request.
		 * \param response The HTTP response.
		 * \param hostType The HTTP session host type, that is the destination of the forwarding.
		 */
		void forward(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
			HostType hostType);
		
		/**
		 * \brief Sends a 400 Bad Request as a response to the caller.
		 * \param request The HTTP request.
		 * \param response The HTTP response.
		 */
		void badRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);

		/**
		 * \brief Gives order to rescan all plot directories.
		 * During this process, the plot size can be changed.
		 * \param request The HTTP request.
		 * \param response The HTTP response.
		 * \param server The Minerserver, which will propagate the changed config to his connected users.
		 */
		void rescanPlotfiles(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
		                     MinerServer& server);

		/**
		 * \brief Accepts an incoming websocket connection and if established,
		 * sends the config and all entries from log for the current block.
		 * \param request The HTTP request.
		 * \param response The HTTP response.
		 * \param server The server instance, that will accept and handle the websocket.
		 */
		void addWebsocket(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
						  MinerServer& server);

		/**
		 * \brief Checks the credentials for a request and compares them with the credentials set in the config file.
		 * \param request The HTTP request.
		 * \param response The HTTP response.
		 * \return true, if the request could be authenticated, false otherwise.
		 */
		bool checkCredentials(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);

		/**
		 * \brief Shuts down the application. Before that, the credentials are checked.
		 * \param request The HTTP request.
		 * \param response The HTTP response.
		 * \param miner The miner instance, that is shut down.
		 * \param server The server instance, that is shut down.
		 */
		void shutdown(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
		              Miner& miner, MinerServer& server);

		/**
		 * \brief Submits a nonce by forwarding it to the pool of the local miner instance.
		 * \param request The HTTP request.
		 * \param response The HTTP response.
		 * \param server The server instance, that will propagate 
		 * \param miner 
		 */
		void submitNonce(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
		                 MinerServer& server, Miner& miner);

		/**
		 * \brief Sends back the current mining info of the local miner instance.
		 * \param request The HTTP request.
		 * \param response The HTTP response.
		 * \param miner The miner instance, from which the mining info is gathered and send.
		 */
		void miningInfo(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
			Miner& miner);
	
		/**
		 * \brief Processes setting changes from a POST request.
		 * \param request The HTTP request.
		 * \param response The HTTP response.
		 * \param miner The miner instance, which is affected by the setting changes.
		 */
		void changeSettings(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
		                    Miner& miner);

		/**
		 * \brief Adds or deletes a plot directory to the current configuration.
		 * \param request The HTTP request.
		 * \param response The HTTP response.
		 * \param server The Minerserver, which will propagate the changed config to his connected users.
		 * \param remove If it is set to true, the plot directory will be removed. Otherwise it will be added.
		 */
		void changePlotDirs(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
		                    MinerServer& server, bool remove);

		/**
		 * \brief Sends a 404 Not Found as a response to the caller.
		 * \param request The HTTP request.
		 * \param response The HTTP response.
		 */
		void notFound(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);
	}
}
