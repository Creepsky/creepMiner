#pragma once

#include <memory>
#include <Poco/Task.h>

namespace Burst
{
	class Miner;
	class Deadline;

	class NonceSubmitter : public Poco::Task
	{
	public:
		NonceSubmitter(Miner& miner, std::shared_ptr<Deadline> deadline);
		~NonceSubmitter() override = default;

		void runTask() override;

	private:
		Miner& miner;
		std::shared_ptr<Deadline> deadline;
	};
}
