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
		Deadlines,
		Bytes
	};

	enum class MemoryCopyDirection
	{
		ToHost,
		ToDevice
	};
}
