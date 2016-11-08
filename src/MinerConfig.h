//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include <memory>
#include <vector>
#include <string>
#include "Url.hpp"

/*
 {
	poolUrl : "burst-pool.cryptoport.io:80",
	submissionMaxDelay : 60000,
	submissionMaxRetry : 3,
	plots : [
		"/mnt/sda/plots/",
		"/mnt/sdb/plots/"
	]
 }
 */

namespace Burst
{
	class PlotFile
	{
	public:
		PlotFile(std::string&& path, size_t size);

		const std::string& getPath() const;
		size_t getSize() const;

	private:
		std::string path;
		size_t size;
	};

	struct Output
	{
		bool progress = false;
		bool debug = false;
	};

	class MinerConfig
	{
	public:
		bool readConfigFile(const std::string& configPath);
		void rescan();

		size_t submissionMaxRetry = 5;
		Url urlPool;
		Url urlMiningInfo;
		size_t socketTimeout = 30;
		size_t maxBufferSizeMB = 64;
		Output output;
		const std::string& getPath() const;

		const std::vector<std::shared_ptr<PlotFile>>& getPlotFiles() const;
		uintmax_t getTotalPlotsize() const;

		static MinerConfig& getConfig();

	private:
		bool addPlotLocation(const std::string fileOrPath);
		std::shared_ptr<PlotFile> addPlotFile(const std::string& file);

		std::string configPath;
		std::vector<std::shared_ptr<PlotFile>> plotList;
	};
}
