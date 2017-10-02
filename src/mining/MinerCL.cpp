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
#include "gpu/impl/gpu_opencl_impl.hpp"

namespace Burst
{
#ifdef USE_OPENCL
	struct MinerCLHelper
	{
		template <typename TSubject, typename TInfoFunc, typename TArg>
		static auto infoHelperFunc(const TSubject& subject, std::string& info, TInfoFunc infoFunc, TArg arg, int& ret)
		{
			info = std::string(255, '\0');
			size_t size = 0;

			ret = infoFunc(subject, arg, info.size(), &info[0], &size);

			if (ret != CL_SUCCESS)
				return false;

			// cut away the \0
			if (size > 0)
				--size;

			info.resize(size);
			return true;
		};
	};
#endif
}

Burst::MinerCL::~MinerCL()
{
#ifdef USE_OPENCL
	for (auto command_queue : command_queues_)
	{
		if (command_queue != nullptr)
		{
			clFlush(command_queue);
			clFinish(command_queue);
		}
	}

	if (kernel_ != nullptr)
		clReleaseKernel(kernel_);

	if (program_ != nullptr)
		clReleaseProgram(program_);

	for (auto command_queue : command_queues_)
		if (command_queue != nullptr)
			clReleaseCommandQueue(command_queue);

	if (context_ != nullptr)
		clReleaseContext(context_);
#endif
}

bool Burst::MinerCL::create(unsigned platformIdx, unsigned deviceIdx)
{
	deviceIdx_ = deviceIdx;
	platformIdx_ = platformIdx;

#ifdef USE_OPENCL
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

		auto ret = clGetPlatformIDs(0, nullptr, &sizePlatforms);

		// get the number of valid opencl platforms 
		if (ret != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not detect the number of valid OpenCL platforms!\n\tError-code:\t%d", ret);
			return false;
		}

		if (sizePlatforms == 0)
		{
			log_fatal(MinerLogger::miner, "No valid OpenCL platforms detected!");
			return false;
		}

		// get all valid opencl platforms 
		platforms_.resize(sizePlatforms);

		ret = clGetPlatformIDs(sizePlatforms, platforms_.data(), nullptr);

		if (ret != CL_SUCCESS)
		{
			log_critical(MinerLogger::miner, "Could not get a list of valid OpenCL platforms!\n\tError-code:\t%d", ret);
			return false;
		}

		std::stringstream sstream;
		sstream << "Available OpenCL platforms:";

		// print platform infos
		for (auto i = 0u; i < platforms_.size(); ++i)
		{
			sstream << std::endl;
			std::string name;
			std::string version;

			const auto& platform = platforms_[i];
			cl_int ret_platform, ret_version;
			
			if (!MinerCLHelper::infoHelperFunc(platform, name, clGetPlatformInfo, CL_PLATFORM_NAME, ret_platform) ||
				!MinerCLHelper::infoHelperFunc(platform, version, clGetPlatformInfo, CL_PLATFORM_VERSION, ret_version))
			{
				if (ret_platform != CL_SUCCESS)
					sstream << '\t' << "Could not get OpenCL platform name\n\tError-code:\t%d" << std::endl;

				if (ret_version != CL_SUCCESS)
					sstream << '\t' << "Could not get OpenCL platform version\n\tError-code:\t%d" << std::endl;

				sstream << '\t' << "Skipping it!" << std::endl;
				continue;
			}

			sstream << '\t' << "Platform[" << i << "]: " << name << ", Version: " << version;
		}

		log_system(MinerLogger::miner, sstream.str());
	}

	// get the devices of the desired platform 
	{
		if (platformIdx < platforms_.size())
		{
			log_system(MinerLogger::miner, "Using platform[%u]", platformIdx);
		}
		else
		{
			log_fatal(MinerLogger::miner, "Platform index is out of bounds (%u, max: %z)",
				platformIdx, platforms_.size() - 1);
			return false;
		}

		const auto platform = platforms_[platformIdx];
		cl_uint sizeDevices = 0;

		auto ret = clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0, nullptr, &sizeDevices);

		if (ret != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not detect the number of valid OpenCL devices!\n\tError-code:\t%d", ret);
			return false;
		}

		devices_.resize(sizeDevices);

		ret = clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, sizeDevices, devices_.data(), nullptr);

		if (ret != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not detect the number of valid OpenCL devices!\n\tError-code:\t%d", ret);
			return false;
		}

		std::stringstream sstream;
		sstream << "Available OpenCL devices on platform[" << platformIdx << "]:";

		// print platform infos
		for (auto i = 0u; i < devices_.size(); ++i)
		{
			sstream << std::endl;
			std::string name;

			const auto& device = devices_[i];
			cl_int ret_device_name;

			if (!MinerCLHelper::infoHelperFunc(device, name, clGetDeviceInfo, CL_DEVICE_NAME, ret_device_name))
			{
				sstream << '\t' << "Could not get all infos of a OpenCL device (Error-code: " << ret_device_name <<
					")... skipping it!" << std::endl;
				continue;
			}

			sstream << '\t' << "Device[" << i << "]: " << name;
		}

		log_system(MinerLogger::miner, sstream.str());

		if (deviceIdx < devices_.size())
		{
			log_system(MinerLogger::miner, "Using device[%u]", deviceIdx);
		}
		else
		{
			log_fatal(MinerLogger::miner, "Device index is out of bounds (%u, max: %z)",
				deviceIdx, devices_.size() - 1);
			return false;
		}
	}

	// create the context 
	{
		auto err = 0;

		context_ = clCreateContext(nullptr, static_cast<cl_uint>(devices_.size()), devices_.data(), nullptr, nullptr, &err);

		if (err != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not create an OpenCL context!\n\tError-code:\t%d", err);
			return false;
		}
	}

	// create program
	{
		cl_int retCreate;
		auto size = miningKernel.size();
		auto miningKernel_Cstr = miningKernel.c_str();

		program_ = clCreateProgramWithSource(context_, 1, reinterpret_cast<const char**>(&miningKernel_Cstr),
		                                     reinterpret_cast<const size_t*>(&size), &retCreate);

		const auto retBuild = clBuildProgram(program_, 1, &devices_[deviceIdx], nullptr, nullptr, nullptr);

		if (retCreate != CL_SUCCESS || retBuild != CL_SUCCESS)
		{
			size_t logSize;
			clGetProgramBuildInfo(program_, devices_[deviceIdx], CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
			
			std::string log;
			log.resize(logSize);

			clGetProgramBuildInfo(program_, devices_[deviceIdx], CL_PROGRAM_BUILD_LOG, logSize, &log[0], nullptr);

			log_fatal(MinerLogger::miner, "Could not create the OpenCL program (mining.cl)!\n%s", log);
			return false;
		}
	}

	// create kernel
	{
		cl_int ret;
		kernel_ = clCreateKernel(program_, "calculate_deadlines", &ret);

		if (ret != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not create the OpenCL kernel!\n\tError-code:\t%d", ret);
			return false;
		}
	}

	log_system(MinerLogger::miner, "Successfully initialized OpenCL!");
#endif

	initialized_ = true;

	return true;
}

cl_command_queue Burst::MinerCL::createCommandQueue()
{
#ifdef USE_OPENCL
	auto ret = 0;
	auto command_queue = clCreateCommandQueue(context_, devices_[deviceIdx_], 0, &ret);

	if (ret != CL_SUCCESS)
	{
		log_fatal(MinerLogger::miner, "Could not create an OpenCL command queue!\n\tError-code:\t%d", ret);
		return nullptr;
	}

	command_queues_.emplace_back(command_queue);
	return command_queue;
#else
	return nullptr;
#endif
}

cl_context Burst::MinerCL::getContext() const
{
	return context_;
}

cl_kernel Burst::MinerCL::getKernel() const
{
	return kernel_;
}

bool Burst::MinerCL::initialized() const
{
	return initialized_;
}

Burst::MinerCL& Burst::MinerCL::getCL()
{
	static MinerCL minerCL;
	return minerCL;
}
