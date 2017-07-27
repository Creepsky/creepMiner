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

#include <Poco/Task.h>
#include <vector>
#include "Declarations.hpp"
#include <Poco/AutoPtr.h>
#include <Poco/Notification.h>
#include <Poco/NotificationQueue.h>
#include "shabal/MinerShabal.hpp"

namespace Burst
{
	class PlotReadProgress;
	class Miner;

	struct VerifyNotification : Poco::Notification
	{
		typedef Poco::AutoPtr<VerifyNotification> Ptr;

		std::vector<ScoopData> buffer;
		Poco::UInt64 accountId = 0;
		Poco::UInt64 nonceRead = 0;
		Poco::UInt64 nonceStart = 0;
		std::string inputPath = "";
		Poco::UInt64 block = 0;
		GensigData gensig;
		Poco::UInt64 baseTarget = 0;
	};

	class PlotVerifier : public Poco::Task
	{
	public:
		/**
		 * \brief 1. element: nonce, 2. element: deadline
		 */
		using DeadlineTuple = std::pair<Poco::UInt64, Poco::UInt64>;

		PlotVerifier(Miner &miner, Poco::NotificationQueue& queue, std::shared_ptr<PlotReadProgress> progress);
		void runTask() override;
		
		static std::array<DeadlineTuple, Shabal256::HashSize> verify(std::vector<ScoopData>& buffer, Poco::UInt64 nonceRead, Poco::UInt64 nonceStart, size_t offset,
		                       const GensigData& gensig, Poco::UInt64 baseTarget);

	private:
		Miner* miner_;
		Poco::NotificationQueue* queue_;
		std::shared_ptr<PlotReadProgress> progress_;
	};
}
