##################################################################
# Project and CMake settings
##################################################################
cmake_minimum_required(VERSION 3.2.2)
project(creepMiner VERSION 1.6.0)
set(CMAKE_CXX_STANDARD 14)

##################################################################
# Configuration types
##################################################################
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Release_AVX;Release_AVX2" CACHE STRING "" FORCE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

##################################################################
# Environment variables
##################################################################
set(POCO_ROOT "$ENV{POCO_ROOT}" CACHE PATH "POCO root folder")
set(OPENSSL_ROOT "$ENV{OPENSSL_ROOT}" CACHE PATH "OpenSSL root folder")

##################################################################
# LOG_TERMINAL / LOG_SERVICE
##################################################################
set(LOG_OUTPUT "Terminal" CACHE STRING "Logging output location")
set_property(CACHE LOG_OUTPUT PROPERTY STRINGS Terminal Service) 

if (LOG_OUTPUT STREQUAL "Service")
	set(LOG_OUTPUT_FLAG "-DLOG_SERVICE")
else ()
	set(LOG_OUTPUT_FLAG "-DLOG_TERMINAL")
endif ()

##################################################################
# GPU acceleration
##################################################################
SET(GPU_ACCELERATION "None" CACHE STRING "GPU acceleration")
SET_PROPERTY(CACHE GPU_ACCELERATION PROPERTY STRINGS None CUDA) 

if (GPU_ACCELERATION STREQUAL "CUDA")
	find_package(CUDA)
endif ()

##################################################################
# UNIX / BSD specific
##################################################################
if (UNIX OR APPLE)
	# setup the different configurations
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -march=native ")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O3 -march=native")
	set(CMAKE_CXX_FLAGS_RELEASE_AVX "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O3 -march=native -mavx -DUSE_AVX")
	set(CMAKE_CXX_FLAGS_RELEASE_AVX2 "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O3 -march=native -mavx2 -DUSE_AVX2")

	add_definitions(
		${LOG_OUTPUT_FLAG}
		-D_REENTRANT)

	link_directories(/usr/local/lib)
	include_directories(
		/usr/local/include
		${CMAKE_CURRENT_SOURCE_DIR}/src)
##################################################################
# MSVC specific
##################################################################
elseif (MSVC)
	# setup the different configurations
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${LOG_OUTPUT_FLAG} -DPOCO_STATIC")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${LOG_OUTPUT_FLAG} -DPOCO_STATIC")
	set(CMAKE_CXX_FLAGS_RELEASE_AVX "${CMAKE_CXX_FLAGS_RELEASE} ${LOG_OUTPUT_FLAG} /arch:AVX -DPOCO_STATIC -DUSE_AVX")
	set(CMAKE_CXX_FLAGS_RELEASE_AVX2 "${CMAKE_CXX_FLAGS_RELEASE} ${LOG_OUTPUT_FLAG} /arch:AVX2 -DPOCO_STATIC -DUSE_AVX2")

	set(CMAKE_EXE_LINKER_FLAGS_RELEASE_AVX "${CMAKE_EXE_LINKER_FLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE_AVX2 "${CMAKE_EXE_LINKER_FLAGS}")

	# 64 bit
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(POCO_LIB_DIR lib64)
	# 32 bit
	elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
		set(POCO_LIB_DIR lib)
	endif ()

	link_directories(
		${POCO_ROOT}/${POCO_LIB_DIR}
		${OPENSSL_ROOT}/lib
	)

	include_directories(
		${POCO_ROOT}/Net/include
		${POCO_ROOT}/Foundation/include
		${POCO_ROOT}/NetSSL_OpenSSL/include
		${POCO_ROOT}/Crypto/include
		${POCO_ROOT}/Util/include
		${POCO_ROOT}/JSON/include
		${OPENSSL_ROOT}/include
	)

	set_source_files_properties(icon.ico src/resources.rc PROPERTIES LANGUAGE RC)
	add_definitions(-DUNICODE -D_UNICODE)
endif ()

##################################################################
# Header files
##################################################################
set(HEADER_FILES
	src/nxt/nxt_address.h
	src/shabal/cuda/Shabal.hpp
	src/shabal/impl/mshabal_avx_impl.hpp
	src/shabal/impl/mshabal_avx2_impl.hpp
	src/shabal/impl/sphlib_impl.hpp
	src/shabal/mshabal/mshabal.h
	src/shabal/sphlib/sph_shabal.h
	src/shabal/sphlib/sph_types.h
	src/Account.hpp
	src/Deadline.hpp
	src/Declarations.hpp
	src/Miner.hpp
	src/MinerConfig.hpp
	src/MinerData.hpp
	src/MinerLogger.hpp
	src/MinerServer.hpp
	src/MinerShabal.hpp
	src/MinerUtil.hpp
	src/NonceSubmitter.hpp
	src/Output.hpp
	src/PlotReader.hpp
	src/PlotSizes.hpp
	src/PlotVerifier.hpp
	src/Request.hpp
	src/RequestHandler.hpp
	src/Response.hpp
	src/Url.hpp
	src/Wallet.hpp
	src/resources.rc)

##################################################################
# Source files
##################################################################
set(SOURCE_FILES
	src/nxt/nxt_address.cpp
	src/shabal/mshabal/mshabal_avx1.cpp
	src/shabal/mshabal/mshabal_avx2.cpp
	src/shabal/sphlib/sph_shabal.cpp
	src/Account.cpp
	src/Deadline.cpp
	src/Declarations.cpp
	src/main.cpp
	src/Miner.cpp
	src/MinerConfig.cpp
	src/MinerData.cpp
	src/MinerLogger.cpp
	src/MinerServer.cpp
	src/MinerUtil.cpp
	src/NonceSubmitter.cpp
	src/Output.cpp
	src/PlotReader.cpp
	src/PlotSizes.cpp
	src/PlotVerifier.cpp
	src/Request.cpp
	src/RequestHandler.cpp
	src/Response.cpp
	src/Url.cpp
	src/Wallet.cpp)

##################################################################
# Executable
##################################################################
add_executable(creepMiner ${SOURCE_FILES} ${HEADER_FILES})

##################################################################
# Libraries
##################################################################
if (UNIX OR APPLE)
	target_link_libraries(creepMiner
		pthread
		optimized PocoFoundation debug PocoFoundationd
		optimized PocoNet debug PocoNetd
		optimized PocoNetSSL debug PocoNetSSLd
		optimized PocoUtil debug PocoUtild
		optimized PocoCrypto debug PocoCryptod
		optimized PocoJSON debug PocoJSONd)
elseif (MSVC)
	target_link_libraries (creepMiner
			Ws2_32
			optimized PocoFoundationmd debug PocoFoundationmdd
			optimized PocoNetmd debug PocoNetmdd
			optimized PocoNetSSLmd debug PocoNetSSLmdd
			optimized PocoUtilmd debug PocoUtilmdd
			optimized PocoCryptomd debug PocoCryptomdd
			optimized PocoJSONmd debug PocoJSONmdd
			iphlpapi
			libcrypto
			libssl)

	# 64 bit
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(lib-crypto libcrypto-1_1-x64.dll)
		set(lib-ssl libssl-1_1-x64.dll)
	# 32 bit
	elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
		set(lib-crypto libcrypto-1_1-x32.dll)
		set(lib-ssl libssl-1_1-x32.dll)
	endif ()

	configure_file(${OPENSSL_ROOT}/bin/${lib-crypto} ${CMAKE_CURRENT_BINARY_DIR}/bin/${lib-crypto} COPYONLY)
	configure_file(${OPENSSL_ROOT}/bin/${lib-ssl} ${CMAKE_CURRENT_BINARY_DIR}/bin/${lib-ssl} COPYONLY)
endif ()

##################################################################
# Additional files
##################################################################
if (NOT ${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/bin/mining.conf DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin/mining.conf)
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/bin/public DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin)
endif ()

##################################################################
# Naming
##################################################################
set_target_properties(creepMiner PROPERTIES RELEASE_AVX_POSTFIX -avx)
set_target_properties(creepMiner PROPERTIES RELEASE_AVX2_POSTFIX -avx2)
