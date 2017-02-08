#pragma once

#include <vector>
#include <Poco/Task.h>
#include <Poco/Thread.h>
#include <memory>

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
