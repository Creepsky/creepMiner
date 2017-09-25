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

#include "mining/Miner.hpp"
#include "logging/MinerLogger.hpp"
#include "mining/MinerConfig.hpp"
#include <Poco/Net/SSLManager.h>
#include "Poco/Net/ConsoleCertificateHandler.h"
#include "Poco/Net/HTTPSStreamFactory.h"
#include "Poco/Net/AcceptCertificateHandler.h"
#include <Poco/Net/HTTPSSessionInstantiator.h>
#include <Poco/Net/PrivateKeyPassphraseHandler.h>
#include <Poco/NestedDiagnosticContext.h>
#include "webserver/MinerServer.hpp"
#include <Poco/Logger.h>
#include "network/Request.hpp"
#include <Poco/Net/HTTPRequest.h>
#include "mining/MinerCL.hpp"
#include "gpu/impl/gpu_cuda_impl.hpp"

#ifdef USE_CUDA
#include <cuda_runtime_api.h>
#endif

class SSLInitializer
{
public:
	SSLInitializer()
	{
		Poco::Net::initializeSSL();
	}

	~SSLInitializer()
	{
		Poco::Net::uninitializeSSL();
	}
};

int main(int argc, const char* argv[])
{
	poco_ndc(main);

	Burst::MinerLogger::setup();
	
	// create a message dispatcher..
	//auto messageDispatcher = Burst::Message::Dispatcher::create();
	// ..and start it in its own thread
	//Poco::ThreadPool::defaultPool().start(*messageDispatcher);

	auto general = &Poco::Logger::get("general");
	
#ifdef NDEBUG
	std::string mode = "Release";
#else
	std::string mode = "Debug";
#endif

	using Burst::Settings;

	std::stringstream sstream;

	const auto checkAndPrint = [&](bool flag, const std::string& text) {
		sstream << ' ' << (flag ? '+' : '-') << text;
	};

	checkAndPrint(Settings::Cuda, "CUDA");
	checkAndPrint(Settings::OpenCl, "OpenCL");
	checkAndPrint(Settings::Sse4, "SSE4");
	checkAndPrint(Settings::Avx, "AVX");
	checkAndPrint(Settings::Avx2, "AVX2");

	log_information(general, Burst::Settings::Project.nameAndVersionVerbose);
	log_information(general, "%s mode%s", mode, sstream.str());
	log_information(general, "----------------------------------------------");
	log_information(general, "Github:   https://github.com/Creepsky/creepMiner");
	log_information(general, "Author:   Creepsky [creepsky@gmail.com]");
	log_information(general, "Burst :   BURST-JBKL-ZUAV-UXMB-2G795");
	log_information(general, "----------------------------------------------");

	std::string configFile = "mining.conf";

	if (argc > 1)
	{
		if (argv[1][0] == '-')
		{
			log_information(general, "usage : creepMiner <config-file>");
			log_information(general, "if no config-file specified, program will look for mining.conf file inside current directory");
		}
		configFile = std::string(argv[1]);
	}

	log_system(general, "using config file %s", configFile);
	
	try
	{
		using namespace Poco;
		using namespace Net;
		
		SSLInitializer sslInitializer;
		HTTPSStreamFactory::registerFactory();

		SharedPtr<InvalidCertificateHandler> ptrCert = new AcceptCertificateHandler(false); // ask the user via console
		Context::Ptr ptrContext = new Context(Context::CLIENT_USE, "", "", "",
			Context::VERIFY_RELAXED, 9, false, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
		SSLManager::instance().initializeClient(nullptr, ptrCert, ptrContext);

		HTTPSessionInstantiator::registerInstantiator();
		HTTPSSessionInstantiator::registerInstantiator();

		// check for a new version
		try
		{
			// fetch the online version file
			const std::string host = "https://github.com/Creepsky/creepMiner";
			const std::string versionPrefix = "version:";
			//
			Burst::Url url{ "https://raw.githubusercontent.com" };
			//
			Poco::Net::HTTPRequest getRequest{ "GET", "/Creepsky/creepMiner/master/version.id" };
			//
			Burst::Request request{ url.createSession() };
			auto response = request.send(getRequest);
			//
			std::string responseString;
			//
			if (response.receive(responseString))
			{
				// first we check if the online version begins with the prefix
				if (Poco::icompare(responseString, 0, versionPrefix.size(), versionPrefix) == 0)
				{
					auto onlineVersionStr = responseString.substr(versionPrefix.size());

					Burst::Version onlineVersion{ onlineVersionStr };

					if (onlineVersion > Burst::Settings::ProjectVersion)
						log_system(general, "There is a new version (%s) on\n\t%s", onlineVersion.literal, host);
				}
			}
		}
		// just skip if version could not be determined
		catch (...)
		{ }

		if (Burst::MinerConfig::getConfig().readConfigFile(configFile))
		{
			if (Burst::MinerConfig::getConfig().getProcessorType() == "OPENCL")
				Burst::MinerCL::getCL().create(Burst::MinerConfig::getConfig().getGpuPlatform(),
					Burst::MinerConfig::getConfig().getGpuDevice());
			else if (Burst::MinerConfig::getConfig().getProcessorType() == "CUDA")
			{
				Burst::Gpu_Cuda_Impl::listDevices();
				Burst::Gpu_Cuda_Impl::useDevice(Burst::MinerConfig::getConfig().getGpuDevice());
			}

			Burst::Miner miner;
			Burst::MinerServer server{miner};

			if (Burst::MinerConfig::getConfig().getStartServer())
			{
				server.connectToMinerData(miner.getData());
				server.run(Burst::MinerConfig::getConfig().getServerUrl().getPort());
			}

			Burst::MinerLogger::setChannelMinerData(&miner.getData());

			miner.run();
			server.stop();
		}
		else
		{
			log_error(general, "Aborting program due to invalid configuration");
		}
	}
	catch (Poco::Exception& exc)
	{
		log_fatal(general, "Aborting program due to exceptional state: %s", exc.displayText());
		log_exception(general, exc);
	}
	catch (std::exception& exc)
	{
		log_fatal(general, "Aborting program due to exceptional state: %s", std::string(exc.what()));
	}

	// wake up all message dispatcher
	//Burst::Message::wakeUpAllDispatcher();

	// stop all running background-tasks
	Poco::ThreadPool::defaultPool().stopAll();
	Poco::ThreadPool::defaultPool().joinAll();

	return 0;
}
