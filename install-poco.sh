#!/bin/sh
set -ex
wget https://pocoproject.org/releases/poco-1.7.6/poco-1.7.6.tar.gz
tar -xzvf poco-1.7.6.tar.gz
cd poco-1.7.6 && ./configure --prefix=/usr --no-tests --no-samples && make && sudo make install