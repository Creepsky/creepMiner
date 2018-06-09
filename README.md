creepMiner [![GPLv3](https://img.shields.io/badge/license-GPLv3-red.svg)](LICENSE.md)
===========

## What is the creepMiner

The creepMiner is a client application for mining Burst on a pool or solo. For more detailed information on Burst mining please see [this link](https://www.burst-team.us/).
creepMiner is written in C++ and is multi-threaded to get the best performance, it can also be compiled on most operating systems.

## Features

- Mine with your **CPU** (__SSE2__/__SSE4__/__AVX__/__AVX2__) or your **GPU** (__OpenCL__, __CUDA__)
- Mine **solo** or in a **pool**
- Multi Mining (Build a network of several miners)
- Filter bad deadlines with the auto target deadline feature
- Responsive web interface for keeping a close eye on your mining 
- **Support for BFS (Burst File System).**

## Quickstart

- Download and install the [latest release](https://github.com/Creepsky/creepMiner/releases/latest)
- Follow the [Setting up the miner](https://github.com/Creepsky/creepMiner/wiki/Setting-up-the-miner) documentation.

If you need help and support then please review the [**FAQ**](https://github.com/Creepsky/creepMiner/wiki/FAQ) and [Setting up the miner](https://github.com/Creepsky/creepMiner/wiki/Setting-up-the-miner) else [![Join the chat at https://discord.gg/qnjyVQt](https://img.shields.io/badge/join-discord-blue.svg)](https://discord.gg/qnjyVQt) chat and ask in the **#help** channel.

Alternatively their is a [![Join the chat at https://gitter.im/creepminer/lobby](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/creepminer/lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge) however that is not as much monitored as discord.

## BFS

For creating a hard disk with BFS do the following steps with bfs.py from https://github.com/brmmm3/plotTools:

1. Initialize hard disk (will overwrite the first 1024 bytes):
python3 bfs.py i /dev/sdX

2. Write plot files to hard disk:
python3 bfs.py w /dev/sdX plotfile1 plotfile2 ...

For further possible commands for bfs.py see https://github.com/brmmm3/plotTools.

In mining.conf add to the list of directories:
"/dev/sdX"

That's it!

## Instructions

- [Setting up the miner](https://github.com/Creepsky/creepMiner/wiki/Setting-up-the-miner)
- [Multi miner setup](https://github.com/Creepsky/creepMiner/wiki/Multi-miner-setup)
- [Solo mining](https://github.com/Creepsky/creepMiner/wiki/Solo-mining)
- [Mining & Optimization](https://github.com/Creepsky/creepMiner/wiki/Mining-&-Optimization)

### Build Status

| Platform | Master | Development |
| -------- | ------ | ----------- |
|   Linux   | [![Build Status](https://travis-ci.org/Creepsky/creepMiner.svg?branch=master)](https://travis-ci.org/Creepsky/creepMiner) | [![Build Status](https://travis-ci.org/Creepsky/creepMiner.svg?branch=development)](https://travis-ci.org/Creepsky/creepMiner) |
|   Windows   | [![Build status](https://ci.appveyor.com/api/projects/status/8c4pu0t70riqydny/branch/master?svg=true)](https://ci.appveyor.com/project/Creepsky75522/creepminer/branch/master) | [![Build status](https://ci.appveyor.com/api/projects/status/8c4pu0t70riqydny/branch/master?svg=true)](https://ci.appveyor.com/project/Creepsky75522/creepminer/branch/development) |
