#pragma once

#include <cstdint>

namespace Burst
{
	enum Output : uint32_t
	{
		LastWinner,
		NonceFound,
		NonceOnTheWay,
		NonceSent,
		NonceConfirmed,
		PlotDone,
		DirDone
	};
}
