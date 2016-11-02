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

[![Build Status](https://travis-ci.org/Creepsky/burst-miner.svg?branch=master)](https://travis-ci.org/Creepsky/burst-miner)

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
    "poolUrl" : "burst-pool.cryptoport.io",
    "submissionMaxRetry" : 3,
    "socketTimeout" : 60,
    "maxBufferSizeMB" : 128,
	"output" : ["debug", "progress"],
    "plots" : 
    [
    	"/Users/uraymeiviar/plots",
    	"/Users/uraymeiviar/Documents/plots"
	]
 }
```

**poolUrl** : the host, to whom the miner will connect.

**submissionMaxRetry** : the max tries to resend a message to the server.

**socketTimeout** : the max time to wait for a response from the server.

**maxBufferSizeMB** : the buffer size while reading the plot files.

**output** : decides, what messages will be seen in the output. Possible values: progress (the progress in percent while reading the plot files), debug (debug messages).

**plots** : the paths to the directories, where to search for plot files. plot files are searched every new block.
