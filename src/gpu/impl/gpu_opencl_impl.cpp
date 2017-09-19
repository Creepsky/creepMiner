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

#include "gpu_opencl_impl.hpp"
#include <CL/cl.h>
#include "mining/MinerCL.hpp"
#include "gpu/gpu_shell.hpp"

cl_int Burst::Gpu_Opencl_Impl::lastError_ = 0;

bool Burst::Gpu_Opencl_Impl::allocateMemory(void** memory, MemoryType type, size_t size)
{
	cl_int ret;

	size = Gpu_Helper::calcMemorySize(type, size);

	if (size <= 0)
		return false;

	const auto allocated = clCreateBuffer(MinerCL::getCL().getContext(), CL_MEM_READ_WRITE, size, nullptr, &ret);

	if (ret == CL_SUCCESS)
	{
		*memory = allocated;
		return true;
	}

	lastError_ = ret;
	return false;
}

bool Burst::Gpu_Opencl_Impl::verify(const GensigData* gpuGensig, ScoopData* gpuScoops, Poco::UInt64* gpuDeadlines,
	size_t nonces, Poco::UInt64 nonceStart, Poco::UInt64 baseTarget)
{
	auto ret = clSetKernelArg(MinerCL::getCL().getKernel(), 0, sizeof(cl_mem), reinterpret_cast<const unsigned char*>(&gpuGensig));

	if (ret == CL_SUCCESS)
		ret = clSetKernelArg(MinerCL::getCL().getKernel(), 1, sizeof(cl_mem), reinterpret_cast<unsigned char*>(&gpuScoops));

	if (ret == CL_SUCCESS)
		ret = clSetKernelArg(MinerCL::getCL().getKernel(), 2, sizeof(cl_mem), reinterpret_cast<unsigned long*>(&gpuDeadlines));

	if (ret == CL_SUCCESS)
		ret = clSetKernelArg(MinerCL::getCL().getKernel(), 3, sizeof(cl_ulong), reinterpret_cast<const void*>(&nonces));
	
	if (ret == CL_SUCCESS)
		ret = clSetKernelArg(MinerCL::getCL().getKernel(), 4, sizeof(cl_ulong), reinterpret_cast<const void*>(&baseTarget));

	if (ret != CL_SUCCESS)
		return false;

	size_t globalWorkSize[3] = { nonces, 0, 0 };
	size_t localWorkSize[3] = { 512, 0, 0 };
	
	ret = clEnqueueNDRangeKernel(MinerCL::getCL().getCommandQueue(), MinerCL::getCL().getKernel(), 1, nullptr,
	                             globalWorkSize, localWorkSize, 0, nullptr, nullptr);

	if (ret != CL_SUCCESS)
		return false;

	return true;
}

bool Burst::Gpu_Opencl_Impl::getMinDeadline(Poco::UInt64* gpuDeadlines, size_t size, Poco::UInt64& minDeadline, Poco::UInt64& minDeadlineIndex)
{
	std::vector<Poco::UInt64> deadlines;
	deadlines.resize(size);

	if (!copyMemory(gpuDeadlines, deadlines.data(), MemoryType::Bytes, sizeof(Poco::UInt64) * size, MemoryCopyDirection::ToHost))
		return false;

	const auto minIter = std::min_element(deadlines.begin(), deadlines.end());

	minDeadline = *minIter;
	minDeadlineIndex = std::distance(deadlines.begin(), minIter);

	return true;
}

bool Burst::Gpu_Opencl_Impl::freeMemory(void* memory)
{
	const auto ret = clReleaseMemObject(static_cast<cl_mem>(memory));

	if (ret == CL_SUCCESS)
		return true;

	lastError_ = ret;
	return false;
}

bool Burst::Gpu_Opencl_Impl::getError(std::string& errorString)
{
	const auto getErrorString = [&]() {
		switch (lastError_)
		{
		case 0: return "CL_SUCCESS";
		case -1: return "CL_DEVICE_NOT_FOUND";
		case -2: return "CL_DEVICE_NOT_AVAILABLE";
		case -3: return "CL_COMPILER_NOT_AVAILABLE";
		case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
		case -5: return "CL_OUT_OF_RESOURCES";
		case -6: return "CL_OUT_OF_HOST_MEMORY";
		case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
		case -8: return "CL_MEM_COPY_OVERLAP";
		case -9: return "CL_IMAGE_FORMAT_MISMATCH";
		case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
		case -11: return "CL_BUILD_PROGRAM_FAILURE";
		case -12: return "CL_MAP_FAILURE";
		case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
		case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
		case -15: return "CL_COMPILE_PROGRAM_FAILURE";
		case -16: return "CL_LINKER_NOT_AVAILABLE";
		case -17: return "CL_LINK_PROGRAM_FAILURE";
		case -18: return "CL_DEVICE_PARTITION_FAILED";
		case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";
		case -30: return "CL_INVALID_VALUE";
		case -31: return "CL_INVALID_DEVICE_TYPE";
		case -32: return "CL_INVALID_PLATFORM";
		case -33: return "CL_INVALID_DEVICE";
		case -34: return "CL_INVALID_CONTEXT";
		case -35: return "CL_INVALID_QUEUE_PROPERTIES";
		case -36: return "CL_INVALID_COMMAND_QUEUE";
		case -37: return "CL_INVALID_HOST_PTR";
		case -38: return "CL_INVALID_MEM_OBJECT";
		case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
		case -40: return "CL_INVALID_IMAGE_SIZE";
		case -41: return "CL_INVALID_SAMPLER";
		case -42: return "CL_INVALID_BINARY";
		case -43: return "CL_INVALID_BUILD_OPTIONS";
		case -44: return "CL_INVALID_PROGRAM";
		case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
		case -46: return "CL_INVALID_KERNEL_NAME";
		case -47: return "CL_INVALID_KERNEL_DEFINITION";
		case -48: return "CL_INVALID_KERNEL";
		case -49: return "CL_INVALID_ARG_INDEX";
		case -50: return "CL_INVALID_ARG_VALUE";
		case -51: return "CL_INVALID_ARG_SIZE";
		case -52: return "CL_INVALID_KERNEL_ARGS";
		case -53: return "CL_INVALID_WORK_DIMENSION";
		case -54: return "CL_INVALID_WORK_GROUP_SIZE";
		case -55: return "CL_INVALID_WORK_ITEM_SIZE";
		case -56: return "CL_INVALID_GLOBAL_OFFSET";
		case -57: return "CL_INVALID_EVENT_WAIT_LIST";
		case -58: return "CL_INVALID_EVENT";
		case -59: return "CL_INVALID_OPERATION";
		case -60: return "CL_INVALID_GL_OBJECT";
		case -61: return "CL_INVALID_BUFFER_SIZE";
		case -62: return "CL_INVALID_MIP_LEVEL";
		case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
		case -64: return "CL_INVALID_PROPERTY";
		case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
		case -66: return "CL_INVALID_COMPILER_OPTIONS";
		case -67: return "CL_INVALID_LINKER_OPTIONS";
		case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";
		case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
		case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
		case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
		case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
		case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
		case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
		default: return "Unknown OpenCL error";
		}
	};

	if (lastError_ == CL_SUCCESS)
		return false;

	errorString = getErrorString();
	return true;
}

bool Burst::Gpu_Opencl_Impl::copyMemory(const void* input, void* output, MemoryType type, size_t size, MemoryCopyDirection direction)
{
	size = Gpu_Helper::calcMemorySize(type, size);

	if (direction == MemoryCopyDirection::ToDevice)
	{
		const auto ret = clEnqueueWriteBuffer(MinerCL::getCL().getCommandQueue(), cl_mem(output), CL_TRUE, 0, size, input, 0,
		                                      nullptr, nullptr);

		if (ret == CL_SUCCESS)
			return true;

		lastError_ = ret;
		return false;
	}

	if (direction == MemoryCopyDirection::ToHost)
	{
		const auto ret = clEnqueueReadBuffer(MinerCL::getCL().getCommandQueue(), cl_mem(input), CL_TRUE, 0, size, output, 0,
		                                     nullptr, nullptr);

		if (ret == CL_SUCCESS)
			return true;

		lastError_ = ret;
		return false;
	}

	return false;
}