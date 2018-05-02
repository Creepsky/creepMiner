#!/bin/sh
sudo apt install python-pip
pip install --upgrade pip
sudo apt install python-setuptools
pip install conan --user
sh ./install_deps_conan.sh
