creepMiner [![Join the chat at https://gitter.im/creepminer/lobby](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/creepminer/lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
===========

## What is the creepMiner

The creepMiner is a client application for mining Burst on a pool or solo. For more informations about Burst follow [this link](https://www.burst-team.us/).
It is written in C++ and therefore can be compiled on different operating systems.
It is designed multi-threaded to reach the best performance.

## Fork

This is a (standalone) fork of the burst-miner found on [this github repository](https://github.com/uraymeiviar/burst-miner).

## Features

- Mine with your **CPU** (__AVX__/__AVX2__) or your Nvidia **GPU**
- Mine **solo** or in a **pool**
- Filter bad deadlines with the auto target deadline feature
- Build a network of several miners with the forwarding feature
- Watch the status of the miner in your web browser on every device

## Build

| Platform | Master | Development |
| -------- | ------ | ----------- |
|   Linux   | [![Build Status](https://travis-ci.org/Creepsky/creepMiner.svg?branch=master)](https://travis-ci.org/Creepsky/creepMiner) | [![Build Status](https://travis-ci.org/Creepsky/creepMiner.svg?branch=development)](https://travis-ci.org/Creepsky/creepMiner) |
|   Windows   | [![Build status](https://ci.appveyor.com/api/projects/status/8c4pu0t70riqydny/branch/master?svg=true)](https://ci.appveyor.com/project/Creepsky75522/creepminer/branch/master) | [![Build status](https://ci.appveyor.com/api/projects/status/8c4pu0t70riqydny/branch/master?svg=true)](https://ci.appveyor.com/project/Creepsky75522/creepminer/branch/development) |

## Instructions

### [Quickstart](https://github.com/Creepsky/creepMiner/wiki/Quickstart)

- [Compilation & Installation](https://github.com/Creepsky/creepMiner/wiki/Compilation-&-Installation)
- [Setting up the miner](https://github.com/Creepsky/creepMiner/wiki/Setting-up-the-miner)
- [Forwarding](https://github.com/Creepsky/creepMiner/wiki/Forwarding)
- [Solo mining](https://github.com/Creepsky/creepMiner/wiki/Solo-mining)
- [The mining process and otimizations](https://github.com/Creepsky/creepMiner/wiki/The-mining-process-and-otimizations)

## Dependencies

- [POCO C++ libraries](https://pocoproject.org) ([Boost Software License 1.0](https://pocoproject.org/license.html))
- [OpenSSL](https://www.openssl.org) ([Apache style license](https://www.openssl.org/source/license.html))

## Contribution

We are glad about every contribution to the project. Dont hesitate to open an issue, if you found a bug (with or without fix) or have an idea for a new feature!

If you want to share your own code, please follow these steps:
- create a fork of this repository
- add a new branch for your changings
- add your changes to the code
- dont forget to mention the issue number in the commit messages (just write something like ```<message> #<id>```)
- open a pull request and try to describe what the change is for
- done :)