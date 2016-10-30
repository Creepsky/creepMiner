#pragma once

#include <cstdint>
#include <set>

namespace Burst
{
	class Deadline
	{
	public:
		Deadline(uint64_t nonce, uint64_t deadline);

		void confirm();
		std::string deadlineToReadableString() const;
		uint64_t getNonce() const;
		uint64_t getDeadline() const;
		bool isConfirmed() const;

		bool operator< (const Deadline& rhs) const;
		bool operator()(const Deadline& lhs, const Deadline& rhs) const;

	private:
		uint64_t nonce = 0;
		uint64_t deadline = 0;
		bool confirmed = false;
	};

	class Deadlines
	{
	public:
		void add(Deadline&& deadline);
		void clear();

		Deadline* getBestDeadline();
		Deadline* getBestConfirmed();

	private:
		std::set<Deadline> deadlines;
	};
}
