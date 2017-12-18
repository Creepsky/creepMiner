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

const std::string Burst::Setup::exit = "Exit";

bool Burst::Setup::setup(MinerConfig& config)
{
	auto logger = &Poco::Logger::get("general");
	log_information(logger, "Welcome to the creepMiner setup");

	std::string processorType;
	std::string instructionSet;
	int platformIndex, deviceIndex;
	std::vector<std::string> plotLocations;

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

	if (!config.save())
		log_warning(MinerLogger::miner, "Your settings could not be saved! They are only valid for this session.");

	return true;
}

std::string Burst::Setup::readInput(const std::vector<std::string>& options, const std::string& header,
                                    const std::string& defaultValue, int& index)
{
	auto pb = Console::print();
	pb.addTime().setColor(ConsoleColor::LightCyan).print(": %s", header).resetColor().nextLine();

	for (size_t i = 0; i < options.size(); ++i)
	{
		pb.addTime().print(": [%7z]: %s", i + 1, options[i]);

		if (options[i] == defaultValue)
			pb.setColor(ConsoleColor::LightGreen).print(" *").resetColor();

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
	log_notice(MinerLogger::general, "Please enter your plotfile locations");
	log_notice(MinerLogger::general, "[ Enter] to accept the current list");

	plots.clear();
	std::string path;

	auto pb = Console::print();

	do
	{
		try
		{
			pb.addTime().setColor(ConsoleColor::Yellow).print(": Path: ").resetColor();
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
