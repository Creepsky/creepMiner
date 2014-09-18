/*
	Basic NXT address (without error correction).

	More info: http://wiki.nxtcrypto.org/wiki/New_Address_Format

    Version: 1.0, license: Public Domain, coder: NxtChg (admin@nxtchg.com).
*/

#include <stdio.h>
#include <string.h>
#include <cstring>
#include <stdint.h>
#include <string>

class NxtAddress
{
 public:
    NxtAddress(uint64_t id);
	static char alphabet[33];
	char* account_id();
	bool  set(char *adr);
	char* c_str(bool prefix = false);
    std::string to_string();
	operator uint64_t();
	void operator=(uint64_t acc);
    
private:
    char codeword[17];
    char syndrome[5];
    static int gexp[32];
    static int glog[32];
    int gmult(int a, int b);
    void encode();
    bool ok();
};
