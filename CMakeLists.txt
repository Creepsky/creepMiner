cmake_minimum_required(VERSION 3.6)
project(burst-miner)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O3 -D_REENTRANT -march=native -DNDEBUG")

link_directories(/usr/local/lib/)

set(SOURCE_FILES
        src/nxt/nxt_address.cpp
        src/sphlib/sph_shabal.cpp
        src/Deadline.cpp
        src/Declarations.cpp
        src/main.cpp
        src/Miner.cpp
        src/MinerConfig.cpp
        src/MinerLogger.cpp
        src/MinerShabal.cpp
        src/MinerUtil.cpp
        src/NonceSubmitter.cpp
        src/PlotReader.cpp
        src/PlotSizes.cpp
        src/PlotVerifier.cpp
        src/Request.cpp
        src/Response.cpp
        src/Socket.cpp
        src/Url.cpp
        src/Account.cpp
        src/MinerServer.cpp
        src/Wallet.cpp
        src/MinerData.cpp
        src/RequestHandler.cpp)

add_executable(burst-miner ${SOURCE_FILES})

target_link_libraries (burst-miner
        pthread
        PocoFoundation
        PocoNet
        PocoNetSSL
        PocoUtil
        PocoCrypto
        PocoJSON)
