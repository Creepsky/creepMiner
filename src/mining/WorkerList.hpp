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

#include <vector>
#include <Poco/Task.h>
#include <Poco/Thread.h>
#include <memory>
#if __cplusplus < 201402L
#include "MinerUtil.hpp"
#endif

namespace Burst
{
	template <typename T>
	class WorkerList
	{
	public:
		template <typename ...Args>
		WorkerList(Poco::Thread::Priority priority, size_t size, Args&&... args)
		{
			for (auto i = 0u; i < size; ++i)
				create(priority, std::forward<Args>(args)...);
		}

		void stop()
		{
			for (auto& worker : worker_)
			{
				worker.task->cancel();
				worker.thread->tryJoin(10 * 1000);
			}
		}

		size_t size() const
		{
			return worker_.size();
		}

		bool empty() const
		{
			return worker_.empty();
		}

	private:
		struct Worker
		{
			std::shared_ptr<Poco::Task> task;
			std::shared_ptr<Poco::Thread> thread;
		};

		template <typename ...Args>
		bool create(Poco::Thread::Priority priority, Args&&... args)
		{
			try
			{
				Worker worker;
				worker.task = std::make_unique<T>(std::forward<Args>(args)...);
				worker.thread = std::make_unique<Poco::Thread>();
				worker.thread->setPriority(priority);
				worker.thread->start(*worker.task);

				worker_.emplace_back(worker);
				return true;
			}
			catch (...)
			{
				return false;
			}

		}

		std::vector<Worker> worker_;
	};
}
