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
#ifdef USE_OPENCL
#include <CL/cl.h>
#else
using cl_context = int*;
using cl_command_queue = int*;
using cl_kernel = int*;
using cl_program = int*;
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
		bool create(size_t platformIdx = 0, size_t deviceIdx = 0);

		cl_context getContext() const;
		cl_command_queue getCommandQueue() const;
		cl_kernel getKernel() const;

		bool initialized() const;

		static MinerCL& getCL();

	private:
		cl_context context_ = nullptr;
		cl_command_queue command_queue_ = nullptr;
		cl_program program_ = nullptr;
		cl_kernel kernel_ = nullptr;
		bool initialized_ = false;
	};
}
