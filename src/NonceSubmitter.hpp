#pragma once

#include <memory>
#include "Response.hpp"

namespace Burst
{
	class Miner;
	class Deadline;

	class NonceSubmitter
	{
	public:
		NonceSubmitter(Miner& miner, std::shared_ptr<Deadline> deadline);

		void startSubmit();

	private:
		void submitThread(Miner* miner, std::shared_ptr<Deadline> deadline) const;

	private:
		Miner* miner_;
		std::shared_ptr<Deadline> deadline_;
	};
}
