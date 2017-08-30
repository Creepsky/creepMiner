#pragma once

#include <Poco/Types.h>

namespace Burst
{
	struct CalculatedDeadline
	{
		Poco::UInt64 deadline;
		Poco::UInt64 nonce;
	};

	enum class MemoryType
	{
		Buffer,
		Gensig,
		Deadlines
	};

	enum class MemoryCopyDirection
	{
		ToHost,
		ToDevice
	};
}
