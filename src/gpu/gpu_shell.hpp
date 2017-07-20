#pragma once

#if MINING_CUDA
#include "impl/gpu_cuda_impl.hpp"
#elif MINING_OPENCL
#include "impl/gpu_opencl_impl.hpp"
#endif

namespace Burst
{
	/**
	 * \brief A shell class, that has simple functions that operates on a GPU.
	 * The functions are adapters that call a real implementation.
	 * \tparam TImpl The implementation class.
	 */
	template <typename TImpl>
	struct Gpu_Shell
	{
		/**
		 * \brief An alias for the implementation.
		 */
		using impl_t = TImpl;

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
		 * \brief Frees a list of memory blocks on the GPU.
		 * \tparam T The type of the tail.
		 * \tparam Args Variadic template types.
		 * \param arg The tail argument, that is freed.
		 * \param args The list of memory, that needs to be freed.
		 * \return true, when the list of memory was freed, false otherwise.
		 */
		//template <typename T, typename ...Args>
		//static bool freeMemory(T arg, Args&&... args)
		//{
		//	if (!freeMemory(arg))
		//		return false;

		//	return freeMemory(std::forward<Args&&>(args)...);
		//}

		/**
		 * \brief Searches for the best deadline in a memory block.
		 * \tparam Args Variadic template types.
		 * \param args The arguments that are needed the verify the deadlines.
		 * \return A pair<nonce, deadline> that is the best deadline found.
		 */
		template <typename ...Args>
		static bool verify(Args&&... args)
		{
			return TImpl::verify(std::forward<Args&&>(args)...);
		}

		/**
		 * \brief Runs an algorithm to search for the best deadline in a memory block.
		 * \tparam TAlgorithm The type of the algorithm that is used.
		 * \tparam Args Variadic template types.
		 * \param args The arguments that are needed for the search of deadlines.
		 * \return 
		 */
		template <typename TAlgorithm, typename ...Args>
		static bool run(Args&&... args)
		{
			return TAlgorithm::template run<Gpu_Shell>(std::forward<Args&&>(args)...);
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

#if MINING_CUDA
	using Gpu = Gpu_Shell<Gpu_Cuda_Impl>;
#elif MINING_OPENCL
	using Gpu = Gpu_Shell<Gpu_Opencl_Impl>;
#endif
}
