cmake_minimum_required(VERSION 3.5)
project(burst-miner)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")

link_directories(/usr/local/lib/)

set(SOURCE_FILES
    src/nxt/nxt_address.cpp
    src/nxt/nxt_address.h
    src/rapidjson/internal/pow10.h
    src/rapidjson/internal/stack.h
    src/rapidjson/internal/strfunc.h
    src/rapidjson/document.h
    src/rapidjson/filestream.h
    src/rapidjson/prettywriter.h
    src/rapidjson/rapidjson.h
    src/rapidjson/reader.h
    src/rapidjson/stringbuffer.h
    src/rapidjson/writer.h
    src/sphlib/sph_shabal.cpp
    src/sphlib/sph_shabal.h
    src/sphlib/sph_types.h
    src/win/dirent.h
    src/Deadline.cpp
    src/Deadline.hpp
    src/Declarations.hpp
    src/main.cpp
    src/Miner.cpp
    src/Miner.hpp
    src/MinerConfig.cpp
    src/MinerConfig.hpp
    src/MinerLogger.cpp
    src/MinerLogger.hpp
    src/MinerShabal.cpp
    src/MinerShabal.hpp
    src/MinerUtil.cpp
    src/MinerUtil.hpp
    src/NonceSubmitter.cpp
    src/NonceSubmitter.hpp
    src/PlotReader.cpp
    src/PlotReader.hpp
    src/PoolSockets.cpp
    src/PoolSockets.hpp
    src/Request.cpp
    src/Request.hpp
    src/Response.cpp
    src/Response.hpp
    src/Socket.cpp
    src/Socket.hpp
    src/SocketDefinitions.hpp
    src/Url.cpp
    src/Url.hpp)

add_executable(burst-miner ${SOURCE_FILES})

target_link_libraries (burst-miner
        pthread
        PocoFoundation
        PocoNet
        PocoNetSSL
        PocoUtil
        PocoCrypto)
