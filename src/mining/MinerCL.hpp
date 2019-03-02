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

#pragma once

/*
 * OpenCL relevant classes and functions.
 */
#include <vector>
#include <cstdio>
#include <string>

#ifdef USE_OPENCL
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#else
using cl_context = int*;
using cl_command_queue = int*;
using cl_kernel = int*;
using cl_program = int*;
using cl_platform_id = int*;
using cl_device_id = int*;
#endif

namespace Burst
{
	struct ClDevice
	{
		cl_device_id id;
		std::string name;
	};

	struct ClPlatform
	{
		cl_platform_id id;
		std::string name;
		std::string version;
		std::vector<ClDevice> devices;
		std::vector<cl_device_id> getDeviceIds() const;

		static bool getPlatforms(std::vector<ClPlatform>& platforms, std::string& error);
	};

	/**
	 * \brief A OpenCL context.
	 */
	class MinerCl
	{
	public:
		MinerCl();
		~MinerCl();

		bool create(unsigned platformIdx = 0, unsigned deviceIdx = 0);
		cl_command_queue createCommandQueue();

		cl_context getContext() const;
		cl_program getProgram() const;
		cl_kernel getKernel_Calculate() const;
		cl_kernel getKernel_GetMin() const;
		size_t getKernelCalculateWorkGroupSize(bool preferred = false) const;
		size_t getKernelFindBestWorkGroupSize(bool preferred = false) const;
		size_t getComputeUnits() const;
		const ClPlatform* getPlatform() const;
		const cl_device_id* getDeviceId() const;
		const ClDevice* getDevice() const;
		const std::vector<ClPlatform>& getPlatforms() const;

		bool initialized() const;

		static MinerCl& getCL();

	private:
		cl_context context_ = nullptr;
		cl_program program_ = nullptr;
		std::vector<cl_command_queue> commandQueues_;
		cl_kernel kernelCalculateDeadlines_ = nullptr,
			kernelBestDeadline_ = nullptr;
		bool initialized_ = false;
		std::vector<ClPlatform> platforms_;
		unsigned platformIdx_ = 0, deviceIdx_ = 0;
		size_t kernelCalculateWorkGroupSize_ = 0;
		size_t kernelFindBestWorkGroupSize_ = 0;
		size_t kernelCalculatePrefferedWorkGroupSize_ = 0;
		size_t kernelFindBestPrefferedWorkGroupSize_ = 0;
		size_t computeUnits_ = 0;
	};
}
