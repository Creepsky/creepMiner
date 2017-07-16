#!/bin/sh
set -ex
wget https://pocoproject.org/releases/poco-1.7.6/poco-1.7.6-all.tar.gz
tar -xzvf poco-1.7.6-all.tar.gz
cd poco-1.7.6-all && ./configure --no-tests --no-samples --omit=Data/ODBC,Data/MySQL && make -j $(nproc) && sudo ldconfig && sudo make install
