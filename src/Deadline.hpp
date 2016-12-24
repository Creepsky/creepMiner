#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "Declarations.hpp"
#include <string>

namespace Burst
{
	class Deadline
	{
	public:
		//Deadline(uint64_t nonce, uint64_t deadline);
		Deadline(uint64_t nonce, uint64_t deadline, AccountId accountId, uint64_t block, std::string plotFile);
		Deadline(Deadline&& rhs) = default;
		~Deadline();

		void send();
		void confirm();
		std::string deadlineToReadableString() const;
		uint64_t getNonce() const;
		uint64_t getDeadline() const;
		AccountId getAccountId() const;
		uint64_t getBlock() const;
		bool isOnTheWay() const;
		bool isConfirmed() const;
		const std::string& getPlotFile() const;

		bool operator<(const Deadline& rhs) const;
		bool operator()(const Deadline& lhs, const Deadline& rhs) const;

	private:
		AccountId accountId_ = 0;
		uint64_t block_ = 0;
		uint64_t nonce_ = 0;
		uint64_t deadline_ = 0;
		std::string plotFile_ = "";
		bool sent_ = false;
		bool confirmed_ = false;
	};

	class Deadlines
	{
	public:
		std::shared_ptr<Deadline> add(Deadline&& deadline);
		void clear();
		bool confirm(Nonce nonce);
		bool confirm(Nonce nonce, AccountId accountId, uint64_t block);

		std::shared_ptr<Deadline> getBest() const;
		std::shared_ptr<Deadline> getBestConfirmed() const;
		std::shared_ptr<Deadline> getBestSent() const;

	private:
		std::vector<std::shared_ptr<Deadline>> deadlines;
	};
}
