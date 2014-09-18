//
//  nxt_address.cpp
//  cryptoport-miner
//
//  Created by Uray Meiviar on 9/19/14.
//  Copyright (c) 2014 Miner. All rights reserved.
//

#include "nxt_address.h"

#ifdef WIN32
#	define strcasecmp _stricmp 
#	define strncasecmp _strnicmp 
#endif

char NxtAddress::alphabet[33] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
char cwmap[17] = { 3, 2, 1, 0, 7, 6, 5, 4, 13, 14, 15, 16, 12, 8, 9, 10, 11, };
int NxtAddress::gexp[32] = { 1, 2, 4, 8, 16, 5, 10, 20, 13, 26, 17, 7, 14, 28, 29, 31, 27, 19, 3, 6, 12, 24, 21, 15, 30, 25, 23, 11, 22, 9, 18, 1 };
int NxtAddress::glog[32] = { 0, 0, 1, 18, 2, 5, 19, 11, 3, 29, 6, 27, 20, 8, 12, 23, 4, 10, 30, 17, 7, 22, 28, 26, 21, 25, 9, 16, 13, 14, 24, 15  };

NxtAddress::NxtAddress(uint64_t id)
{
    for(int i = 0; i < 13; i++)
	{
		codeword[i] = (id >> (5 * i)) & 31;
	}
    
	encode();
}

NxtAddress::operator uint64_t()
{
	uint64_t acc = 0;
    
	for(int i = 0; i < 13; i++)
	{
		acc |= uint64_t(codeword[i] & 31) << (5 * i);
	}
    
	return acc;
}

void NxtAddress::operator=(uint64_t acc)
{
	for(int i = 0; i < 13; i++)
	{
		codeword[i] = (acc >> (5 * i)) & 31;
	}
    
	encode();
}

char* NxtAddress::c_str(bool prefix)
{
	static char out[32]; int pos = 0;
    
	if(prefix){ strcpy(out, "BURST-"); pos = 4; }
    
	for(int i = 0; i < 17; i++)
	{
		out[pos++] = alphabet[(int)codeword[(int)cwmap[i]]];
        
		if((i & 3) == 3 && i < 13) out[pos++] = '-';
	}
    
	out[pos] = 0;
    
	return out;
}

std::string NxtAddress::to_string()
{
    return std::string(this->c_str());
}

char* NxtAddress::account_id()
{
	static char out[21];
    
	sprintf(out, "%llu", (unsigned long long)(*this));
    
	return out;
}

bool NxtAddress::set(char *adr)
{
	int len = 0; memset(codeword, 1, 17);
    
	if(!strncasecmp(adr, "BURST-", 4)) adr += 4;
    
	size_t digits = strspn(adr, "0123456789 \t");
    
	if(adr[digits] == 0) // account id
	{
		if(digits > 5 && digits < 21)
		{
            uint64_t acc;
            
			if(digits == 20 && *adr != '1') return false;
			
			if(sscanf(adr, "%llu", (unsigned long long*)&acc) == 1)
			{
				*this = acc; return true;
			}
		}
	}
	else // address
	{
		while(*adr)
		{
			char c = *adr++;
            
			if(c >= 'a' && c <= 'z') c -= 'a'-'A';
            
			char *p = strchr(alphabet, c); if(!p) continue;
            
			if(len > 16) return false;
            
			codeword[(int)cwmap[len++]] = (char)(p - alphabet);
		}
	}
    
	return (len == 17 ? ok() : false);
}

int NxtAddress::gmult(int a, int b)
{
    if(a == 0 || b == 0) return 0;
	
    int idx = (glog[a] + glog[b]) % 31;
	
    return gexp[idx];
}

void NxtAddress::encode()
{
    char *p = codeword + 13;
    
    p[3] = p[2] = p[1] = p[0] = 0;
    
    for(int i = 12; i >= 0; i--)
    {
        int fb = codeword[i] ^ p[3];
        
        p[3] = p[2] ^ gmult(30, fb);
        p[2] = p[1] ^ gmult( 6, fb);
        p[1] = p[0] ^ gmult( 9, fb);
        p[0] =        gmult(17, fb);
    }
}

bool NxtAddress::ok()
{
    int sum = 0;
    int t = 0;
	
    for(int i = 1; i < 5; i++)
    {
		t = 0;
        for(int j = 0; j < 31; j++)
        {
            if(j > 12 && j < 27) continue;
            
            int pos = j; if(j > 26) pos -= 14;
            
            t ^= gmult(codeword[pos], gexp[(i*j)%31]);
        }
        
        sum |= t;
        syndrome[i] = t;
    }
    
    return (sum == 0);
}
