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

#define CUDA_API_PER_THREAD_DEFAULT_STREAM

#include "mining/Miner.hpp"
#include "logging/MinerLogger.hpp"
#include "mining/MinerConfig.hpp"
#include <Poco/Net/SSLManager.h>
#include "Poco/Net/ConsoleCertificateHandler.h"
#include "Poco/Net/HTTPSStreamFactory.h"
#include "Poco/Net/AcceptCertificateHandler.h"
#include <Poco/Net/HTTPSSessionInstantiator.h>
// ReSharper disable CppUnusedIncludeDirective
#include <Poco/Net/PrivateKeyPassphraseHandler.h>
// ReSharper restore CppUnusedIncludeDirective
#include <Poco/NestedDiagnosticContext.h>
#include "webserver/MinerServer.hpp"
#include <Poco/Logger.h>
#include "network/Request.hpp"
#include <Poco/Net/HTTPRequest.h>
#include "mining/MinerCL.hpp"
#include "gpu/impl/gpu_cuda_impl.hpp"
#include <Poco/Util/OptionSet.h>
#include "webserver/RequestHandler.hpp"
#include <Poco/Util/OptionProcessor.h>
#include <Poco/Util/HelpFormatter.h>
#include <Poco/Util/Validator.h>
#include <Poco/FileStream.h>
#include <Poco/File.h>
#include <Poco/DirectoryIterator.h>
#include <regex>

class SslInitializer
{
public:
	SslInitializer();
	~SslInitializer();
};

class KeyConfigHandler: public Poco::Net::PrivateKeyPassphraseHandler
{
public:
	explicit KeyConfigHandler(bool server);
	~KeyConfigHandler() override;
	void onPrivateKeyRequested(const void*, std::string& privateKey) override;
};

struct Arguments
{
	Arguments();
	bool process(int argc, const char* argv[]);

	bool helpRequested = false;
	std::string confPath = "mining.conf";

private:
	void displayHelp(const std::string& name, const std::string& value);
	void setConfPath(const std::string& name, const std::string& value);

private:
	Poco::Util::OptionSet options_;
};

int main(const int argc, const char* argv[])
{
	poco_ndc(main);

	Arguments arguments;

	if (!arguments.process(argc, argv))
		return EXIT_FAILURE;

	if (arguments.helpRequested)
		return EXIT_SUCCESS;

	Burst::MinerLogger::setup();

	// create a message dispatcher..
	//auto messageDispatcher = Burst::Message::Dispatcher::create();
	// ..and start it in its own thread
	//Poco::ThreadPool::defaultPool().start(*messageDispatcher);

	const auto general = &Poco::Logger::get("general");

#ifdef NDEBUG
	std::string mode = "Release";
#else
	std::string mode = "Debug";
#endif

	using namespace Burst::Settings;

	std::stringstream sstream;

	const auto checkAndPrint = [&](bool flag, const std::string& text) {
		sstream << ' ' << (flag ? '+' : '-') << text;
	};

	checkAndPrint(Cuda, "CUDA");
	checkAndPrint(OpenCl, "OpenCL");
	checkAndPrint(Sse4, "SSE4");
	checkAndPrint(Avx, "AVX");
	checkAndPrint(Avx2, "AVX2");

	log_information(general, Burst::Settings::Project.nameAndVersionVerbose);
	log_information(general, "%s mode%s", mode, sstream.str());
	log_information(general, "----------------------------------------------");
	log_information(general, "Github:   https://github.com/Creepsky/creepMiner");
	log_information(general, "Author:   Creepsky [creepsky@gmail.com]");
	log_information(general, "Burst :   BURST-JBKL-ZUAV-UXMB-2G795");
	log_information(general, "----------------------------------------------");

	try
	{
		using namespace Poco;
		using namespace Net;

		SslInitializer sslInitializer;
		HTTPSStreamFactory::registerFactory();

		const SharedPtr<InvalidCertificateHandler> ptrCert(new AcceptCertificateHandler(false));
		const Context::Ptr clientContext(new Context(Context::CLIENT_USE, ""));
		SSLManager::instance().initializeClient(nullptr, ptrCert, clientContext);

		HTTPSessionInstantiator::registerInstantiator();
		HTTPSSessionInstantiator::registerInstantiator();

		// start versionChecker timer thread , checking online version every 30 minutes
		Poco::Timer checkVersionTimer(100, 1800000);
		checkVersionTimer.start(Poco::TimerCallback<Burst::ProjectData>(Burst::Settings::Project, &Burst::ProjectData::refreshAndCheckOnlineVersion));

		auto running = true;

		while (running)
		{
			// load the config
			auto configLoaded = Burst::MinerConfig::getConfig().readConfigFile(arguments.confPath);

			// the config could not be loaded, look for a config in the creepMiner home dir
			if (!configLoaded)
			{
				log_information(general, "Could not load config %s", arguments.confPath);

				Path minerRootPath(Path::home());
				minerRootPath.pushDirectory(".creepMiner");

				File(minerRootPath).createDirectory();

				Path minerHomePath(minerRootPath);
				minerHomePath.pushDirectory(std::string(Project.getVersion()));

				if (File(minerHomePath).createDirectory())
					log_information(general, "Home directory has created: %s", minerHomePath.toString());

				Path homeConfigPath(minerHomePath);
				homeConfigPath.append("mining.conf");
				const auto homeConfig = homeConfigPath.toString();

				log_information(general, "Trying to load config %s", homeConfig);
				configLoaded = Burst::MinerConfig::getConfig().readConfigFile(homeConfig);

				// if there is also no config in the home dir, create one in the home dir
				if (!configLoaded)
				{
					// create the log dir
					Path homeLogPath(minerHomePath);
					homeLogPath.pushDirectory("logs");
					homeLogPath.makeDirectory();

					// and save it in the config
					Burst::MinerConfig::getConfig().setLogDir(homeLogPath.toString());
					Burst::MinerConfig::getConfig().useLogfile(true);

					if (!Burst::MinerConfig::getConfig().save(homeConfig))
						log_error(Burst::MinerLogger::general, "Could not save current settings!");

					// load the freshly created config in the home dir
					configLoaded = Burst::MinerConfig::getConfig().readConfigFile(homeConfig);
				}
			}

			if (configLoaded)
			{
				log_information(general, "Config loaded: %s", Burst::MinerConfig::getConfig().getPath());

				if (!Burst::MinerConfig::getConfig().getServerCertificatePath().empty())
				{
#ifdef _WIN32
					const Context::Ptr serverContext(new Context(Context::SERVER_USE,
						Burst::MinerConfig::getConfig().getServerCertificatePath(),
						Context::VERIFY_RELAXED, Context::OPT_LOAD_CERT_FROM_FILE | Context::OPT_USE_STRONG_CRYPTO));
#elif defined __linux__ || defined __APPLE__
					const Context::Ptr serverContext(new Context(Context::SERVER_USE,
						Burst::MinerConfig::getConfig().getServerCertificatePath(),
						Context::VERIFY_RELAXED));
#endif

					const SharedPtr<PrivateKeyPassphraseHandler> privateKeyPassphraseHandler(new KeyConfigHandler(true));
					SSLManager::instance().initializeServer(privateKeyPassphraseHandler, ptrCert, serverContext);
				}

				if (Burst::MinerConfig::getConfig().getProcessorType() == "OPENCL" &&
					!Burst::MinerCL::getCL().initialized())
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
				Burst::MinerConfig::getConfig().checkPlotOverlaps();

				miner.run();
				server.stop();

				running = miner.wantRestart();
				Burst::MinerLogger::setChannelMinerData(nullptr);

				if (running)
					log_system(Burst::MinerLogger::miner, "Restarting the miner...");
			}
			else
			{
				log_error(general, "Aborting program due to invalid configuration");
				running = false;
			}
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

SslInitializer::SslInitializer()
{
	Poco::Net::initializeSSL();
}

SslInitializer::~SslInitializer()
{
	Poco::Net::uninitializeSSL();
}

Arguments::Arguments()
{
	using Poco::Util::Option;

	options_.addOption(Option("help", "h", "Display this help information")
		.required(false)
		.repeatable(false)
		.callback(Poco::Util::OptionCallback<Arguments>(this, &Arguments::displayHelp)));

	options_.addOption(Option("config", "c", "Path to miner config file\n"
        "e.g. Windows -c d:\\creepMiner\\miner.config\n"
        "     or      --config=c:\\creepMiner\\miner.config\n"
        "e.g. linux   -c /path/miner.config")
		.required(false)
		.repeatable(false)
		.argument("path")
		.callback(Poco::Util::OptionCallback<Arguments>(this, &Arguments::setConfPath)));
}

bool Arguments::process(const int argc, const char* argv[])
{
	Poco::Util::OptionProcessor optionProcessor(options_);
	optionProcessor.setUnixStyle(std::string(Burst::Settings::OsFamily) != "Windows");

	try
	{
		for (auto i = 1; i < argc; ++i)
		{
			std::string name;
			std::string value;

			if (optionProcessor.process(argv[i], name, value))
			{
				if (!name.empty())
				{
					const auto& option = options_.getOption(name);

					if (option.validator())
						option.validator()->validate(option, value);

					if (option.callback())
						option.callback()->invoke(name, value);
				}
			}
		}

		optionProcessor.checkRequired();
		return true;
	}
	catch (...)
	{
		displayHelp("", "");
		return false;
	}
}

void Arguments::displayHelp(const std::string& name, const std::string& value)
{
	Poco::Util::HelpFormatter helpFormatter(options_);
	helpFormatter.setUnixStyle(std::string(Burst::Settings::OsFamily) != "Windows");
	helpFormatter.setCommand("creepMiner");
	helpFormatter.setUsage("<options>");
	helpFormatter.setHeader("Burstcoin Proof-of-Capacity CPU and GPU miner.");
	helpFormatter.setFooter("Copyright (C)  2016-2018 Creepsky (creepsky@gmail.com)");
	helpFormatter.setAutoIndent();
	helpFormatter.format(std::cout);

	helpRequested = true;
}

void Arguments::setConfPath(const std::string& name, const std::string& value)
{
	confPath = value;
}

KeyConfigHandler::KeyConfigHandler(bool server)
	: PrivateKeyPassphraseHandler{server}
{}

KeyConfigHandler::~KeyConfigHandler()
{};

void KeyConfigHandler::onPrivateKeyRequested(const void*, std::string& privateKey)
{
	privateKey = Burst::MinerConfig::getConfig().getServerCertificatePass();
}
