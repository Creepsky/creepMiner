//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include "Declarations.hpp"
#include <Poco/Task.h>
#include <atomic>

namespace Burst
{
	class Miner;
	class PlotFile;
	class PlotReadProgress;

	class PlotReader : public Poco::Task
	{
	public:
		PlotReader(Miner& miner, const std::string& path);
		PlotReader(Miner& miner, std::unique_ptr<std::istream> stream, size_t accountId,
			size_t nonceStart, size_t nonceCount, size_t staggerSize);
		~PlotReader() override = default;

		void runTask() override;

	private:
		class PlotVerifier : public Task
		{
		public:
			PlotVerifier(Miner &miner, std::vector<ScoopData>&& buffer, uint64_t nonceRead, const GensigData& gensig,
				uint64_t nonceStart, const std::string& inputPath, uint64_t accountId);
			void runTask() override;

		private:
			Miner* miner_;
			std::vector<ScoopData> buffer_;
			uint64_t nonceRead_;
			const GensigData* gensig_;
			uint64_t nonceStart_;
			const std::string* inputPath_;
			uint64_t accountId_;
		};

		uint64_t nonceStart_ = 0;
		uint64_t scoopNum_ = 0;
		uint64_t nonceCount_ = 0;
		uint64_t nonceOffset_ = 0;
		uint64_t nonceRead_ = 0;
		uint64_t staggerSize_ = 0;
		uint64_t accountId_ = 0;
		GensigData gensig_;
		std::string inputPath_ = "";
		std::unique_ptr<std::istream> inputStream_;
		Miner& miner_;
	};

	class PlotListReader : public Poco::Task
	{
	public:
		PlotListReader(Miner& miner, std::shared_ptr<PlotReadProgress> progress,
			std::string&& dir, std::vector<std::shared_ptr<PlotFile>>&& plotFiles);
		~PlotListReader() override = default;

		void runTask() override;

	private:
		std::vector<std::shared_ptr<PlotFile>> plotFileList_;
		Miner* miner_;
		std::shared_ptr<PlotReadProgress> progress_;
		std::string dir_;
	};

	class PlotReadProgress
	{
	public:
		void reset();
		void add(uintmax_t value);
		void set(uintmax_t value);
		void setMax(uintmax_t value);
		bool isReady() const;
		uintmax_t getValue() const;
		float getProgress() const;

	private:
		uintmax_t progress_ = 0, max_ = 0;
		std::mutex lock_;
	};
}
