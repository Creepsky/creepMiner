#!/bin/sh
set -ex
wget https://pocoproject.org/releases/poco-1.7.8/poco-1.7.8p3-all.tar.gz
tar -xzvf poco-1.7.8p3-all.tar.gz
cd poco-1.7.8p3-all && ./configure --no-tests --no-samples --omit=Data/ODBC,Data/MySQL && make && sudo ldconfig && sudo make install
