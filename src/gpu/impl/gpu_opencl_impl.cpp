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

#include "gpu_opencl_impl.hpp"
#include "mining/MinerCL.hpp"
#include "gpu/gpu_shell.hpp"
#include <random>
#include "logging/Message.hpp"
#include "logging/MinerLogger.hpp"

int Burst::GpuOpenclImpl::lastError_ = 0;

bool Burst::GpuOpenclImpl::initStream(void** stream)
{
#ifdef USE_OPENCL
	const auto queue = MinerCl::getCL().createCommandQueue();

	if (queue == nullptr)
		return false;

	*stream = queue;
#endif
	return true;
}

bool Burst::GpuOpenclImpl::allocateMemory(void** memory, MemoryType type, size_t size)
{
#ifdef USE_OPENCL
	cl_int ret;

	size = GpuHelper::calcMemorySize(type, size);

	if (size <= 0)
		return false;

	const auto allocated = clCreateBuffer(MinerCl::getCL().getContext(), CL_MEM_READ_WRITE, size, nullptr, &ret);

	if (ret == CL_SUCCESS)
	{
		*memory = allocated;
		return true;
	}

	lastError_ = ret;
	return false;
#else
	return true;
#endif
}

namespace Burst
{
	struct Gpu_Opencl_Impl_Helper
	{
		static size_t calcOccupancy(const size_t elements, const size_t local_optimal, const size_t local_preferred_multiply)
		{
			if (local_preferred_multiply == 0 || local_optimal == 0)
				return 1;

			auto local = local_optimal;

			for (auto i = local / local_preferred_multiply; i > 0 && local == local_optimal; i--)
				if (fmod(elements, i * local_preferred_multiply) == 0.f)
					local = i * local_preferred_multiply;

			if (local == local_optimal)
				local = 1;

			return local;
		}
	};
}

bool Burst::GpuOpenclImpl::verify(const GensigData* gpuGensig, ScoopData* gpuScoops, Poco::UInt64* gpuDeadlines,
	size_t nonces, Poco::UInt64 nonceStart, Poco::UInt64 baseTarget, void* stream)
{
#ifdef USE_OPENCL
	auto ret = clSetKernelArg(MinerCl::getCL().getKernel_Calculate(), 0, sizeof(cl_mem), &gpuGensig);

	if (ret == CL_SUCCESS)
		ret = clSetKernelArg(MinerCl::getCL().getKernel_Calculate(), 1, sizeof(cl_mem), &gpuScoops);

	if (ret == CL_SUCCESS)
		ret = clSetKernelArg(MinerCl::getCL().getKernel_Calculate(), 2, sizeof(cl_mem), &gpuDeadlines);

	if (ret == CL_SUCCESS)
		ret = clSetKernelArg(MinerCl::getCL().getKernel_Calculate(), 3, sizeof(cl_ulong), reinterpret_cast<const void*>(&nonces));
	
	if (ret == CL_SUCCESS)
		ret = clSetKernelArg(MinerCl::getCL().getKernel_Calculate(), 4, sizeof(cl_ulong), reinterpret_cast<const void*>(&baseTarget));

	if (ret != CL_SUCCESS)
		return false;

	auto local = Gpu_Opencl_Impl_Helper::calcOccupancy(nonces, MinerCl::getCL().getKernelCalculateWorkGroupSize(),
	                                                   MinerCl::getCL().getKernelCalculateWorkGroupSize(true));

	ret = clEnqueueNDRangeKernel(static_cast<cl_command_queue>(stream), MinerCl::getCL().getKernel_Calculate(), 1, nullptr,
	                             &nonces, &local, 0, nullptr, nullptr);

	if (ret != CL_SUCCESS)
	{
		lastError_ = ret;
		return false;
	}
#endif
	return true;
}

bool Burst::GpuOpenclImpl::getMinDeadline(Poco::UInt64* gpuDeadlines, size_t size, Poco::UInt64& minDeadline, Poco::UInt64& minDeadlineIndex, void* stream)
{
#ifdef USE_OPENCL
	auto errorCode = CL_SUCCESS;
	Poco::UInt64* gpuBestMemory;

	//auto global = size;
	//const auto preferred_local_size = MinerCL::getCL().getKernelFindBestWorkGroupSize(true);

	//if (global % preferred_local_size != 0)
	//	global = (global / preferred_local_size + 1) * preferred_local_size;

	//auto local = Gpu_Opencl_Impl_Helper::calcOccupancy(size, MinerCL::getCL().getKernelFindBestWorkGroupSize(),
	//                                                   MinerCL::getCL().getKernelFindBestWorkGroupSize(true));

	//auto local = MinerCL::getCL().getComputeUnits() * MinerCL::getCL().getKernelFindBestWorkGroupSize(true);

	//local = MinerCL::getCL().getKernelFindBestWorkGroupSize();
	//global = local * 4;

	auto local = MinerCl::getCL().getKernelFindBestWorkGroupSize();
	auto global = local;

	const auto reduction_groups = global / local;
	const auto reduction_group_size = 2;
	const auto reduction_group_bytes = reduction_group_size * sizeof(Poco::UInt64);
	const auto reduction_groups_size = reduction_groups * reduction_group_size;
	const auto reduction_groups_bytes = reduction_groups * reduction_group_bytes;

	if (!allocateMemory(reinterpret_cast<void**>(&gpuBestMemory), MemoryType::Bytes, reduction_groups_bytes))
		return false;

	auto ret = true;
	ret = ret && (errorCode = clSetKernelArg(MinerCl::getCL().getKernel_GetMin(), 0, sizeof(cl_mem), &gpuDeadlines)) == CL_SUCCESS;
	ret = ret && (errorCode = clSetKernelArg(MinerCl::getCL().getKernel_GetMin(), 1, sizeof(cl_uint), &size)) == CL_SUCCESS;
	ret = ret && (errorCode = clSetKernelArg(MinerCl::getCL().getKernel_GetMin(), 2, sizeof(cl_uint) * local, nullptr)) == CL_SUCCESS;
	ret = ret && (errorCode = clSetKernelArg(MinerCl::getCL().getKernel_GetMin(), 3, sizeof(cl_ulong) * local, nullptr)) == CL_SUCCESS;
	ret = ret && (errorCode = clSetKernelArg(MinerCl::getCL().getKernel_GetMin(), 4, sizeof(cl_mem), &gpuBestMemory)) == CL_SUCCESS;

	ret = ret && (errorCode = clEnqueueNDRangeKernel(static_cast<cl_command_queue>(stream),
	                                                 MinerCl::getCL().getKernel_GetMin(), 1, nullptr,
	                                                 &global, &local, 0, nullptr, nullptr)) == CL_SUCCESS;

	ret = ret && (errorCode = clEnqueueReadBuffer(static_cast<cl_command_queue>(stream), cl_mem(gpuBestMemory), CL_TRUE,
	                                              0, sizeof(Poco::UInt64), &minDeadlineIndex, 0,
	                                              nullptr, nullptr)) == CL_SUCCESS;

	ret = ret && (errorCode = clEnqueueReadBuffer(static_cast<cl_command_queue>(stream), cl_mem(gpuBestMemory), CL_TRUE,
	                                              sizeof(Poco::UInt64), sizeof(Poco::UInt64), &minDeadline, 0,
	                                              nullptr, nullptr)) == CL_SUCCESS;
	
	std::vector<Poco::UInt64> bestMemory(reduction_groups_size);

	// only needed when global == local * N; N > 1
	/*copyMemory(gpuBestMemory, bestMemory.data(), MemoryType::Bytes, reduction_groups_bytes, MemoryCopyDirection::ToHost, stream);
	
	for (auto i = 0; i < reduction_groups_size; ++i)
	{
		if (i % 2 == 0)
		{
			if (minDeadline > bestMemory[i] || i == 0)
			{
				minDeadlineIndex = bestMemory[i + 0];
				minDeadline = bestMemory[i + 1];
			}
		}
	}*/

	if (!ret)
		lastError_ = errorCode;

	freeMemory(gpuBestMemory);

	return ret;
#else
	return true;
#endif
}

bool Burst::GpuOpenclImpl::freeMemory(void* memory)
{
#ifdef USE_OPENCL
	const auto ret = clReleaseMemObject(static_cast<cl_mem>(memory));

	if (ret == CL_SUCCESS)
		return true;

	lastError_ = ret;
	return false;
#else
	return true;
#endif
}

bool Burst::GpuOpenclImpl::getError(std::string& errorString)
{
#ifdef USE_OPENCL
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
#else
	return false;
#endif
}

bool Burst::GpuOpenclImpl::copyMemory(const void* input, void* output, MemoryType type, size_t size, MemoryCopyDirection direction, void* stream)
{
#ifdef USE_OPENCL
	size = GpuHelper::calcMemorySize(type, size);

	if (direction == MemoryCopyDirection::ToDevice)
	{
		const auto ret = clEnqueueWriteBuffer(static_cast<cl_command_queue>(stream), cl_mem(output), CL_TRUE, 0, size, input, 0,
		                                      nullptr, nullptr);

		if (ret == CL_SUCCESS)
			return true;

		lastError_ = ret;
		return false;
	}

	if (direction == MemoryCopyDirection::ToHost)
	{
		const auto ret = clEnqueueReadBuffer(static_cast<cl_command_queue>(stream), cl_mem(input), CL_TRUE, 0, size, output, 0,
		                                     nullptr, nullptr);

		if (ret == CL_SUCCESS)
			return true;

		lastError_ = ret;
		return false;
	}

	return false;
#else
	return true;
#endif
}