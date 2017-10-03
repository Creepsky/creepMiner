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

/*
 * OpenCL relevant classes and functions.
 */
#include <vector>

#ifdef USE_OPENCL
#include <CL/cl.h>
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
	/**
	 * \brief A OpenCL context.
	 */
	class MinerCL
	{
	public:
		~MinerCL();
		bool create(unsigned platformIdx = 0, unsigned deviceIdx = 0);
		cl_command_queue createCommandQueue();

		cl_context getContext() const;
		cl_kernel getKernel() const;
		size_t getKernelWorkGroupSize() const;

		bool initialized() const;

		static MinerCL& getCL();

	private:
		cl_context context_ = nullptr;
		cl_program program_ = nullptr;
		std::vector<cl_command_queue> command_queues_;
		cl_kernel kernel_ = nullptr;
		bool initialized_ = false;
		std::vector<cl_platform_id> platforms_;
		std::vector<cl_device_id> devices_;
		unsigned platformIdx_ = 0, deviceIdx_ = 0;
		size_t kernelWorkGroupSize_;
	};
}
