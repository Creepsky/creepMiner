##################################################################
# Project and CMake settings
##################################################################
cmake_minimum_required(VERSION 3.0.2)
project(creepMiner VERSION 1.7.0)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_BUILD_TYPE RELEASE)

##################################################################
# Configuration types
##################################################################
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

option(MINIMAL_BUILD "If yes, the miner will be build without any extras like CUDA, CPU instructions..." OFF)
option(NO_GPU "If yes, the miner will be build without CUDA and OpenCL." OFF)

##################################################################
# Environment variables
##################################################################
if (MSVC)
	set(POCO_ROOT "$ENV{POCO_ROOT}" CACHE PATH "POCO root folder")
	set(OPENSSL_ROOT "$ENV{OPENSSL_ROOT}" CACHE PATH "OpenSSL root folder")
endif ()

##################################################################
# CPU architecture (only for Unix/Mac)
##################################################################
if (UNIX OR APPLE)
	set(CPU_ARCH "X64" CACHE STRING "CPU architecture")
	set_property(CACHE CPU_ARCH PROPERTY STRINGS X32 X64)
	
	if (CPU_ARCH STREQUAL "X32")
		add_definitions(-m32)
	endif ()
endif ()

##################################################################
# GPU acceleration
##################################################################
option(USE_CUDA "If yes, CUDA will be enabled" ON)
option(USE_OPENCL "If yes, OpenCL will be enabled" ON)

if (USE_CUDA AND NOT MINIMAL_BUILD AND NOT NO_GPU)
	 find_package(CUDA REQUIRED)
	 add_definitions(-DUSE_CUDA)
	 set(GPU_CUDA_ARCH "sm_52" CACHE STRING "The CUDA architecture")
	 set(CUDA_NVCC_FLAGS
		 ${CUDA_NVCC_FLAGS}
		 -arch=${GPU_CUDA_ARCH}
		 -std=c++11
		 -gencode=arch=compute_30,code=sm_30
		 -gencode=arch=compute_50,code=sm_50
		 -gencode=arch=compute_52,code=sm_52
		 -gencode=arch=compute_52,code=compute_52
		 -gencode=arch=compute_60,code=sm_60
		 -gencode=arch=compute_61,code=sm_61
		 -gencode=arch=compute_62,code=sm_62)
endif()

if (USE_OPENCL AND NOT MINIMAL_BUILD AND NOT NO_GPU)
	find_package(OpenCL REQUIRED)
	add_definitions(-DUSE_OPENCL)
endif ()

##################################################################
# Additional options
##################################################################
option(OVERWRITE_MINING_CONF "If yes, on install the mining.conf will be overwritten" OFF)

##################################################################
# UNIX / BSD specific
##################################################################
if (UNIX OR APPLE)	 
	add_definitions(
		-D_REENTRANT
		-Wall
		-march=native
		-std=c++14)

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
	src/gpu/algorithm/gpu_algorithm_atomic.hpp
	src/gpu/impl/gpu_cuda_impl.hpp
	src/gpu/impl/gpu_opencl_impl.hpp
	src/gpu/gpu_algorithm_shell.hpp
	src/gpu/gpu_declarations.hpp
	src/gpu/gpu_shell.hpp
	src/logging/channels/ColoredPriorityConsoleChannel.hpp
	src/logging/channels/MinerDataChannel.hpp
	src/logging/Console.hpp
	src/logging/Message.hpp
	src/logging/MinerLogger.hpp
	src/logging/Output.hpp
	src/logging/ProgressPrinter.hpp
	src/mining/Deadline.hpp
	src/mining/Miner.hpp
	src/mining/MinerCL.hpp
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
	src/logging/Performance.hpp
	src/resources.rc)

##################################################################
# Source files
##################################################################
set(SOURCE_FILES
	src/gpu/impl/gpu_cuda_impl.cpp
	src/gpu/impl/gpu_opencl_impl.cpp
	src/logging/channels/ColoredPriorityConsoleChannel.cpp
	src/logging/channels/MinerDataChannel.cpp
	src/logging/Console.cpp
	src/logging/Message.cpp
	src/logging/MinerLogger.cpp
	src/logging/Output.cpp
	src/logging/ProgressPrinter.cpp
	src/mining/Deadline.cpp
	src/mining/Miner.cpp
	src/mining/MinerCL.cpp
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
	src/shabal/MinerShabal.cpp
	src/wallet/Account.cpp
	src/wallet/Wallet.cpp
	src/shabal/sphlib/sph_shabal.cpp
	src/webserver/MinerServer.cpp
	src/webserver/RequestHandler.cpp
	src/Declarations.cpp
	src/main.cpp
	src/MinerUtil.cpp
	src/logging/Performance.cpp)

##################################################################
# Special files and settings
##################################################################
option(USE_SSE4 "If yes, SSE4 will be enabled" ON)
option(USE_AVX "If yes, AVX will be enabled" ON)
option(USE_AVX2 "If yes, AVX2 will be enabled" ON)

if (USE_SSE4 AND NOT MINIMAL_BUILD)
	add_definitions(-DUSE_SSE4)
	set(SOURCE_FILES ${SOURCE_FILES} src/shabal/mshabal/mshabal_sse4.cpp)
	if (UNIX OR APPLE)
		set_source_files_properties(src/shabal/mshabal/mshabal_sse4.cpp PROPERTIES COMPILE_FLAGS -msse4)
	endif ()
endif ()

if (USE_AVX AND NOT MINIMAL_BUILD)
	add_definitions(-DUSE_AVX)
	set(SOURCE_FILES ${SOURCE_FILES} src/shabal/mshabal/mshabal_avx1.cpp)
	if (UNIX OR APPLE)
		set_source_files_properties(src/shabal/mshabal/mshabal_avx1.cpp PROPERTIES COMPILE_FLAGS -mavx)
	elseif (MSVC)
		set_source_files_properties(src/shabal/mshabal/mshabal_avx1.cpp PROPERTIES COMPILE_FLAGS /arch:AVX)
	endif ()
endif ()

if (USE_AVX2 AND NOT MINIMAL_BUILD)
	add_definitions(-DUSE_AVX2)
	set(SOURCE_FILES ${SOURCE_FILES} src/shabal/mshabal/mshabal_avx2.cpp)
	if (UNIX OR APPLE)
		set_source_files_properties(src/shabal/mshabal/mshabal_avx2.cpp PROPERTIES COMPILE_FLAGS -mavx2)
	elseif (MSVC)
		set_source_files_properties(src/shabal/mshabal/mshabal_avx2.cpp PROPERTIES COMPILE_FLAGS /arch:AVX2)
	endif ()
endif ()

if (USE_CUDA AND NOT MINIMAL_BUILD AND NOT NO_GPU)
	set(SOURCE_FILES ${SOURCE_FILES} src/shabal/cuda/Shabal.cu)
endif ()

configure_file(${CMAKE_SOURCE_DIR}/bin/mining.conf ${CMAKE_CURRENT_BINARY_DIR}/bin/mining.conf COPYONLY)

##################################################################
# Executable
##################################################################
if (USE_CUDA AND NOT MINIMAL_BUILD AND NOT NO_GPU)
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

	 if (USE_OPENCL)
		  target_link_libraries(creepMiner OpenCL::OpenCL)
	 endif ()

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
	configure_file(${CMAKE_SOURCE_DIR}/src/shabal/opencl/mining.cl ${CMAKE_CURRENT_BINARY_DIR}/bin/mining.cl COPYONLY)
endif ()

##################################################################
# Naming
##################################################################
set_target_properties(creepMiner PROPERTIES DEBUG_POSTFIX -d)

##################################################################
# Installing
##################################################################
install(TARGETS	creepMiner RUNTIME DESTINATION .)
install(FILES src/shabal/opencl/mining.cl DESTINATION .)
install(DIRECTORY bin/public DESTINATION .)

if (OVERWRITE_MINING_CONF)
	install(FILES bin/mining.conf DESTINATION .)
endif  ()

if (MSVC)
	install(FILES
		${OPENSSL_ROOT}/bin/${lib-crypto}
		${OPENSSL_ROOT}/bin/${lib-ssl}
		DESTINATION .)
else ()
	install(FILES
		bin/run.sh
		icon.png
		DESTINATION .)
endif ()
