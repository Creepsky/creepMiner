burst-miner
===========

## What is burst-miner

burst-miner is a client application for mining Burst on a pool. For more informations about Burst follow [this link](https://www.burst-team.us/).
It is written in C++ and therefore can be compiled on different operating systems.
It is designed multi-threaded to reach the best performance. While mining, only the CPU is used.

## Fork

This is a (standalone) fork of the burst-miner found on [this github repository](https://github.com/uraymeiviar/burst-miner).

## Build

| Platform | Master |
| -------- | ------ |
|   Linux   | [![Build Status](https://travis-ci.org/Creepsky/creepMiner.svg?branch=master)](https://travis-ci.org/Creepsky/creepMiner) |
|   Windows   | [![Build status](https://ci.appveyor.com/api/projects/status/f78q7xbf4lh6q491/branch/master?svg=true)](https://ci.appveyor.com/project/Creepsky75522/creepMiner/branch/master) |

## Config

Please see the [wiki page for setting up the miner](https://github.com/Creepsky/creepMiner/wiki/Setting-up-the-miner).

## Local HTTP server

If you start the miner with the settings **Start Server : true** and a valid **serverUrl**, the miner will run a HTTP server.
Simply use your favorite browser and go to the location where the server is running (serverUrl).

The HTTP server shows the intern mining process and some additional informations.

## Compiling

If you want to compile the project by yourself, please note the following informations.

Also have a look at this [wiki page](https://github.com/Creepsky/creepMiner/wiki/Compilation-&-Installation).

**Dependencies**

The project uses the following POCO C++ libraries:
- Net
- Foundation
- NetSSL
- Crypto
- Util
- JSON

You have to build them on your system to compile the project by yourself.

**Important**: On **Windows**, the static versions are used, on **Linux** the shared ones.

The sources can be found on the [Homepage of POCO](https://pocoproject.org/download/index.html).
Be sure to download the Complete Edition.

**Linux**

In the root directory, you can call the following commands:

```
make (all): build the project, output dir is /bin
make clean: remove the compiled data in /bin
```

Alternativly you can build with CMake. Just use the CMakeLists in the root directory.

**Windows**

Use the Visual Studio project to compile the sources. Everything is set up already.

## Contribution

We are glad about every contribution to the project. Dont hesitate to open an issue, if you found a bug (with or without fix) or have an idea for a new feature!

If you want to share your own code, please follow these steps:
- create a fork of this repository
- add a new branch for your changings
- add your changes to the code
- dont forget to mention the issue number in the commit messages (just write something like ```<message> #<id>```)
- open a pull request and try to describe what the change is for
- done :)

## Donations :moneybag:

The orignal author is uray meiviar (uraymeiviar@gmail.com).

If you want to support him, you can donate to:

```
- [ Burst   ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
- [ Bitcoin ] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b
```

If you want to support me, you can donate to:

```
- [ Burst   ] BURST-JBKL-ZUAV-UXMB-2G795
```
