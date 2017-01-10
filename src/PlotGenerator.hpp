#pragma once

#include <cstdint>
#include <memory>
#include <Poco/Task.h>

namespace Burst
{
	class Miner;

	class PlotGenerator : public Poco::Runnable
	{
	public:
		PlotGenerator(uint64_t account, uint64_t staggerSize, uint64_t startNonce, uint64_t nonces, void* output = nullptr);
		~PlotGenerator() override = default;

		void run() override;
		void* getOutput() const;

		static void* generate(uint64_t account, uint64_t staggerSize, uint64_t startNonce, uint64_t cachePos, void* output);

	private:
		uint64_t account_;
		uint64_t staggerSize_;
		uint64_t startNonce_;
		uint64_t nonces_;
		void* output_;
	};

	class RandomNonceGenerator : public Poco::Task
	{
	public:
		RandomNonceGenerator(Miner& miner, uint64_t account, uint64_t staggerSize, uint64_t randomNonces);
		~RandomNonceGenerator() override = default;

		void runTask() override;

	private:
		Miner* miner_;
		uint64_t account_;
		uint64_t staggerSize_;
		uint64_t randomNonces_;
	};
}
