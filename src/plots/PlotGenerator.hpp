// ==========================================================================
// 
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2017 Creepsky (creepsky@gmail.com)
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301  USA
// 
// ==========================================================================

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
