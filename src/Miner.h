//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#ifndef cryptoport_Miner_h
#define cryptoport_Miner_h

#include <array>
#include <vector>
#include <mutex>
#include <thread>
#include <ios>
#include <iomanip>
#include <string>
#include <iostream>
#include <fstream>
#include <istream>
#include <sstream>
#include <memory>
#include <map>
#include <cstring>
#include <limits>
#include <random>
#include <deque>
#include <condition_variable>
#include <atomic>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>

#include "rapidjson/document.h"

#ifndef WIN32
#   include <netdb.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include "unistd.h"
#	include <dirent.h>
#   ifndef MSG_NOSIGNAL
#       define MSG_NOSIGNAL SO_NOSIGPIPE
#   endif
#	define closesocket close
#	define SOCKET int
#	define PATH_SEPARATOR '/'
#else
#   define MSG_NOSIGNAL 0
#	include "win/dirent.h"
#	include <WinSock2.h>
#	include <ws2tcpip.h>
#	include <intrin.h>
#	define strcasecmp _stricmp 
#	define strncasecmp _strnicmp 
#	define SHUT_RDWR SD_BOTH
#	define SHUT_RD SD_RECEIVE
#	define SHUT_WR SD_SEND
#   define __builtin_bswap64 _byteswap_uint64
#	define PATH_SEPARATOR '\\'
#endif

#include "nxt/nxt_address.h"
#include "MinerConfig.h"

namespace Burst
{
    class MinerShabal;
    class Miner;
    class MinerLogger;
    class MinerConfig;
    class MinerProtocol;
    class PlotReader;
    
    template <size_t SZ>
    using BytesArray = std::array<uint8_t,SZ>;
    using ScoopData  = BytesArray<MinerConfig::scoopSize>;
    using GensigData = BytesArray<MinerConfig::hashSize>;
    using HashData   = BytesArray<MinerConfig::hashSize>;
}

#include "sphlib/sph_types.h"
#include "sphlib/sph_shabal.h"
#include "MinerShabal.h"
#include "MinerUtil.h"
#include "MinerLogger.h"
#include "MinerProtocol.h"
#include "PlotReader.h"

namespace Burst
{    
    class Miner
    {
    public:
        Miner(MinerConfig& config);
        void run();
        size_t getScoopNum() const;
        uint64_t getBaseTarget() const;
        const GensigData& getGensig() const;
        const MinerConfig* getConfig() const;
        void updateGensig(const std::string gensigStr, uint64_t blockHeight, uint64_t baseTarget);
        void submitNonce(uint64_t nonce, uint64_t accountId, uint64_t deadline);
        void nonceSubmitReport(uint64_t nonce, uint64_t accountId, uint64_t deadline);
    private:
        void nonceSubmitterThread();
        bool running;
        MinerConfig* config;
        size_t scoopNum;
        uint64_t baseTarget;
        uint64_t blockHeight;
        std::string gensigStr;
        MinerProtocol protocol;
        Shabal256 hash;
        GensigData gensig;
		std::vector<std::shared_ptr<PlotReader>> plotReaders;
        std::map<uint64_t,uint64_t> bestDeadline;
        std::map<uint64_t,uint64_t> bestNonce;
        std::map<uint64_t,uint64_t> bestDeadlineConfirmed;
        std::mutex accountLock;
        std::chrono::system_clock::time_point nextNonceSubmission;
    };
}

#endif
