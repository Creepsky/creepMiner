cd ..
conan install . --build=missing -s compiler.libcxx=libstdc++11 -if conan/debug -s build_type=Debug
conan install . --build=missing -s compiler.libcxx=libstdc++11 -s build_type=Release
pause
