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
#include <conio.h>
#include "mining/MinerCL.hpp"
#include "logging/MinerLogger.hpp"
#include "plots/PlotVerifier.hpp"
#include <Poco/File.h>
#include <Poco/Delegate.h>
#include <Poco/Random.h>

const std::string Burst::Setup::exit = "Exit";
const std::string Burst::Setup::yes = "Yes";
const std::string Burst::Setup::no = "No";

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
	std::string ip;
	bool fancyProgressbar, steadyProgressbar;

	if (!chooseProcessorType(processorType))
		return false;

	if (processorType == "CPU")
	{
		if (!chooseCpuInstructionSet(instructionSet))
			return false;

		config.setCpuInstructionSet(instructionSet);
	}

	if (processorType == "OPENCL")
	{
		if (!chooseGpuPlatform(platformIndex))
			return false;

		if (!chooseGpuDevice(platformIndex, deviceIndex))
			return false;

		config.setGpuPlatform(platformIndex);
		config.setGpuDevice(deviceIndex);
	}

	config.setProcessorType(processorType);

	if (!choosePlots(plotLocations))
		return false;

	config.setPlotDirs(plotLocations);

	if (!plotLocations.empty())
	{
		if (!chooseBufferSize(memory))
			return false;
	
		config.setBufferSize(memory);

		if (!choosePlotReader(plotLocations.size(), reader, verifier))
			return false;

		config.setMaxPlotReaders(reader);
		config.setMininigIntensity(verifier);
	}
	
	if (!chooseIp(ip))
		return false;

	if (ip.empty())
	{
	}

	if (!chooseProgressbar(fancyProgressbar, steadyProgressbar))
		return false;

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
		const auto choice = _getch();
		exit = choice == 0x1B;
		useDefault = choice == '\r';
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
			else if (input == "\r") // exit
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
					tasks.start(new PlotVerifier<PlotVerifierAlgorithm_avx2>(data, verifyQueue, progressVerify, submitFunction));

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

bool Burst::Setup::chooseIp(std::string& ip)
{
	// TODO: default is the first local ip
	return false;
}

bool Burst::Setup::chooseProgressbar(bool& fancy, bool& steady)
{
	// TODO: show different progressbar combinations
	return false;
}
