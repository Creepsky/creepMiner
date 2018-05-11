cd ..
conan install . --build=missing -o Poco:shared=False -o OpenSSL:shared=False -o zlib:shared=False -if conan/debug -s build_type=Debug
conan install . --build=missing -o Poco:shared=False -o OpenSSL:shared=False -o zlib:shared=False -s build_type=Release
pause
