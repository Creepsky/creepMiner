#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "Declarations.hpp"

namespace Burst
{
	class Deadline
	{
	public:
		Deadline(uint64_t nonce, uint64_t deadline);
		Deadline(uint64_t nonce, uint64_t deadline, AccountId accountId, uint64_t block);
		Deadline(Deadline&& rhs) = default;
		~Deadline();

		void confirm();
		std::string deadlineToReadableString() const;
		uint64_t getNonce() const;
		uint64_t getDeadline() const;
		AccountId getAccountId() const;
		uint64_t getBlock() const;
		bool isConfirmed() const;

		bool operator< (const Deadline& rhs) const;
		bool operator()(const Deadline& lhs, const Deadline& rhs) const;

	private:
		AccountId accountId = 0;
		uint64_t block = 0;
		uint64_t nonce = 0;
		uint64_t deadline = 0;
		bool confirmed = false;
	};

	class Deadlines
	{
	public:
		void add(Deadline&& deadline);
		void clear();
		bool confirm(Nonce nonce);
		bool confirm(Nonce nonce, AccountId accountId, uint64_t block);

		std::shared_ptr<Deadline> getBestDeadline();
		std::shared_ptr<Deadline> getBestConfirmed();

	private:
		std::vector<std::shared_ptr<Deadline>> deadlines;
	};
}
