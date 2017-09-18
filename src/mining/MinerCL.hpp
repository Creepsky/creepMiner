#pragma once

/*
 * OpenCL relevant classes and functions.
 */
#include <CL/cl.h>
#include <vector>

namespace Burst
{
	/**
	 * \brief A OpenCL context.
	 */
	class MinerCL
	{
	public:
		bool create(size_t platformIdx = 0, size_t deviceIdx = 0);

		cl_context getContext() const;
		cl_command_queue getCommandQueue() const;

		static MinerCL& getCL();

	private:
		cl_context context_ = nullptr;
		cl_command_queue command_queue_ = nullptr;
	};
}
