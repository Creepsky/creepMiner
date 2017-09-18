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

#include "MinerCL.hpp"
#include "logging/MinerLogger.hpp"
#include <sstream>
#include <fstream>
#include <thrust/execution_policy.h>

namespace Burst
{
	struct MinerCLHelper
	{
		template <typename TSubject, typename TInfoFunc, typename TArg>
		static auto infoHelperFunc(const TSubject& subject, std::string& info, TInfoFunc infoFunc, TArg arg)
		{
			info = std::string(255, '\0');
			size_t size = 0;

			if (infoFunc(subject, arg, info.size(), &info[0], &size) != CL_SUCCESS)
				return false;

			// cut away the \0
			if (size > 0)
				--size;

			info.resize(size);
			return true;
		};
	};
}

Burst::MinerCL::~MinerCL()
{
	if (command_queue_ != nullptr)
		clFlush(command_queue_);

	if (command_queue_ != nullptr)
		clFinish(command_queue_);

	if (kernel_ != nullptr)
		clReleaseKernel(kernel_);

	if (program_ != nullptr)
		clReleaseProgram(program_);

	if (command_queue_)
		clReleaseCommandQueue(command_queue_);

	if (context_ != nullptr)
		clReleaseContext(context_);
}

bool Burst::MinerCL::create(size_t platformIdx, size_t deviceIdx)
{
	std::vector<cl_platform_id> platforms;
	std::vector<cl_device_id> devices;

	// try open the kernel file..
	std::ifstream stream("mining.cl");
	std::string miningKernel(std::istreambuf_iterator<char>(stream), {});

	if (miningKernel.empty())
	{
		log_error(MinerLogger::miner, "Could not initialize OpenCL - the kernel file is missing!");
		return false;
	}

	// first, delete the old context 
	if (context_ != nullptr)
	{
		clReleaseContext(context_);
		context_ = nullptr;
	}

	// get the platforms and print infos 
	{
		cl_uint sizePlatforms;

		// get the number of valid opencl platforms 
		if (clGetPlatformIDs(0, nullptr, &sizePlatforms) != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not detect the number of valid OpenCL platforms!");
			return false;
		}

		if (sizePlatforms == 0)
		{
			log_fatal(MinerLogger::miner, "No valid OpenCL platforms detected!");
			return false;
		}

		// get all valid opencl platforms 
		platforms.resize(sizePlatforms);

		if (clGetPlatformIDs(sizePlatforms, platforms.data(), nullptr) != CL_SUCCESS)
		{
			log_critical(MinerLogger::miner, "Could not get a list of valid OpenCL platforms!");
			return false;
		}

		std::stringstream sstream;
		sstream << "Available OpenCL platforms:";

		// print platform infos
		for (auto i = 0u; i < platforms.size(); ++i)
		{
			sstream << std::endl;
			std::string name;
			std::string version;

			const auto& platform = platforms[i];
			
			if (!MinerCLHelper::infoHelperFunc(platform, name, clGetPlatformInfo, CL_PLATFORM_NAME) ||
				!MinerCLHelper::infoHelperFunc(platform, version, clGetPlatformInfo, CL_PLATFORM_VERSION))
			{
				sstream << '\t' << "Could not get all infos of a OpenCL platform... skipping it!" << std::endl;
				continue;
			}

			sstream << '\t' << "Platform[" << i << "]: " << name << ", Version: " << version;
		}

		log_system(MinerLogger::miner, sstream.str());
	}

	// get the devices of the desired platform 
	{
		if (platformIdx < platforms.size())
		{
			log_system(MinerLogger::miner, "Using platform[%z]", platformIdx);
		}
		else
		{
			log_fatal(MinerLogger::miner, "Platform index is out of bounds (%z, max: %z)",
				platformIdx, platforms.size() - 1);
			return false;
		}

		const auto platform = platforms[platformIdx];
		cl_uint sizeDevices = 0;

		if (clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, nullptr, &sizeDevices) != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not detect the number of valid OpenCL devices!");
			return false;
		}

		devices.resize(sizeDevices);

		if (clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, sizeDevices, devices.data(), nullptr) != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not detect the number of valid OpenCL devices!");
			return false;
		}

		std::stringstream sstream;
		sstream << "Available OpenCL devices on platform[" << platformIdx << "]:";

		// print platform infos
		for (auto i = 0u; i < devices.size(); ++i)
		{
			sstream << std::endl;
			std::string name;

			const auto& device = devices[i];

			if (!MinerCLHelper::infoHelperFunc(device, name, clGetDeviceInfo, CL_DEVICE_NAME))
			{
				sstream << '\t' << "Could not get all infos of a OpenCL device... skipping it!" << std::endl;
				continue;
			}

			sstream << '\t' << "Device[" << i << "]: " << name;
		}

		log_system(MinerLogger::miner, sstream.str());

		if (deviceIdx < devices.size())
		{
			log_system(MinerLogger::miner, "Using device[%z]", deviceIdx);
		}
		else
		{
			log_fatal(MinerLogger::miner, "Device index is out of bounds (%z, max: %z)",
				deviceIdx, devices.size() - 1);
			return false;
		}
	}

	// create the context 
	{
		auto err = 0;

		context_ = clCreateContext(nullptr, static_cast<cl_uint>(devices.size()), devices.data(), nullptr, nullptr, &err);

		if (err != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not create an OpenCL context!");
			return false;
		}

	}

	// create the command queue
	{
		auto ret = 0;
		command_queue_ = clCreateCommandQueue(context_, devices[deviceIdx], 0, &ret);

		if (ret != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not create an OpenCL command queue!");
			return false;
		}
	}

	// create program
	{
		cl_int retCreate;
		auto size = miningKernel.size();
		program_ = clCreateProgramWithSource(context_, 1, reinterpret_cast<const char**>(&miningKernel),
		                                     reinterpret_cast<const size_t*>(&size), &retCreate);

		const auto retBuild = retCreate && clBuildProgram(program_, 1, &devices[deviceIdx], nullptr, nullptr, nullptr);

		if (retCreate != CL_SUCCESS || retBuild != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not create the OpenCL program (mining.cl)!");
			return false;
		}
	}

	// create kernel
	{
		cl_int ret;
		kernel_ = clCreateKernel(program_, "verify", &ret);

		if (ret != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not create the OpenCL kernel!");
			return false;
		}
	}

	log_system(MinerLogger::miner, "Successfully initialized OpenCL!");
	return true;
}

cl_context Burst::MinerCL::getContext() const
{
	return context_;
}

cl_command_queue Burst::MinerCL::getCommandQueue() const
{
	return command_queue_;
}

cl_kernel Burst::MinerCL::getKernel() const
{
	return kernel_;
}

Burst::MinerCL& Burst::MinerCL::getCL()
{
	static MinerCL minerCL;
	return minerCL;
}
