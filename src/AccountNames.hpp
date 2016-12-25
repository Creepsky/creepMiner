#pragma once

#include <unordered_map>
#include "Declarations.hpp"

namespace Burst
{
	class Wallet;

	class AccountNames
	{
	public:
		const std::string& getName(AccountId accountId, Wallet& wallet, bool reset = false);
		void reset();

	private:
		std::unordered_map<AccountId, std::string> names_;
	};
}
