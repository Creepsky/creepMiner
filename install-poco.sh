#!/bin/sh
set -ex
wget https://pocoproject.org/releases/poco-1.7.6/poco-1.7.6.tar.gz
tar -xzf poco-1.7.6.tar.gz
cd Poco-1.7.6 && ./configure --prefix=/usr --no-tests --no-samples && make && sudo make install