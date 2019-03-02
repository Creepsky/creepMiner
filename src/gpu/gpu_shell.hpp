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

#include "impl/gpu_cuda_impl.hpp"
#include "impl/gpu_opencl_impl.hpp"

namespace Burst
{
	/**
	 * \brief A shell class, that has simple functions that operates on a GPU.
	 * The functions are adapters that call a real implementation.
	 * \tparam TImpl The implementation class.
	 */
	template <typename TImpl>
	struct GpuShell
	{
		/**
		 * \brief An alias for the implementation.
		 */
		using ImplT = TImpl;

		/**
		* \brief Initializes a new stream (queue).
		* \tparam Args Variadic template types.
		* \param args The arguments to create the stream.
		* \return true, when the stream was created, false otherwise.
		*/
		template <typename ...Args>
		static bool initStream(Args&&... args)
		{
			return TImpl::initStream(std::forward<Args&&>(args)...);
		}

		/**
		 * \brief Allocates memory on the GPU.
		 * \tparam Args Variadic template types.
		 * \param args The arguments to create the memory.
		 * \return true, when the memory was allocated, false otherwise.
		 */
		template <typename ...Args>
		static bool allocateMemory(Args&&... args)
		{
			return TImpl::allocateMemory(std::forward<Args&&>(args)...);
		}

		/**
		 * \brief Copies a memory block on the GUP to another memory block on the GPU.
		 * \tparam Args Variadic template types.
		 * \param args The arguments to copy the memory.
		 * \return true, when the memory was copied, false otherwise.
		 */
		template <typename ...Args>
		static bool copyMemory(Args&&... args)
		{
			return TImpl::copyMemory(std::forward<Args&&>(args)...);
		}

		/**
		 * \brief Frees a memory block on the GPU.
		 * \tparam Args Variadic template types.
		 * \param args The arguments to free the memory.
		 * \return true, when the memory was freed, false otherwise.
		 */
		template <typename ...Args>
		static bool freeMemory(Args&&... args)
		{
			return TImpl::freeMemory(std::forward<Args&&>(args)...);
		}

		/**
		 * \brief Searches for the best deadline in a memory block.
		 * \tparam Args Variadic template types.
		 * \param args The arguments that are needed to verify the deadlines.
		 * \return true, when there was an error, false otherwise.
		 */
		template <typename ...Args>
		static bool verify(Args&&... args)
		{
			return TImpl::verify(std::forward<Args&&>(args)...);
		}

		/**
		 * \brief Searches for the best deadline in an array of deadlines.
		 * \tparam Args Variadic template types. 
		 * \param args The arguments that are needed to search for the best deadline.
		 * \return true, when there was no error, false otherwise.
		 */
		template <typename ...Args>
		static bool getMinDeadline(Args&&... args)
		{
			return TImpl::getMinDeadline(std::forward<Args&&>(args)...);
		}

		/**
		 * \brief Runs an algorithm to search for the best deadline in a memory block.
		 * \tparam TAlgorithm The type of the algorithm that is used.
		 * \tparam Args Variadic template types.
		 * \param args The arguments that are needed for the search of deadlines.
		 * \return true, when there was no error, false otherwise.
		 */
		template <typename TAlgorithm, typename ...Args>
		static bool run(Args&&... args)
		{
			return TAlgorithm::template run<GpuShell>(std::forward<Args&&>(args)...);
		}

		/**
		 * \brief Gets the last error, if any occured.
		 * \tparam Args Variadic template types.
		 * \param args The arguments that are needed the get the last error.
		 * \return true, when there was an error, false otherwise.
		 */
		template <typename ...Args>
		static bool getError(Args&&... args)
		{
			return TImpl::getError(std::forward<Args&&>(args)...);
		}
	};

	struct GpuHelper
	{
		/**
		 * \brief Calculates the byte size a structure type.
		 * \param memType The type of the structure.
		 * \param size The amount of structures.
		 * \return The size of all structures in bytes.
		 */
		static Poco::UInt64 calcMemorySize(const MemoryType memType, Poco::UInt64 size)
		{
			if (memType == MemoryType::Buffer)
				size *= sizeof(Poco::UInt8) * Settings::scoopSize;
			else if (memType == MemoryType::Gensig)
				size = sizeof(Poco::UInt8) * Settings::hashSize;
			else if (memType == MemoryType::Deadlines)
				size *= sizeof(CalculatedDeadline);

			return size;
		}

		/**
		 * \brief Calculates the bytes for a type.
		 * \tparam T The type.
		 * \param size The amount of types.
		 * \return The size of all types in bytes.
		 */
		template <typename T>
		static Poco::UInt64 calcMemorySize(const Poco::UInt64 size)
		{
			return size * sizeof(T);
		}
	};

	using GpuCuda = GpuShell<GpuCudaImpl>;
	using GpuOpenCl = GpuShell<GpuOpenclImpl>;
}
