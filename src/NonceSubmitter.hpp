#pragma once

#include <memory>
#include <Poco/Task.h>
#include "Response.hpp"

namespace Burst
{
	class Miner;
	class Deadline;

	class NonceSubmitter : public Poco::Task
	{
	public:
		NonceSubmitter(Miner& miner, std::shared_ptr<Deadline> deadline);
		~NonceSubmitter() override = default;

		Poco::ActiveMethod<NonceConfirmation, void, NonceSubmitter> submitAsync;
		NonceConfirmation submit();

		void runTask() override;

	private:
		Miner& miner;
		std::shared_ptr<Deadline> deadline;
	};
}
