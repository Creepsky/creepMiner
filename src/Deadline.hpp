#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "Declarations.hpp"
#include <string>
#include <Poco/Mutex.h>
#include <atomic>

namespace Burst
{
	class Deadlines;
	class Account;
	class BlockData;

	class Deadline : public std::enable_shared_from_this<Deadline>
	{
	public:
		//Deadline(uint64_t nonce, uint64_t deadline);
		Deadline(uint64_t nonce, uint64_t deadline, std::shared_ptr<Account> account, uint64_t block, std::string plotFile,
			Deadlines* parent = nullptr);
		Deadline(Deadline&& rhs) = default;
		~Deadline();

		void onTheWay();
		void send();
		void confirm();

		std::string deadlineToReadableString() const;
		uint64_t getNonce() const;
		uint64_t getDeadline() const;
		AccountId getAccountId() const;
		std::string getAccountName() const;
		uint64_t getBlock() const;
		bool isOnTheWay() const;
		bool isSent() const;
		bool isConfirmed() const;
		const std::string& getPlotFile() const;
		void setDeadline(uint64_t deadline);

		bool operator<(const Deadline& rhs) const;
		bool operator()(const Deadline& lhs, const Deadline& rhs) const;

	private:
		std::shared_ptr<Account> account_;
		std::atomic<uint64_t> block_;
		std::atomic<uint64_t> nonce_;
		std::atomic<uint64_t> deadline_;
		std::string plotFile_ = "";
		std::atomic<bool> onTheWay_;
		std::atomic<bool> sent_;
		std::atomic<bool> confirmed_;
		Deadlines* parent_;
		mutable Poco::FastMutex mutex_;
	};

	class Deadlines
	{
	public:
		explicit Deadlines(BlockData* parent = nullptr);

		std::shared_ptr<Deadline> add(uint64_t nonce, uint64_t deadline, std::shared_ptr<Account> account, uint64_t block, std::string plotFile);
		void clear();
		bool confirm(Nonce nonce);
		bool confirm(Nonce nonce, AccountId accountId, uint64_t block);

		std::shared_ptr<Deadline> getBest() const;
		std::shared_ptr<Deadline> getBestConfirmed() const;
		std::shared_ptr<Deadline> getBestFound() const;
		std::shared_ptr<Deadline> getBestSent() const;

	private:
		void deadlineEvent(std::shared_ptr<Deadline> deadline, const std::string& type) const;
		void deadlineConfirmed(std::shared_ptr<Deadline> deadline) const;

	private:
		std::vector<std::shared_ptr<Deadline>> deadlines;
		BlockData* parent_;
		mutable Poco::FastMutex mutex_;

		friend class Deadline;
	};
}
