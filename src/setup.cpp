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


#include "setup.hpp"
#include "logging/Console.hpp"
#include "logging/Message.hpp"
#include "mining/MinerCL.hpp"
#include "logging/MinerLogger.hpp"
#include "plots/PlotVerifier.hpp"
#include <Poco/File.h>
#include <Poco/Delegate.h>
#include <Poco/Random.h>
#include <Poco/Net/DNS.h>
#include <Poco/Net/ServerSocket.h>
#include "network/Request.hpp"
#include "MinerUtil.hpp"
#include <Poco/Net/HTTPRequest.h>
#include <Poco/JSON/Parser.h>
#include <condition_variable>

const std::string Burst::Setup::exit = "Exit";
const std::string Burst::Setup::yes = "Yes";
const std::string Burst::Setup::no = "No";

namespace Burst
{
	namespace Helper
	{
		std::string enterPassword()
		{
			std::string pass;
			setStdInEcho(false);
			while (pass.empty())
			{
				getline(std::cin, pass);
				Console::nextLine();
			}
			setStdInEcho(true);
			return pass;
		}

		void enterPasswordSecured(std::string& password)
		{
			const auto pb = Console::print();
			password.clear();

			std::string passRepeat = "0";

			while (password != passRepeat)
			{
				pb.addTime().print(": ").setColor(ConsoleColor::LightCyan).print("Your pass: ").resetColor();
				password = enterPassword();
				
				pb.addTime().print(": ").setColor(ConsoleColor::LightCyan).print("Repeat: ").resetColor();
				passRepeat = enterPassword();

				if (password != passRepeat)
					log_error(MinerLogger::general, "Mismatch, try again");
			}
		}
	}
}

bool Burst::Setup::setup(MinerConfig& config)
{
	auto logger = &Poco::Logger::get("general");
	log_information(logger, "Welcome to the creepMiner setup");

	std::string processorType;
	std::string instructionSet;
	int platformIndex, deviceIndex;
	std::vector<std::string> plotLocations;
	unsigned memory = 0;
	unsigned reader = 0, verifier = 0;
	std::string ip, webinterfaceUser, webinterfacePassword;
	bool fancyProgressbar, steadyProgressbar;
	std::string submission, miningInfo, wallet;
	std::string passphrase;
	int index;

	const auto type = readInput(
		{
			"Everything", "Processor type", "Plots", "Buffersize", "Plotreader/verifier",
			"Progressbar", "URIs"
		}, "What do you want to setup?", "Everything",
		index);

	const auto all = type == "Everything";
	// TODO: delete and create and create a new file with the content {} <- the miner will create a default config
	const auto reset = type == "Reset";
	const auto processorTypeFlag = type == "Processor type" || all;
	const auto plots = type == "Plots" || all;
	const auto buffersize = type == "Buffersize" || all;
	const auto plotReaderVerifier = type == "Plotreader/verifier" || all || processorTypeFlag;
	const auto progressBar = type == "Progressbar" || all;
	const auto webserver = type == "URIs" || all;
	const auto poolWallet = type == "URIs" || all;

	if (reset)
		return true;

	if (processorTypeFlag)
	{
		if (chooseProcessorType(processorType))
			config.setProcessorType(processorType);
		else return false;
	}

	if (processorTypeFlag && processorType == "CPU")
	{
		if (chooseCpuInstructionSet(instructionSet))
			config.setCpuInstructionSet(instructionSet);
		else return false;
	}

	if (processorTypeFlag && processorType == "OPENCL")
	{
		if (!chooseGpuPlatform(platformIndex) ||
			!chooseGpuDevice(platformIndex, deviceIndex))
			return false;

		config.setGpuPlatform(platformIndex);
		config.setGpuDevice(deviceIndex);

		MinerCL::getCL().create();
	}
	
	if (plots)
	{
		if (choosePlots(plotLocations))
			config.setPlotDirs(plotLocations);
		else return false;
	}

	if ((buffersize || plotReaderVerifier) && !plotLocations.empty())
	{
		if (buffersize)
		{
			if (chooseBufferSize(memory))
				config.setBufferSize(memory);
			else return false;
		}

		if (plotReaderVerifier)
		{
			if (choosePlotReader(plotLocations.size(), reader, verifier))
			{
				config.setMaxPlotReaders(reader);
				config.setMininigIntensity(verifier);
			}
			else return false;
		}
	}

	if (progressBar)
	{
		if (chooseProgressbar(fancyProgressbar, steadyProgressbar))
			config.setProgressbar(fancyProgressbar, steadyProgressbar);
		else return false;
	}
	
	if (webserver)
	{
		if (chooseWebserver(ip, webinterfaceUser, webinterfacePassword))
		{
			config.setUrl(ip, HostType::Server);
			config.setWebserverCredentials(webinterfaceUser, webinterfacePassword);
		}
		else return false;
	}

	if (poolWallet)
	{
		if (chooseUris(submission, miningInfo, wallet, passphrase))
		{
			config.setUrl(submission, HostType::MiningInfo);
			config.setUrl(miningInfo, HostType::Pool);
			config.setUrl(wallet, HostType::Wallet);
			config.setPassphrase(passphrase);
		}
		else return false;
	}

	if (!config.save())
		log_warning(MinerLogger::miner, "Your settings could not be saved! They are only valid for this session.");

	return true;
}

std::string Burst::Setup::readInput(const std::vector<std::string>& options, const std::string& header,
                                    const std::string& defaultValue, int& index)
{
	auto pb = Console::print();
	pb.addTime().print(": ").setColor(ConsoleColor::LightCyan).print("%s", header).resetColor().nextLine();

	for (size_t i = 0; i < options.size(); ++i)
	{
		pb.addTime().print(": [%7z]: %s", i + 1, options[i]);

		if (options[i] == defaultValue)
			pb.setColor(ConsoleColor::LightGreen).print(" <--").resetColor();

		pb.nextLine();
	}

	pb.addTime().print(": [ Escape]: Exit").nextLine();

	pb.addTime().print(": [  Enter]: Use the default value (")
	  .setColor(ConsoleColor::Green).print(defaultValue)
	  .resetColor().print(')').nextLine();

	bool exit, useDefault;

	pb.addTime().print(": Your choice: ");

	do
	{
		const auto choice = getChar();
		exit = choice == 0x1B;
		useDefault = choice == '\n' || choice == '\r';
		index = choice - '0' - 1;
	}
	while (!exit && !useDefault && (index < 0 || index >= options.size()));

	std::string choiceText;
	auto color = ConsoleColor::LightGreen;

	if (exit)
	{
		choiceText = Setup::exit;
		color = ConsoleColor::Red;
	}
	else if (useDefault)
	{
		choiceText = defaultValue;
		const auto iter = find(options.begin(), options.end(), defaultValue);
		index = static_cast<int>(distance(options.begin(), iter));
	}
	else
		choiceText = options[index];

	pb.setColor(color).print(choiceText).nextLine().resetColor().flush();

	return choiceText;
}

std::string Burst::Setup::readYesNo(const std::string& header, const bool defaultValue)
{
	int index;
	return readInput({"Yes", "No"}, header, defaultValue ? "Yes" : "No", index);
}

bool Burst::Setup::readNumber(const std::string& title, const Poco::Int64 min, const Poco::Int64 max,
                              const Poco::Int64 defaultValue, Poco::Int64& number)
{
	auto entered = false;
	std::string input;
	const auto pb = Console::print();

	while (!entered)
	{
		try
		{
			pb.addTime().print(": ").print(title).print(": ");
			getline(std::cin, input);
			
			if (input.empty())
				number = defaultValue;
			else if (input == "\n" || input == "\r") // exit
				return false;
			else
				number = Poco::NumberParser::parse64(input);

			entered = number >= min && number <= max;
		}
		catch (...)
		{
		}
	}

	return entered;
}

std::string Burst::Setup::readText(const std::string& title, const std::function<bool(const std::string&, std::string&)> validator)
{
	auto validated = false;
	std::string text;

	const auto pb = Console::print();

	while (!validated)
	{
		pb.addTime().print(": ").setColor(ConsoleColor::LightCyan).print(title).print(": ").resetColor();
		getline(std::cin, text);
		validated = validator(text, text);
	}

	return text;
}

bool Burst::Setup::chooseProcessorType(std::string& processorType)
{
	// first, check what processor types are available
	std::vector<std::string> processorTypes = {"CPU"};

	if (Settings::Cuda)
		processorTypes.emplace_back("CUDA");

	if (Settings::OpenCl && !MinerCL::getCL().getPlatforms().empty())
		processorTypes.emplace_back("OPENCL");

	int index;
	processorType = readInput(processorTypes, "Choose your processor type", "CPU", index);

	return processorType != exit;
}

bool Burst::Setup::chooseCpuInstructionSet(std::string& instructionSet)
{
	std::vector<std::string> instructionSets = {"SSE2"};
	std::string defaultSetting;

	if (Settings::Sse4)
		instructionSets.emplace_back("SSE4");

	if (Settings::Avx)
		instructionSets.emplace_back("AVX");

	if (Settings::Avx2)
		instructionSets.emplace_back("AVX2");

	if (cpuHasInstructionSet(avx2) && Settings::Avx2)
		defaultSetting = "AVX2";
	else if (cpuHasInstructionSet(avx) && Settings::Avx)
		defaultSetting = "AVX";
	else if (cpuHasInstructionSet(sse4) && Settings::Sse4)
		defaultSetting = "SSE4";
	else
		defaultSetting = "SSE2";

	int index;
	instructionSet = readInput(instructionSets, "Choose the CPU instruction set", defaultSetting, index);

	return instructionSet != exit;
}

bool Burst::Setup::chooseGpuPlatform(int& platformIndex)
{
	std::vector<std::string> platforms;

	for (const auto& platform : MinerCL::getCL().getPlatforms())
		platforms.emplace_back(platform.name);

	return readInput(platforms, "Choose the OpenCL platform", MinerCL::getCL().getPlatforms().begin()->name,
	          platformIndex) != exit;
}

bool Burst::Setup::chooseGpuDevice(const int platformIndex, int& deviceIndex)
{
	std::vector<std::string> devices;
	auto platform = MinerCL::getCL().getPlatforms()[platformIndex];

	for (const auto& device : platform.devices)
		devices.emplace_back(device.name);

	return readInput(devices, "Choose the OpenCL device", platform.devices.begin()->name, deviceIndex) != exit;
}

bool Burst::Setup::choosePlots(std::vector<std::string>& plots)
{
	plots.clear();
	std::string path;

	auto pb = Console::print();


	MinerConfig::getConfig().forPlotDirs([&plots](PlotDir& dir)
	{
		plots.emplace_back(dir.getPath());
		return true;
	});

	if (!plots.empty())
	{
		log_notice(MinerLogger::general, "Your current plot list is:");

		for (const auto& plot : plots)
			log_information(MinerLogger::general, plot);

		const auto yesNo = readYesNo("Do you want to use the current list?", true);

		if (yesNo == yes)
			return true;

		if (yesNo == exit)
			return false;
	}

	plots.clear();

	log_notice(MinerLogger::general, "Please enter your plotfile locations");
	log_information(MinerLogger::general, "[ Enter] to accept the current list");

	do
	{
		try
		{
			pb.addTime().print(": ").setColor(ConsoleColor::Yellow).print("Path: ").resetColor();
			getline(std::cin, path);

			if (!path.empty())
			{
				Poco::File location(path);
			
				if (location.isDirectory())
					plots.emplace_back(path);
				else
					log_warning(MinerLogger::general, "The location you entered is not a directory");
			}
		}
		catch (...)
		{
		}
	}
	while (!path.empty());

	return true;
}

bool Burst::Setup::chooseBufferSize(unsigned& memory)
{
	// set buffer size to auto
	MinerConfig::getConfig().setBufferSize(0);

	// get the theoretically used buffer size
	const auto autoBufferSize = MinerConfig::getConfig().getMaxBufferSize();

	// the amount of physical memory
	const auto physicalMemory = getMemorySize();

	// is there enough RAM for the auto buffer size?
	const auto autoBufferSizeAllowed = physicalMemory >= autoBufferSize;

	log_notice(MinerLogger::general, "Please enter your maximum buffer size (in MB)");
	log_notice(MinerLogger::general, "Your physical amount of RAM is: %s MB",
		memToString(physicalMemory, MemoryUnit::Megabyte, 0));
	log_notice(MinerLogger::general, "The optimal amount of memory for your miner is: %s (%.0f%% of your RAM)",
		memToString(autoBufferSize, 0),
		static_cast<double>(autoBufferSize) / physicalMemory * 100);
	
	if (autoBufferSizeAllowed)
		log_notice(MinerLogger::general, "If you use the size 0, the miner will use the optimal amount");
	
	auto entered = false;
	auto pb = Console::print();
	std::string input;

	log_information(MinerLogger::general, "[      0]: Auto adjust the buffer");

	pb.addTime()
		.print(": [  Enter]: Use the optimal value (")
		.setColor(ConsoleColor::Green)
		.print(memToString(autoBufferSize, MemoryUnit::Megabyte, 0))
		.resetColor()
		.print(")")
		.nextLine();

	while (!entered)
	{
		Poco::Int64 number;
		
		if (!readNumber("Buffersize", 0, std::numeric_limits<Poco::Int64>::max(), 0, number))
			return false;

		if (number == 0)
		{
			pb.addTime()
				.print(": Buffersize: %s", memToString(autoBufferSize, MemoryUnit::Megabyte, 0))
				.nextLine();

			auto useAutoBuffer = autoBufferSizeAllowed;

			if (!autoBufferSizeAllowed)
				useAutoBuffer = readYesNo(Poco::format("This will use ~%s of memory, but you have only %s. Still use 0?",
					memToString(autoBufferSize, 0), memToString(physicalMemory, 0)), true) == yes;

			if (useAutoBuffer)
			{
				memory = 0;
				entered = true;
			}
		}
		else
		{
			auto useMemory = true;

			if (memory > physicalMemory / 1024 / 1024)
				useMemory = readYesNo("Are you sure that you want to use a bigger buffer than your physical RAM?", false) == yes;

			entered = useMemory;
		}
	}

	return true;
}

namespace Burst
{
	namespace Helper
	{
		struct ProgressChecker
		{
			mutable std::mutex mutex;
			std::condition_variable conditionVariable;
			float progressRead = 0, progressVerify = 0;

			void onProgressRead(const void*, float& progress)
			{
				std::lock_guard<std::mutex> lock(mutex);
				progressRead = progress;
				checkOverallProgress();
			}

			void onProgressVerify(const void*, float& progress)
			{
				std::lock_guard<std::mutex> lock(mutex);
				progressVerify = progress;
				checkOverallProgress();
			}

			bool done() const
			{
				std::lock_guard<std::mutex> lock(mutex);
				return progressVerify == 100.f && progressRead == 100.f;
			}

			void reset()
			{
				std::lock_guard<std::mutex> lock(mutex);
				progressVerify = 0;
				progressRead = 0;
			}

		private:
			void checkOverallProgress()
			{
				if (progressVerify == 100.f && progressRead == 100.f)
					conditionVariable.notify_all();
			}
		};
	}
}

bool Burst::Setup::choosePlotReader(const size_t plotLocations, unsigned& reader, unsigned& verifier)
{
	const auto cores = std::thread::hardware_concurrency();

	log_notice(MinerLogger::general, "Choose the amount of plot reader");

	std::string type;

	if (cores > 0)
	{
		log_notice(MinerLogger::general, "Your CPU has %u cores", cores);

		int typeIndex;
		type = readInput({"Auto", "Manual"}, "How to set the plotreader/-verifier?", "Auto", typeIndex);
	}
	else
		type = "Manual";

	if (type == exit)
		return false;

	if (type == "Auto")
	{
		log_information(MinerLogger::general, "Performing tests, please wait...");

		Poco::ThreadPool pool(cores, cores);
		Poco::TaskManager tasks(pool);
		Poco::NotificationQueue readQueue, verifyQueue;
		Poco::Clock clock;
		MinerData data;
		Helper::ProgressChecker progressChecker;
		auto progressRead = std::make_shared<PlotReadProgress>();
		auto progressVerify = std::make_shared<PlotReadProgress>();
		std::map<std::pair<int, int>, Poco::Int64> times;
		const auto submitFunction = [](Poco::UInt64, Poco::UInt64, Poco::UInt64, Poco::UInt64,
		                               std::string, bool) {};
		std::vector<Poco::UInt64> scoops;

		PlotReader::globalBufferSize.setMax(MinerConfig::getConfig().getMaxBufferSize());
		progressRead->progressChanged += Poco::delegate(&progressChecker, &Helper::ProgressChecker::onProgressRead);
		progressVerify->progressChanged += Poco::delegate(&progressChecker, &Helper::ProgressChecker::onProgressVerify);

		/*
		 * algorithm:
		 * 1. add plotlocations.size reader
		 * 2. add 1 verifier until full
		 * 3. change 1 reader -> 1 verifier until only 1 reader
		 * 4. remove 1 verifier until 1 verifier
		 * 5. take best time
		 */
		reader = static_cast<unsigned>(plotLocations);

		if (reader >= cores)
			reader = cores - 1;

		verifier = 1;

		auto stage = 0;
		auto done = false;
		const auto plotsize = MinerConfig::getConfig().getTotalPlotsize();
		Poco::Random random;

		const auto processorType = MinerConfig::getConfig().getProcessorType();
		const auto instructionSet = MinerConfig::getConfig().getCpuInstructionSet();

		while (!done)
		{
			// wake up all sleeping reader and verifier
			readQueue.wakeUpAll();
			verifyQueue.wakeUpAll();

			// stop all worker
			tasks.cancelAll();
			tasks.joinAll();

			// only test the reader/verifier combination if it was not already tested
			if (times.find(std::make_pair(reader, verifier)) == times.end())
			{
				// create the reader
				for (auto i = 0; i < reader; i++)
					tasks.start(new PlotReader(data, progressRead, verifyQueue, readQueue));

				// create the verifier
				for (auto i = 0; i < verifier; i++)
				{
					if (processorType == "CPU")
					{
						if (instructionSet == "AVX2")
							tasks.start(new PlotVerifier<PlotVerifierAlgorithm_avx2>(data, verifyQueue, progressVerify, submitFunction));
						else if (instructionSet == "AVX")
							tasks.start(new PlotVerifier<PlotVerifierAlgorithm_avx>(data, verifyQueue, progressVerify, submitFunction));
						else if (instructionSet == "SSE4")
							tasks.start(new PlotVerifier<PlotVerifierAlgorithm_sse4>(data, verifyQueue, progressVerify, submitFunction));
						else
							tasks.start(new PlotVerifier<PlotVerifierAlgorithm_sse2>(data, verifyQueue, progressVerify, submitFunction));
					}
					else if (processorType == "CUDA")
						tasks.start(new PlotVerifier<PlotVerifierAlgorithm_cuda>(data, verifyQueue, progressVerify, submitFunction));
					else if (processorType == "OPENCL")
						tasks.start(new PlotVerifier<PlotVerifierAlgorithm_opencl>(data, verifyQueue, progressVerify, submitFunction));
				}

				// create a new gensig with a scoop that was not read yet
				do
				{
					std::string gensig(64, '0');

					for (auto& c : gensig)
						c = Poco::NumberFormatter::formatHex(random.next(0xF))[0];
					
					data.startNewBlock(1, 1, gensig);
				} while (find(scoops.begin(), scoops.end(), data.getCurrentScoopNum()) != scoops.end());

				// save the scoop to read scoops
				scoops.emplace_back(data.getCurrentScoopNum());

				// reset the progress stuff
				progressRead->reset(1, plotsize);
				progressVerify->reset(1, plotsize);
				progressChecker.reset();

				// start the stopwatch
				clock.update();

				// create the plotread-notifications for the reader
				MinerConfig::getConfig().forPlotDirs([&data, &readQueue](PlotDir& plotDir)
				{
					auto notification = new PlotReadNotification;
					notification->gensig = data.getBlockData()->getGensig();
					notification->scoopNum = data.getCurrentScoopNum();
					notification->blockheight = data.getCurrentBlockheight();
					notification->baseTarget = data.getCurrentBasetarget();
					notification->type = PlotDir::Type::Sequential;
					notification->wakeUpCall = false;
					notification->dir = plotDir.getPath();
					notification->plotList = plotDir.getPlotfiles();
					readQueue.enqueueNotification(notification);
					return true;
				});

				// wait till the read and verify process is done
				while (!progressChecker.done())
				{
					std::unique_lock<std::mutex> lock(progressChecker.mutex);
					progressChecker.conditionVariable.wait(lock);
				}

				// stop the stopwatch
				const auto elapsed = clock.elapsed();

				// save the elapsed time for the reader/verifier combination
				times.insert(make_pair(std::make_pair(reader, verifier), elapsed));

				log_system(MinerLogger::general, "%u reader, %u verifier, time: %fs",
					reader, verifier, static_cast<double>(elapsed) / 1000 / 1000);
			}
			
			// stage 0: fill all free cores with verifiers
			if (stage == 0)
			{
				++verifier;

				if (reader + verifier > cores)
				{
					--verifier;
					++stage;
				}
			}

			// stage 1: switch all reader with verifiers (except 1 reader)
			if (stage == 1)
			{
				++verifier;
				--reader;

				if (reader == 0)
				{
					reader = 1;
					--verifier;
					++stage;
				}
			}

			// stage 2: remove all verifier (except 1 verifier)
			if (stage == 2)
			{
				--verifier;

				if (verifier == 0)
					// if we get to this point, we have exactly 1 reader and 1 verifier
					done = true;
			}
		}

		std::vector<std::string> combinations;
		combinations.reserve(times.size() + 1);

		const auto createIndex = [](const std::pair<int, int>& combination, Poco::Int64 time)
		{
			return Poco::format("%d reader, %d verifier, %.2f seconds",
				combination.first, combination.second, static_cast<double>(time) / 1000 / 1000);
		};

		// choose the best time <-> workload result
		auto best = std::make_pair(0, 0);
		double bestFactor = 0;
		std::string defaultCombination;

		for (const auto& time : times)
		{
			const auto index = createIndex(time.first, time.second);
			const auto factor = static_cast<double>(time.second) * (time.first.first * 0.382 + time.first.second * 0.618);

			if (best.first == 0 || factor < bestFactor)
			{
				best = time.first;
				bestFactor = factor;
				defaultCombination = index;
			}

			combinations.emplace_back(index);
		}
		
		combinations.emplace_back("Manual");

		int choiceIndex;
		const auto choice = readInput(combinations, "Choose the plotreader/-reader combination", defaultCombination, choiceIndex);

		if (choice == exit)
			return false;

		if (choice == "Manual")
			type = "Manual";
		else
		{
			auto iter = times.begin();
			advance(iter, choiceIndex);
		
			reader = iter->first.first;
			verifier = iter->first.second;
		}
	}

	if (type == "Manual")
	{
		Poco::Int64 number;

		log_notice(MinerLogger::general, "Choose the plotreader/-reader combination");
		log_information(MinerLogger::general, "[      0]: Auto adjust the reader/verifier");
		log_information(MinerLogger::general, "[  Enter]: Auto adjust the reader/verifier");

		if (!readNumber("Plotreader", 0, std::numeric_limits<Poco::Int64>::max(), 0, number))
			return false;

		reader = static_cast<unsigned>(number);

		if (!readNumber("Plotverifier", 0, std::numeric_limits<Poco::Int64>::max(), 0, number))
			return false;

		verifier = static_cast<unsigned>(number);
	}

	return true;
}

bool Burst::Setup::chooseWebserver(std::string& ip, std::string& user, std::string& password)
{
	std::vector<std::string> ips;

	auto thisHost = Poco::Net::DNS::thisHost();
	std::string defaultIp;

	for (const auto& address : thisHost.addresses())
	{
		ips.emplace_back(address.toString());

		if (address.family() == Poco::Net::AddressFamily::IPv4 && defaultIp.empty())
			defaultIp = address.toString();
	}
	
	ips.emplace_back("Manual");
	int index;
	ip = readInput(ips, "Choose your IP", defaultIp, index);
		
	if (ip == exit)
		return false;

	if (ip == "Manual")
	{
		auto entered = false;
		const auto pb = Console::print();

		while (!entered)
		{
			pb.addTime().print(": ");
			getline(std::cin, ip);
			Poco::Net::IPAddress address;
			entered = Poco::Net::IPAddress::tryParse(ip, address);
		}
	}
	
	std::vector<std::string> ports;

	const auto checkPort = [](Poco::UInt16 portToCheck, Poco::UInt16& port)
	{
		try
		{
			Poco::Net::ServerSocket serverSocket;
			serverSocket.bind(portToCheck);
			serverSocket.listen();
			port = serverSocket.address().port();
			serverSocket.close();
			return true;
		}
		catch (...)
		{
			return false;
		}
	};

	Poco::UInt16 port;

	if (checkPort(8080, port))
		ports.emplace_back(std::to_string(port));

	for (auto i = 0; i < 5; ++i)
		if (checkPort(0, port))
			ports.emplace_back(std::to_string(port));

	ports.emplace_back("Random");
	ports.emplace_back("Manual");

	int portIndex;
	const auto portInput = readInput(ports, "Choose your port", *ports.begin(), portIndex);

	if (portInput == exit)
		return false;

	if (portInput == "Manual")
	{
		log_information(MinerLogger::general, "[  Enter]: Use a random port");

		auto valid = false;
		while (!valid)
		{
			Poco::Int64 number;
			Poco::UInt16 numberPort;

			if (!readNumber("Port", 0, std::numeric_limits<Poco::UInt16>::max(), 0, number))
				return false;

			if (checkPort(static_cast<Poco::UInt16>(number), numberPort))
			{
				port = static_cast<Poco::UInt16>(numberPort);
				valid = true;
			}
		}
	}
	else if (portInput == "Random")
	{
		if (!checkPort(0, port))
			return false;
	}
	else
		port = Poco::NumberParser::parse(portInput);

	Poco::URI uri;
	uri.setScheme("http");
	uri.setHost(ip);
	uri.setPort(port);

	// username
	log_notice(MinerLogger::general, "Enter the username for the webserver");
	readText("Username", [](const std::string& toVerify, std::string& verified)
	{
		verified = toVerify;
		return true;
	});

	// password
	log_notice(MinerLogger::general, "Enter the password for the webserver");
	Helper::enterPasswordSecured(password);

	ip = uri.toString();
	log_information(MinerLogger::general, "Your ip is: %s", ip);
	return true;
}

bool Burst::Setup::chooseProgressbar(bool& fancy, bool& steady)
{
	log_notice(MinerLogger::general, "Have a look at different the progressbar types");

	Progress progress;
	ProgressPrinter printer;
	Poco::Random random;
	
	const auto simulateProgressbar = [&progress, &printer, &random]()
	{
		progress.read = 0;
		progress.verify = 0;
		progress.bytesPerSecondRead = 200 * 1024 * 1024;
		progress.bytesPerSecondVerify = 300 * 1024 * 1024;

		while (progress.read < 100.f || progress.verify < 100.f)
		{
			progress.bytesPerSecondRead += 1 * 1024 * 1024 * (random.nextBool() ? 1 : -1);
			progress.bytesPerSecondVerify += 2 * 1024 * 1024 * (random.nextBool() ? 1 : -1);

			//progress.bytesPerSecondRead += random.next(2) * (random.nextBool() ? 1 : -1);
			//progress.bytesPerSecondVerify += random.next(2) * (random.nextBool() ? 1 : -1);
			progress.bytesPerSecondCombined = (progress.bytesPerSecondRead + progress.bytesPerSecondVerify) / 2.0;
			
			MinerLogger::writeProgress(progress);

			const auto verifyStep = random.nextDouble() * 5;

			if (progress.read < 100.f)
			{
				progress.read += verifyStep * 2;

				if (progress.read > 100.f)
					progress.read = 100.f;
			}

			if (progress.verify < 100.f)
			{
				progress.verify += verifyStep;
				
				if (progress.verify > 100.f)
					progress.verify = 100.f;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		MinerLogger::writeProgress(progress);
		Console::nextLine();
	};

	const auto startProgressSimulation = [&simulateProgressbar](bool useFancy, bool useSteady)
	{
		log_information(MinerLogger::general, "fancy: %b, steady: %b", useFancy, useSteady);
		MinerConfig::getConfig().setProgressbar(useFancy, useSteady);
		simulateProgressbar();
	};

	for (auto i = 0; i < 4; i++)
		startProgressSimulation(i & 1, i & 2);

	const std::string nFnS = "not fancy, not steady";
	const std::string FnS = "fancy, not steady";
	const std::string nFS = "not fancy, steady";
	const std::string FS = "fancy, steady";

	const std::vector<std::string> types = {nFnS, FnS, nFS, FS};

	int index;
	const auto input = readInput(types, "Choose your progressbar style", FS, index);

	if (input == exit)
		return false;

	if (input == nFnS)
	{
		fancy = false;
		steady = false;
	}
	else if (input == FnS)
	{
		fancy = true;
		steady = false;
	}
	else if (input == nFS)
	{
		fancy = false;
		steady = true;
	}
	else if (input == FS)
	{
		fancy = true;
		steady = true;
	}
	else
	{
		fancy = true;
		steady = true;
	}

	return true;
}

bool Burst::Setup::chooseUris(std::string& submission, std::string& miningInfo, std::string& wallet, std::string& passphrase)
{
	const std::vector<std::string> miningTypes = {"Pool", "Solo"};

	int index;
	const auto miningType = readInput(miningTypes, "Choose your mining type", "Pool", index);

	if (miningType == exit)
		return false;

	const auto validateUri = [](const std::string& toValidate, const std::string& requestPath, std::string& result)
	{
		const auto checkUri = [&requestPath](const std::string& uriToValidate, Poco::UInt16 port, Poco::URI& resultUri)
		{
			try
			{
				auto url = Url{ uriToValidate, "http", port };

				// change localhosts to 127.0.0.1
				if (url.getUri().getHost() == "localhost" ||
					url.getUri().getHost() == "0.0.0.0")
					url.getUri().setHost("127.0.0.1");

				resultUri = url.getUri().toString();

				Poco::JSON::Parser jsonParser;
				
				if (!url.empty())
				{
					// create a session
					auto session = url.createSession();
					session->setTimeout(Poco::Timespan(5, 0));
					std::string data;

					Poco::Net::HTTPRequest requestData;
					requestData.setURI(requestPath);

					// check mining info
					Request request(move(session));
					auto response = request.send(requestData);

					if (response.receive(data))
					{
						jsonParser.parse(data);
						return true;
					}
				}

				return false;
			}
			catch (...)
			{
				return false;
			}
		};

		try
		{
			Poco::URI uri_(toValidate);

			log_information(MinerLogger::general, "Testing the connection...");

			if (checkUri(uri_.toString(), uri_.getPort(), uri_))
			{
				result = uri_.toString();
				log_success(MinerLogger::general, "%s: ok", result);
				return true;
			}

			const std::vector<Poco::UInt16> wellKnownPorts = { 8080, 8124, 8125, 8880 };

			for (const auto& port : wellKnownPorts)
			{
				if (checkUri(toValidate, port, uri_))
				{
					result = uri_.toString();
					log_success(MinerLogger::general, "%s: ok", result);
					return true;
				}
			}

			log_error(MinerLogger::general, "No connection!");
			return false;
		}
		catch (...)
		{
			return false;
		}
	};

	// Solo mining
	if (miningType == "Solo")
	{
		log_notice(MinerLogger::general, "Wallet");

		miningInfo = readText("URI", [&validateUri](const std::string& toValidate, std::string& result)
		{
			return validateUri(toValidate, "/burst?requestType=getMiningInfo", result);
		});

		submission = miningInfo;
		wallet = miningInfo;

		if (!chooseSoloMining(passphrase))
			return false;

		return true;
	}

	// mining info uri
	log_notice(MinerLogger::general, "Mininginfo");
	miningInfo = readText("URI", [&validateUri](const std::string& toValidate, std::string& result)
	{
		return validateUri(toValidate, "/burst?requestType=getMiningInfo", result);
	});

	// submission uri
	log_notice(MinerLogger::general, "Submission");
	const auto submissionPath = "/burst?requestType=submitNonce&accountId=123&nonce=123";

	if (!validateUri(miningInfo, submissionPath, submission))
	{
		submission = readText("URI", [&validateUri, &submissionPath](const std::string& toValidate, std::string& result)
		{
			return validateUri(toValidate, submissionPath, result);
		});

		if (submission == exit)
			return false;
	}

	// wallet uri
	log_notice(MinerLogger::general, "Wallet");
	wallet = readText("URI", [&validateUri](const std::string& toValidate, std::string& result)
	{
		return validateUri(toValidate, "/burst?requestType=getAccount&accountId=123", result);
	});

	return true;
}

bool Burst::Setup::chooseSoloMining(std::string& passphrase)
{
	Helper::enterPasswordSecured(passphrase);
	return true;
}
