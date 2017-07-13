##################################################################
# Project and CMake settings
##################################################################
cmake_minimum_required(VERSION 3.2.2)
project(creepMiner VERSION 1.6.0)
set(CMAKE_CXX_STANDARD 11)

##################################################################
# Configuration types
##################################################################
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

##################################################################
# Environment variables
##################################################################
if (MSVC)
	set(POCO_ROOT "$ENV{POCO_ROOT}" CACHE PATH "POCO root folder")
	set(OPENSSL_ROOT "$ENV{OPENSSL_ROOT}" CACHE PATH "OpenSSL root folder")
endif ()

##################################################################
# LOG_TERMINAL / LOG_SERVICE
##################################################################
set(LOG_OUTPUT "TERMINAL" CACHE STRING "Logging output location")
set_property(CACHE LOG_OUTPUT PROPERTY STRINGS TERMINAL SERVICE) 

if (LOG_OUTPUT STREQUAL "SERVICE")
	set(LOG_OUTPUT_FLAG "-DLOG_SERVICE")
else ()
	set(LOG_OUTPUT_FLAG "-DLOG_TERMINAL")
endif ()

##################################################################
# CPU instruction set
##################################################################
set(CPU_INSTRUCTION_SET "SSE2" CACHE STRING "Additional CPU instruction set")
set_property(CACHE CPU_INSTRUCTION_SET PROPERTY STRINGS None SSE2 SSE4 AVX AVX2)

if (CPU_INSTRUCTION_SET STREQUAL "AVX2")
	add_definitions(-DUSE_AVX2)
	if (MSVC)
		add_definitions(/arch:AVX2)
	else ()
		add_definitions(-mavx2)
	endif ()
elseif (CPU_INSTRUCTION_SET STREQUAL "AVX")
	add_definitions(-DUSE_AVX)
	if (MSVC)
		add_definitions(/arch:AVX)
	else ()
		add_definitions(-mavx)
	endif ()
elseif (CPU_INSTRUCTION_SET STREQUAL "SSE2")
	if (MSVC)
		add_definitions(/arch:SSE2)
	else ()
		add_definitions(-msse2)
	endif ()
elseif (CPU_INSTRUCTION_SET STREQUAL "SSE4")
	add_definitions(-DUSE_SSE4)
	if (MSVC)
	else ()
		add_definitions(-msse4)
	endif ()
endif ()

##################################################################
# GPU acceleration
##################################################################
set(GPU_ACCELERATION "NONE" CACHE STRING "GPU acceleration")
set_property(CACHE GPU_ACCELERATION PROPERTY STRINGS NONE CUDA) 

if (GPU_ACCELERATION STREQUAL "CUDA")
	find_package(CUDA REQUIRED)
	set(CREEPMINER_POSTFIX ${CREEPMINER_POSTFIX}-cuda)
	set(GPU_CUDA_ARCH "sm_52" CACHE STRING "The CUDA architecture")
	set(CUDA_NVCC_FLAGS
		${CUDA_NVCC_FLAGS}
		-O3 -std=c++11
		-arch=${GPU_CUDA_ARCH}
		-gencode=arch=compute_30,code=sm_30
		-gencode=arch=compute_50,code=sm_50
		-gencode=arch=compute_52,code=sm_52
		-gencode=arch=compute_52,code=compute_52
		-gencode=arch=compute_60,code=sm_60
		-gencode=arch=compute_61,code=sm_61
		-gencode=arch=compute_62,code=sm_62)
	add_definitions(-DMINING_CUDA)
endif ()

##################################################################
# Additional options
##################################################################
option(OVERWRITE_MINING_CONF "If yes, on install the mining.conf will be overwritten" OFF)

##################################################################
# UNIX / BSD specific
##################################################################
if (UNIX OR APPLE)
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${LOG_OUTPUT_FLAG}")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${LOG_OUTPUT_FLAG} -O3")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${LOG_OUTPUT_FLAG} -O3")
	set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} ${LOG_OUTPUT_FLAG} -O3")
	 
	add_definitions(
		${LOG_OUTPUT_FLAG}
		-D_REENTRANT
		-Wall
		-march=native)

	link_directories(/usr/local/lib)
	include_directories(
		/usr/local/include
		${CMAKE_CURRENT_SOURCE_DIR}/src)
##################################################################
# MSVC specific
##################################################################
elseif (MSVC)
	add_definitions(
		-DPOCO_STATIC
		${LOG_OUTPUT_FLAG}
		-DUNICODE
		-D_UNICODE)

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
		${CMAKE_CURRENT_SOURCE_DIR}/src
	)

	set_source_files_properties(icon.ico src/resources.rc PROPERTIES LANGUAGE RC)
endif ()

##################################################################
# Header files
##################################################################
set(HEADER_FILES
	src/logging/channels/ColoredPriorityConsoleChannel.hpp
	src/logging/channels/MinerDataChannel.hpp
	src/logging/Console.hpp
	src/logging/Message.hpp
	src/logging/MinerLogger.hpp
	src/logging/Output.hpp
	src/logging/ProgressPrinter.hpp
	src/mining/Deadline.hpp
	src/mining/Miner.hpp
	src/mining/MinerConfig.hpp
	src/mining/MinerData.hpp
	src/network/NonceSubmitter.hpp
	src/network/Response.hpp
	src/network/Request.hpp
	src/network/Url.hpp
	src/nxt/nxt_address.h
	src/plots/Plot.hpp
	src/plots/PlotReader.hpp
	src/plots/PlotSizes.hpp
	src/plots/PlotVerifier.hpp
	src/shabal/cuda/Shabal.hpp
	src/shabal/impl/mshabal_avx_impl.hpp
	src/shabal/impl/mshabal_avx2_impl.hpp
	src/shabal/impl/sphlib_impl.hpp
	src/shabal/mshabal/mshabal.h
	src/shabal/sphlib/sph_shabal.h
	src/shabal/sphlib/sph_types.h
	src/shabal/MinerShabal.hpp
	src/wallet/Account.hpp
	src/wallet/Wallet.hpp
	src/webserver/MinerServer.hpp
	src/webserver/RequestHandler.hpp
	src/Declarations.hpp
	src/MinerUtil.hpp
	src/resources.rc)

##################################################################
# Source files
##################################################################
set(SOURCE_FILES
	src/logging/channels/ColoredPriorityConsoleChannel.cpp
	src/logging/channels/MinerDataChannel.cpp
	src/logging/Console.cpp
	src/logging/Message.cpp
	src/logging/MinerLogger.cpp
	src/logging/Output.cpp
	src/logging/ProgressPrinter.cpp
	src/mining/Deadline.cpp
	src/mining/Miner.cpp
	src/mining/MinerConfig.cpp
	src/mining/MinerData.cpp
	src/network/NonceSubmitter.cpp
	src/network/Response.cpp
	src/network/Request.cpp
	src/network/Url.cpp
	src/nxt/nxt_address.cpp
	src/plots/Plot.cpp
	src/plots/PlotReader.cpp
	src/plots/PlotSizes.cpp
	src/plots/PlotVerifier.cpp
	src/wallet/Account.cpp
	src/wallet/Wallet.cpp
	src/shabal/sphlib/sph_shabal.cpp
	src/webserver/MinerServer.cpp
	src/webserver/RequestHandler.cpp
	src/Declarations.cpp
	src/main.cpp
	src/MinerUtil.cpp)

##################################################################
# GPU acceleration source files
##################################################################
if (GPU_ACCELERATION STREQUAL "CUDA")
	set(SOURCE_FILES
		${SOURCE_FILES}
		src/shabal/cuda/Shabal.cu)
endif ()

##################################################################
# CPU instruction set source files
##################################################################
if (CPU_INSTRUCTION_SET STREQUAL "AVX")
	set(SOURCE_FILES
		${SOURCE_FILES}
		src/shabal/mshabal/mshabal_avx1.cpp)
elseif (CPU_INSTRUCTION_SET STREQUAL "AVX2")
	set(SOURCE_FILES
		${SOURCE_FILES}
		src/shabal/mshabal/mshabal_avx2.cpp)
elseif (CPU_INSTRUCTION_SET STREQUAL "SSE4")
	set(SOURCE_FILES
		${SOURCE_FILES}
		src/shabal/mshabal/mshabal_sse4.cpp)
endif ()

##################################################################
# Executable
##################################################################
if (GPU_ACCELERATION STREQUAL "CUDA")
	cuda_add_executable(creepMiner ${SOURCE_FILES} ${HEADER_FILES})
else ()
	add_executable(creepMiner ${SOURCE_FILES} ${HEADER_FILES})
endif ()

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
# Naming
##################################################################
if (CPU_INSTRUCTION_SET STREQUAL "AVX2")
	set(CREEPMINER_POSTFIX ${CREEPMINER_POSTFIX}-avx2)
elseif (CPU_INSTRUCTION_SET STREQUAL "AVX")
	set(CREEPMINER_POSTFIX ${CREEPMINER_POSTFIX}-avx)
elseif (CPU_INSTRUCTION_SET STREQUAL "SSE2")
	set(CREEPMINER_POSTFIX ${CREEPMINER_POSTFIX}-sse2)
elseif (CPU_INSTRUCTION_SET STREQUAL "SSE4")
	set(CREEPMINER_POSTFIX ${CREEPMINER_POSTFIX}-sse4)
else ()
	set(CREEPMINER_POSTFIX ${CREEPMINER_POSTFIX}"")
endif ()

set_target_properties(creepMiner PROPERTIES
		DEBUG_POSTFIX ${CREEPMINER_POSTFIX}-d
		RELEASE_POSTFIX ${CREEPMINER_POSTFIX}
		MINSIZEREL_POSTFIX ${CREEPMINER_POSTFIX}
		RELWITHDEBINFO_POSTFIX ${CREEPMINER_POSTFIX})

##################################################################
# Installing
##################################################################
install(TARGETS creepMiner RUNTIME DESTINATION .)
install(DIRECTORY bin/public DESTINATION .)

if (OVERWRITE_MINING_CONF)
	install(FILES bin/mining.conf DESTINATION .)
endif  ()
