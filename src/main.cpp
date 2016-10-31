//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "Miner.h"
#include "MinerLogger.h"
#include "MinerConfig.h"
#include "MinerUtil.h"

int main(int argc, const char* argv[])
{
    Burst::MinerLogger::write("Burst miner " + Burst::versionToString());
    Burst::MinerLogger::write("----------------------------------------------");
    Burst::MinerLogger::write("Github:   https://github.com/Creepsky/burst-miner");
    Burst::MinerLogger::write("Author:   Creepsky [creepsky@gmail.com]");
	Burst::MinerLogger::write("Burst :   BURST-JBKL-ZUAV-UXMB-2G795");
    Burst::MinerLogger::write("----------------------------------------------");
    Burst::MinerLogger::write("Based on http://github.com/uraymeiviar/burst-miner");
    Burst::MinerLogger::write("author : uray meiviar [ uraymeiviar@gmail.com ]");
    Burst::MinerLogger::write("please donate to support developments :");
    Burst::MinerLogger::write(" [ Burst   ] BURST-8E8K-WQ2F-ZDZ5-FQWHX");
    Burst::MinerLogger::write(" [ Bitcoin ] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b");
    Burst::MinerLogger::write("----------------------------------------------");

#ifdef WIN32
	WSADATA   wsadata;
	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
	{
		Burst::MinerLogger::write("failed to initalize networking system", Burst::TextType::Error);
	}
#endif
    
    std::string configFile = "mining.conf";

    if(argc > 1)
    {
        if(argv[1][0] == '-')
        {
            Burst::MinerLogger::write("usage : burstminer <config-file>");
            Burst::MinerLogger::write("if no config-file specified, program will look for mining.conf file inside current directory");
        }
        configFile = std::string(argv[1]);
    }

    Burst::MinerLogger::write("using config file : " + configFile);
    
    Burst::MinerConfig config;

    if( config.readConfigFile(configFile) )
    {
		Burst::Miner miner(config);
        miner.run();
    }
    else
    {
        Burst::MinerLogger::write("Aborting program due to invalid configuration", Burst::TextType::Error);
    }
    
#ifdef WIN32
	WSACleanup();
#endif
    return 0;
}

