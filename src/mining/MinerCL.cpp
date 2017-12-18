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
#include <fstream>
#include "gpu/impl/gpu_opencl_impl.hpp"
#include "shabal/cuda/Shabal.hpp"

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

std::vector<cl_device_id> Burst::ClPlatform::getDeviceIds() const
{
	std::vector<cl_device_id> deviceIds;
	deviceIds.reserve(devices.size());

	for (const auto& device : devices)
		deviceIds.emplace_back(device.id);

	return deviceIds;
}

bool Burst::ClPlatform::getPlatforms(std::vector<ClPlatform>& platforms, std::string& error)
{
	platforms.clear();

	std::vector<cl_platform_id> platform_ids;

	// get the platforms and print infos 
	cl_uint sizePlatforms;

	auto ret = clGetPlatformIDs(0, nullptr, &sizePlatforms);

	// get the number of valid opencl platforms 
	if (ret != CL_SUCCESS)
	{
		error = Poco::format("Could not detect the number of valid OpenCL platforms!\n\tError-code:\t%d", ret);
		return false;
	}

	if (sizePlatforms == 0)
	{
		error = "No valid OpenCL platforms detected!";
		return false;
	}

	// get all valid opencl platforms 
	platform_ids.resize(sizePlatforms);
	platforms.resize(sizePlatforms);

	ret = clGetPlatformIDs(sizePlatforms, platform_ids.data(), nullptr);

	if (ret != CL_SUCCESS)
	{
		error = Poco::format("Could not get a list of valid OpenCL platforms!\n\tError-code:\t%d", ret);
		return false;
	}

	// print platform infos
	for (size_t i = 0; i < platform_ids.size(); ++i)
	{
		cl_int ret_platform, ret_version;

		const auto& platform_id = platform_ids[i];
		auto& platform = platforms[i];

		platform.id = platform_id;
		MinerCLHelper::infoHelperFunc(platform_id, platform.name, clGetPlatformInfo, CL_PLATFORM_NAME, ret_platform);
		MinerCLHelper::infoHelperFunc(platform_id, platform.version, clGetPlatformInfo, CL_PLATFORM_VERSION, ret_version);

		// get the devices of the desired platform 
		cl_uint sizeDevices = 0;

		ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL, 0, nullptr, &sizeDevices);

		if (ret != CL_SUCCESS)
			return ret;

		std::vector<cl_device_id> device_ids;

		device_ids.resize(sizeDevices);
		platform.devices.resize(sizeDevices);

		ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL, sizeDevices, device_ids.data(), nullptr);

		if (ret != CL_SUCCESS)
		{
			error = Poco::format("Could not detect the number of valid OpenCL devices!\n\tError-code:\t%d", ret);
			return false;
		}
		
		// print platform infos
		for (size_t j = 0; j < device_ids.size(); ++j)
		{
			std::string deviceName;

			const auto& device_id = device_ids[j];
			auto& device = platform.devices[j];

			cl_int ret_device_name;

			device.id = device_id;
			MinerCLHelper::infoHelperFunc(device_id, device.name, clGetDeviceInfo, CL_DEVICE_NAME, ret_device_name);
		}
	}

	return true;
}

Burst::MinerCL::MinerCL()
{
	std::string error;
	ClPlatform::getPlatforms(platforms_, error);
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

	if (kernel_calculate_deadlines_ != nullptr)
		clReleaseKernel(kernel_calculate_deadlines_);

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
		log_system(MinerLogger::miner, "Available OpenCL platforms:");

		// print platform infos
		for (auto i = 0u; i < platforms_.size(); ++i)
		{
			const auto& platform = platforms_[i];
			log_system(MinerLogger::miner, "Platform[%u]: %s, Version: %s", i, platform.name, platform.version);
		}
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

		log_system(MinerLogger::miner, "Available OpenCL devices on platform[%u]:", platformIdx);
		
		// print platform infos
		for (size_t i = 0; i < platform.devices.size(); ++i)
		{
			const auto& device = platform.devices[i];
			log_system(MinerLogger::miner, "Device[%z]: %s", i, device.name);
		}

		if (deviceIdx < platform.devices.size())
		{
			log_system(MinerLogger::miner, "Using device[%u]", deviceIdx);
		}
		else
		{
			log_fatal(MinerLogger::miner, "Device index is out of bounds (%u, max: %z)",
				deviceIdx, platform.devices.size() - 1);
			return false;
		}
	}

	// create the context 
	{
		auto err = 0;

		context_ = clCreateContext(nullptr, static_cast<cl_uint>(getPlatform()->devices.size()),
		                           getPlatform()->getDeviceIds().data(), nullptr, nullptr, &err);

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

		const auto retBuild = clBuildProgram(program_, 1, getDeviceId(), nullptr, nullptr, nullptr);

		if (retCreate != CL_SUCCESS || retBuild != CL_SUCCESS)
		{
			size_t logSize;
			clGetProgramBuildInfo(program_, *getDeviceId(), CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
			
			std::string log;
			log.resize(logSize);

			clGetProgramBuildInfo(program_, *getDeviceId(), CL_PROGRAM_BUILD_LOG, logSize, &log[0], nullptr);

			log_fatal(MinerLogger::miner, "Could not create the OpenCL program (mining.cl)!\n%s", log);
			return false;
		}
	}

	// create kernel
	{
		cl_int ret;
		kernel_calculate_deadlines_ = clCreateKernel(program_, "calculate_deadlines", &ret);

		if (ret != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not create the OpenCL kernel 'calculate_deadlines'!\n\tError-code:\t%d", ret);
			return false;
		}

		kernel_best_deadline_ = clCreateKernel(program_, "reduce_best", &ret);

		if (ret != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not create the OpenCL kernel 'reduce_best'!\n\tError-code:\t%d", ret);
			return false;
		}
	}

	// get work group size
	{
		auto ret = clGetKernelWorkGroupInfo(kernel_calculate_deadlines_, *getDeviceId(),
		                                    CL_KERNEL_WORK_GROUP_SIZE,
		                                    sizeof(kernel_Calculate_WorkGroupSize_), &kernel_Calculate_WorkGroupSize_,
		                                    nullptr);

		if (ret == CL_SUCCESS)
			ret = clGetKernelWorkGroupInfo(kernel_calculate_deadlines_, *getDeviceId(),
			                               CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
			                               sizeof(kernel_Calculate_PrefferedWorkGroupSize_), &kernel_Calculate_PrefferedWorkGroupSize_,
			                               nullptr);

		if (ret != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not get the maximum work group size for the kernel calculate deadlines!");
			return false;
		}

		ret = clGetKernelWorkGroupInfo(kernel_best_deadline_, *getDeviceId(),
		                               CL_KERNEL_WORK_GROUP_SIZE,
		                               sizeof(kernel_FindBest_WorkGroupSize_), &kernel_FindBest_WorkGroupSize_,
		                               nullptr);

		if (ret == CL_SUCCESS)
			ret = clGetKernelWorkGroupInfo(kernel_best_deadline_, *getDeviceId(),
			                               CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
			                               sizeof(kernel_FindBest_PrefferedWorkGroupSize_), &kernel_FindBest_PrefferedWorkGroupSize_,
			                               nullptr);

		if (ret != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not get the maximum work group size for the kernel find best deadline!");
			return false;
		}
	}

	// compute units
	{
		const auto ret = clGetDeviceInfo(*getDeviceId(), CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(size_t), &compute_units_, nullptr);

		if (ret != CL_SUCCESS)
		{
			log_fatal(MinerLogger::miner, "Could not get the maximum compute units for the device!");
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
	auto command_queue = clCreateCommandQueue(context_, getDevice()->id, 0, &ret);

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

cl_kernel Burst::MinerCL::getKernel_Calculate() const
{
	return kernel_calculate_deadlines_;
}

cl_kernel Burst::MinerCL::getKernel_GetMin() const
{
	return kernel_best_deadline_;
}

size_t Burst::MinerCL::getKernelCalculateWorkGroupSize(const bool preferred) const
{
	return preferred ? kernel_Calculate_PrefferedWorkGroupSize_ : kernel_Calculate_WorkGroupSize_;
}

size_t Burst::MinerCL::getKernelFindBestWorkGroupSize(const bool preferred) const
{
	return preferred ? kernel_FindBest_PrefferedWorkGroupSize_ : kernel_FindBest_WorkGroupSize_;
}

size_t Burst::MinerCL::getComputeUnits() const
{
	return compute_units_;
}

const Burst::ClPlatform* Burst::MinerCL::getPlatform() const
{
	if (platformIdx_ >= platforms_.size())
		return nullptr;

	return &platforms_[platformIdx_];
}

const cl_device_id* Burst::MinerCL::getDeviceId() const
{
	const auto device = getDevice();

	if (device == nullptr)
		return nullptr;

	return &device->id;
}

const Burst::ClDevice* Burst::MinerCL::getDevice() const
{
	if (platformIdx_ >= platforms_.size())
		return nullptr;

	const auto& platform = platforms_[platformIdx_];

	if (deviceIdx_ >= platform.devices.size())
		return nullptr;

	return &platform.devices[platformIdx_];
}

const std::vector<Burst::ClPlatform>& Burst::MinerCL::getPlatforms() const
{
	return platforms_;
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

cl_program Burst::MinerCL::getProgram() const
{
	return program_;
}
