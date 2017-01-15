#include "Account.hpp"
#include <mutex>
#include <Poco/Mutex.h>
#include "Wallet.hpp"
//#include "Wallet.hpp"
#include <thread>

Burst::Account::Account(): id_(0)
						 , wallet_(nullptr)
{}

Burst::Account::Account(Burst::Wallet& wallet, AccountId id, bool fetchAll)
	: id_{ id }, wallet_{ &wallet }
{}

void Burst::Account::setWallet(Wallet& wallet)
{
	wallet_ = &wallet;
}

Burst::AccountId Burst::Account::getId() const
{
	return id_;
}

template <typename T>
const T& getAsyncHelper(Poco::Nullable<T>& val, bool reset, std::function<bool(T&)> fetchFunction)
{
	static Poco::FastMutex mutex;
	Poco::ScopedLock<Poco::FastMutex> lock(mutex);

	// delete cached name if resetflag is set
	if (reset && !val.isNull())
		val.clear();

	// add name if there is no cached one
	if (val.isNull())
	{
		// because the fetching of the name is async
		// we insert an empty name in the map
		val = T{};

		// start thread to fetch name of account
		std::thread fetchThread([&val, fetchFunction]()
			{
				T fetchedVal;

				if (fetchFunction(fetchedVal))
				{
					Poco::ScopedLock<Poco::FastMutex> lock(mutex);
					val = fetchedVal;
				}
				else
				{
					Poco::ScopedLock<Poco::FastMutex> lock(mutex);
					val.clear();
				}
			});

		fetchThread.detach();
	}

	return val;
}

const std::string& Burst::Account::getName(bool reset)
{
	return getAsyncHelper<std::string>(name_, reset, [this](std::string& name)
		{
			return wallet_->getNameOfAccount(id_, name);
		});
}

const Burst::AccountId& Burst::Account::getRewardRecipient(bool reset)
{
	return getAsyncHelper<AccountId>(rewardRecipient_, reset, [this](AccountId& recipient)
		{
			return wallet_->getRewardRecipientOfAccount(id_, recipient);
		});
}

Burst::Deadlines& Burst::Account::getDeadlines()
{
	return deadlines_;
}

std::shared_ptr<Burst::Account> Burst::Accounts::getAccount(AccountId id, Wallet& wallet)
{
	auto iter = accounts_.find(id);

	if (iter == accounts_.end())
		accounts_.emplace(id, std::make_shared<Account>(wallet, id));
	else
		*iter;

	return accounts_[id];
}
