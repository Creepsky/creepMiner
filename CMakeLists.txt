##################################################################
# Project and CMake settings
##################################################################
cmake_minimum_required(VERSION 3.0.2)
project(creepMiner)
set(CMAKE_CXX_STANDARD 14)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RELEASE)
endif ()

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

# get the version number from version.id
file(STRINGS version.id VERSION_RAW)
string(REPLACE "version:" "" VERSION ${VERSION_RAW})

# make a list
string(REPLACE "." ";" VERSION_LIST ${VERSION})

# filter version numbers
list(GET VERSION_LIST 0 VERSION_MAJOR)
list(GET VERSION_LIST 1 VERSION_MINOR)
list(GET VERSION_LIST 2 VERSION_BUILD)
list(GET VERSION_LIST 3 VERSION_REVISION)

# define it for the source files
add_definitions(-DVERSION_MAJOR=${VERSION_MAJOR}
				-DVERSION_MINOR=${VERSION_MINOR}
				-DVERSION_BUILD=${VERSION_BUILD}
				-DVERSION_REVISION=${VERSION_REVISION})

##################################################################
# using Conan for dependencies
##################################################################
option(USE_CONAN "If ON, Conan will be used to satisfy OpenSSL,POCO,and zlib dependency" ON)

if (USE_CONAN)
  # init conan
  if (CONAN_DEBUG)
    include(conan/debug/conanbuildinfo.cmake)
  else ()
    include(conanbuildinfo.cmake)
  endif ()

  conan_basic_setup()
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
		 -default-stream per-thread
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
	include_directories(${OpenCL_INCLUDE_DIR})
	add_definitions(-DUSE_OPENCL)
endif ()

##################################################################
# Additional options
##################################################################
option(CONAN_DEBUG "Only for development" OFF)

##################################################################
# UNIX / BSD specific
##################################################################
if (UNIX OR APPLE)	 
	add_definitions(-D_REENTRANT -std=c++14)
	link_directories(/usr/local/lib)
	include_directories(/usr/local/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
##################################################################
# MSVC specific
##################################################################
elseif (MSVC)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
	set_source_files_properties(bin/icon.ico src/resources.rc PROPERTIES LANGUAGE RC)
	add_definitions(/MP)
endif ()

##################################################################
# Source files
##################################################################
file(GLOB SOURCE_FILES
    src/*.*pp
    src/gpu/impl/*.*pp
    src/logging/channels/*.*pp
    src/logging/*.*pp
    src/mining/*.*pp
	src/network/*.*pp
	src/nxt/nxt_address.h
	src/nxt/nxt_address.cpp
	src/plots/*.*pp
	src/shabal/*.*pp
	src/shabal/sphlib/*.*pp
    src/wallet/*.*pp
    src/webserver/*.*pp
	src/resources.rc)

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
		set_source_files_properties(src/shabal/mshabal/mshabal_sse4.cpp PROPERTIES COMPILE_FLAGS -msse2)
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

##################################################################
# Executable
##################################################################
if (USE_CUDA AND NOT MINIMAL_BUILD AND NOT NO_GPU)
	 cuda_add_executable(creepMiner ${SOURCE_FILES})
else ()
	 add_executable(creepMiner ${SOURCE_FILES})
endif ()

##################################################################
# Libraries
##################################################################
target_link_libraries(creepMiner ${CONAN_LIBS})

if (NOT USE_CONAN)
  find_package(Poco REQUIRED Foundation Util Net Crypto NetSSL)
  target_link_libraries(creepMiner ${Poco_LIBRARIES})
endif()

if (USE_OPENCL)
	target_link_libraries(creepMiner ${OpenCL_LIBRARY})
endif ()

##################################################################
# Naming
##################################################################
set_target_properties(creepMiner PROPERTIES DEBUG_POSTFIX -d)

##################################################################
# Installing
##################################################################
install(FILES src/shabal/opencl/mining.cl DESTINATION .)
install(DIRECTORY resources/public DESTINATION .)

if (NOT MSVC)
	install(DIRECTORY bin/ DESTINATION . USE_SOURCE_PERMISSIONS)
	install(FILES resources/run.sh resources/screen.sh DESTINATION . PERMISSIONS
			OWNER_READ OWNER_WRITE OWNER_EXECUTE
			GROUP_READ GROUP_WRITE GROUP_EXECUTE
			WORLD_READ WORLD_WRITE WORLD_EXECUTE)
	if (UNIX)
		install(DIRECTORY lib/ DESTINATION lib)
	endif ()
else ()
	install(TARGETS creepMiner RUNTIME DESTINATION .)
endif ()

##################################################################
# Packaging
##################################################################
set(CPACK_PACKAGE_NAME ${CMAKE_PROJECT_NAME})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Burstcoin c++ cpu/gpu miner")
set(CPACK_PACKAGE_VENDOR "Creepsky")
set(CPACK_PACKAGE_VERSION ${VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_BUILD})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md")
SET(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_SOURCE_PACKAGE_FILE "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}")
#set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

if (UNIX)
    set(CPACK_SET_DESTDIR true)
    set(CPACK_INSTALL_PREFIX "/opt/${CMAKE_PROJECT_NAME}-${VERSION}")
    set(CPACK_SOURCE_GENERATOR "TGZ;TBZ2")
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_SECTION "misc")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Creepsky")
	include(CPack)
elseif (WIN32)
	set(CPACK_GENERATOR "ZIP")
	#set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}\\resources\\icon.ico")
	#set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}\\resources\\icon.ico")
	#set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}\\resources\\icon.ico")
	set(CPACK_NSIS_MENU_LINKS "creepMiner.exe" "creepMiner")
	include(CPack)
endif ()
