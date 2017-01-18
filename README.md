burst-miner
===========

## What is burst-miner

burst-miner is a client application for mining Burst on a pool. For more informations about Burst follow [this link](https://www.burst-team.us/).
It is written in C++ and therefore can be compiled on different operating systems.
It is designed multi-threaded to reach the best performance. While mining, only the CPU is used.

## Fork

This is a fork of the burst-miner found on [this github repository](https://github.com/uraymeiviar/burst-miner).
The purpose of this fork is to optimize and beautify the original program and on this way make it more user friendly.

## Build

| Platform | Master |
| -------- | ------ |
|   Linux   | [![Build Status](https://travis-ci.org/Creepsky/burst-miner.svg?branch=master)](https://travis-ci.org/Creepsky/burst-miner) |
|   Windows   | [![Build status](https://ci.appveyor.com/api/projects/status/f78q7xbf4lh6q491/branch/master?svg=true)](https://ci.appveyor.com/project/Creepsky75522/burst-miner/branch/master) |

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

## Config

You can use a custom config-file by explicit naming it on call: ```burst-miner.exe <config-file>```.
If you leave the config-file-parameter empty, the config-file with the name mining.conf in the execution-path will be used.

Inside the config-file, you can define the following settings:

```
{
    "poolUrl" : "http://pool.burst-team.us:8124",
    "miningInfoUrl" : "http://pool.burst-team.us:8124",
    "walletUrl" : "https://wallet.burst-team.us:8128",
    "submissionMaxRetry" : 3,
    "timeout" : 30,
    "maxBufferSizeMB" : 128,
    "output" :
    [
		"+progress",
		"-debug"
    ],
    "plots" : 
    [
        "C:\\plots",
        "/Users/uraymeiviar/Documents/plots"
    ],
    "Start Server": true,
    "serverUrl" : "http://localhost:80",
    "targetDeadline": "1m 2d 12:34:56"
}
```

|  setting  |  description  |
| --------- | ------------- |
| **poolUrl** | the host, to whom the miner will connect. |
| **miningInfoUrl** | the host, where the miner will get the mining info. If empty, its set to poolUrl. |
| **walletUrl** | the host, where the miner will get the wallet infos. **Optional**! |
| **submissionMaxRetry** | the max tries to resend a message to the server. |
| **timeout** | the max time to wait for a response from the server. |
| **maxBufferSizeMB** | the buffer size while reading the plot files. |
| **output** | decides, what messages will be printed in the output. For possible values see [Output-Flags](#output-flags).  |
| **plots** | the paths to the directories, where to search for plot files. plot files are searched every new block. |
| **Start Server** | the miner will run a local HTTP server and transfer the mining process to all clients. (true|false) |
| **serverUrl** | the uri and port, where the miner will deploy the HTTP server. only if Start Server == true. the chosen port must be not used already! |
| **targetDeadline** | found nonces need to have a lower deadline then this value to be submitted. If targetDeadline is 0 (default), this feature is ignored. The target deadline can be given as a number (for example "targetDeadline" : 12345), which represents the total seconds of the deadline, or as a formatted string (for example "targetDeadline" : "1y 2d 12:34:56"). |

## Output-Flags

Output-Flags can be set in the **output** block in the configuration file.
If a flag is given, it will be set in the miner application.

It is possible to explicit set flags with the prefix **+** (for example "+nonce found") or unset with the prefix **-** (for example "-nonce found").
If no prefix is set, the **+** prefix is set implicitly.

| flag | description |
| ---- | ----------- |
| progress | the progress in percent while reading the plot files |
| debug | debug messages |
| nonce found | shows every found nonce with the deadline |
| nonce found plot | (only in comination with **nonce found**) shows the plot file of found nonce |
| nonce confirmed plot | shows the plot file of confirmed nonces |
| plot done | shows done plot files while searching for nonces |
| dir done | shows done dirs while searching for nonces |
| last winner | shows the winner of the last block (only with a valid wallet url) |

## Local HTTP server

If you start the miner with the settings **Start Server : true** and a valid **serverUrl**, the miner will run a HTTP server.
Simply use your favorite browser and go to the location where the server is running (serverUrl).

The HTTP server shows the intern mining process and some additional informations.

## Compiling

If you want to compile the project by yourself, please note the following informations.

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
You find the configurations:

```
Release: release mode with SSE2 instruction set
Release_AVX: release mode with AVX instruction set
Release_AVX2: release mode with AVX2 instruction set
```

## Contribution

We are glad about every contribution to the project. Dont hesitate to open an issue, if you found a bug (with or without fix) or have an idea for a new feature!

If you want to share your own code, please follow these steps:
- create a fork of this repository
- add a new branch for your changings
- add your changes to the code
- dont forget to mention the issue number in the commit messages (just write something like ```<message> #<id>```)
- open a pull request and try to describe what the change is for
- done :)

