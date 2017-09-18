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
		bool create();

		cl_context& getContext();
		const cl_context& getContext() const;

		static MinerCL& getCL();

	private:
		cl_context context_ = nullptr;
	};
}
