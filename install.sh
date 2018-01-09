#!/bin/sh
# script to compile and package the creepMiner and all of his dependencies on Linux systems

usage()
{
    echo "Usage:    install.sh [cpu] [gpu] [min] [cuda] [cl] [sse4] [avx] [avx2] [help]"
    echo "cpu:      builds the cpu version (sse2 + sse4 + avx + avx2)"
    echo "gpu:      builds the gpu version (opencl + cuda + cpu)"
    echo "min:      builds the minimal version (only sse2)"
    echo "cuda:     adds CUDA to the build"
    echo "cl:       adds OpenCL to the build"
    echo "sse2:     adds sse2 to the build"
    echo "sse4:     adds sse4 to the build"
    echo "avx:      adds avx to the build"
    echo "avx2:     adds avx2 to the build"
    echo "help:     shows this help"
}

set_cpu()
{
    sse4=$1
    avx=$1
    avx2=$1
}

set_gpu()
{
    cuda=$1
    opencl=$1
}

# default settings
if [ $# = 0 ]; then
    echo "Using default settings"
    set_cpu true
    set_gpu false
fi

# parse settings
for i in $*
do
    if [ $i = "help" ]
    then
        usage
        exit
    elif [ $i = "cpu" ]
    then
        set_cpu true
    elif [ $i = "gpu" ]
    then
        set_gpu true
    elif [ $i = sse4 ]
    then
        sse4=true
    elif [ $i = "avx" ]
    then
        avx=true
    elif [ $i = "avx2" ]
    then
        avx2=true
    elif [ $i = "cl" ]
    then
        opencl=true
    elif [ $i = "cuda" ]
    then
        cuda=true
    fi
done

use_flag()
{
    if [ "$2" = true ]; then
        echo "-D$1=ON"
    else
        echo "-D$1=OFF"
    fi
}

use_sse4=$(use_flag "USE_SSE4" $sse4)
use_avx=$(use_flag "USE_AVX" $avx)
use_avx2=$(use_flag "USE_AVX2" $avx2)
use_opencl=$(use_flag "USE_OPENCL" $opencl)
use_cuda=$(use_flag "USE_CUDA" $cuda)

echo $use_sse4
echo $use_avx
echo $use_avx2
echo $use_opencl
echo $use_cuda

pip install conan --user
conan install --build=missing -s compiler.libcxx=libstdc++11
rm CMakeCache.txt -y
cmake . -DCMAKE_BUILD_TYPE=RELEASE $use_sse4 $use_avx $use_avx2 $use_opencl $use_cuda
make -j$(nproc)
make package

