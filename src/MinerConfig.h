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

    class MinerConfig
    {
    public:
        bool readConfigFile(const std::string& configPath);
        void rescan();
        
        size_t submissionMaxDelay = 60;
        size_t submissionMaxRetry = 5;
        std::string poolHost = "burst-pool.cryptoport.io";
        size_t poolPort = 80;
        size_t socketTimeout = 30;
        size_t maxBufferSizeMB = 64;
        std::string configPath;
        std::vector<std::shared_ptr<PlotFile>> plotList;
        
        static const size_t hashSize = 32;
        static const size_t scoopPerPlot = 4096;
        static const size_t hashPerScoop = 2;
        static const size_t scoopSize = hashPerScoop * hashSize; // 64 Bytes
        static const size_t plotSize = scoopPerPlot * scoopSize; // 256KB = 262144 Bytes
        static const size_t plotScoopSize = scoopSize + hashSize; // 64 + 32 bytes

    private:
        bool addPlotLocation(const std::string fileOrPath);
        std::shared_ptr<PlotFile> addPlotFile(const std::string& file);
    };
}
