#pragma once

#ifndef WIN32
#   include <netdb.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include "unistd.h"
#	include <dirent.h>
#   ifndef MSG_NOSIGNAL
#       define MSG_NOSIGNAL SO_NOSIGPIPE
#   endif
#	define closesocket close
#	define SOCKET int
#	define PATH_SEPARATOR '/'
#else
#   define MSG_NOSIGNAL 0
#	include "win/dirent.h"
#	include <WinSock2.h>
#	include <ws2tcpip.h>
#	include <intrin.h>
#	define strcasecmp _stricmp 
#	define strncasecmp _strnicmp 
#	define SHUT_RDWR SD_BOTH
#	define SHUT_RD SD_RECEIVE
#	define SHUT_WR SD_SEND
#   define __builtin_bswap64 _byteswap_uint64
#	define PATH_SEPARATOR '\\'
#endif
