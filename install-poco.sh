#!/bin/sh
set -ex
wget https://pocoproject.org/releases/poco-1.7.6/poco-1.7.6-all.tar.gz
tar -xzvf poco-1.7.6-all.tar.gz
sudo apt-get install -y openssl libssl-dev
cd poco-1.7.6-all && ./configure --no-tests --no-samples && make -j $(nproc) && sudo ldconfig && sudo make install
