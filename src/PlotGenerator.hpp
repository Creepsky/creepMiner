#pragma once

#include <cstdint>
#include <Poco/Task.h>

namespace Burst
{
	class Miner;

	class PlotGenerator : public Poco::Runnable
	{
	public:
		PlotGenerator(Poco::UInt64 account, Poco::UInt64 staggerSize, Poco::UInt64 startNonce, Poco::UInt64 nonces, void* output = nullptr);
		~PlotGenerator() override = default;

		void run() override;
		void* getOutput() const;

		static void* generate(Poco::UInt64 account, Poco::UInt64 staggerSize, Poco::UInt64 startNonce, Poco::UInt64 cachePos, void* output);

	private:
		Poco::UInt64 account_;
		Poco::UInt64 staggerSize_;
		Poco::UInt64 startNonce_;
		Poco::UInt64 nonces_;
		void* output_;
	};

	class RandomNonceGenerator : public Poco::Task
	{
	public:
		RandomNonceGenerator(Miner& miner, Poco::UInt64 account, Poco::UInt64 staggerSize, Poco::UInt64 randomNonces);
		~RandomNonceGenerator() override = default;

		void runTask() override;

	private:
		Miner* miner_;
		Poco::UInt64 account_;
		Poco::UInt64 staggerSize_;
		Poco::UInt64 randomNonces_;
	};
}
