/*
GPU plot generator for Burst coin.
Author: Cryo
Bitcoin: 138gMBhCrNkbaiTCmUhP9HLU9xwn5QKZgD
Burst: BURST-YA29-QCEW-QXC3-BKXDL
Based on the code of the official miner and dcct's plotgen.
https://github.com/bhamon/gpuPlotGenerator/blob/master/kernel/shabal.cl
*/

#ifndef SHABAL_CL
#define SHABAL_CL

#define SHABAL_C32(x)		((unsigned int)(x ## U))
#define SHABAL_T32(x)		(as_uint(x))

#define SHABAL_INPUT_BLOCK_ADD   do { \
		p_context->B0 = SHABAL_T32(p_context->B0 + M0); \
		p_context->B1 = SHABAL_T32(p_context->B1 + M1); \
		p_context->B2 = SHABAL_T32(p_context->B2 + M2); \
		p_context->B3 = SHABAL_T32(p_context->B3 + M3); \
		p_context->B4 = SHABAL_T32(p_context->B4 + M4); \
		p_context->B5 = SHABAL_T32(p_context->B5 + M5); \
		p_context->B6 = SHABAL_T32(p_context->B6 + M6); \
		p_context->B7 = SHABAL_T32(p_context->B7 + M7); \
		p_context->B8 = SHABAL_T32(p_context->B8 + M8); \
		p_context->B9 = SHABAL_T32(p_context->B9 + M9); \
		p_context->BA = SHABAL_T32(p_context->BA + MA); \
		p_context->BB = SHABAL_T32(p_context->BB + MB); \
		p_context->BC = SHABAL_T32(p_context->BC + MC); \
		p_context->BD = SHABAL_T32(p_context->BD + MD); \
		p_context->BE = SHABAL_T32(p_context->BE + ME); \
		p_context->BF = SHABAL_T32(p_context->BF + MF); \
	} while(0)

#define SHABAL_INPUT_BLOCK_SUB   do { \
		p_context->C0 = SHABAL_T32(p_context->C0 - M0); \
		p_context->C1 = SHABAL_T32(p_context->C1 - M1); \
		p_context->C2 = SHABAL_T32(p_context->C2 - M2); \
		p_context->C3 = SHABAL_T32(p_context->C3 - M3); \
		p_context->C4 = SHABAL_T32(p_context->C4 - M4); \
		p_context->C5 = SHABAL_T32(p_context->C5 - M5); \
		p_context->C6 = SHABAL_T32(p_context->C6 - M6); \
		p_context->C7 = SHABAL_T32(p_context->C7 - M7); \
		p_context->C8 = SHABAL_T32(p_context->C8 - M8); \
		p_context->C9 = SHABAL_T32(p_context->C9 - M9); \
		p_context->CA = SHABAL_T32(p_context->CA - MA); \
		p_context->CB = SHABAL_T32(p_context->CB - MB); \
		p_context->CC = SHABAL_T32(p_context->CC - MC); \
		p_context->CD = SHABAL_T32(p_context->CD - MD); \
		p_context->CE = SHABAL_T32(p_context->CE - ME); \
		p_context->CF = SHABAL_T32(p_context->CF - MF); \
	} while(0)

#define SHABAL_XOR_W   do { \
		p_context->A0 ^= p_context->Wlow; \
		p_context->A1 ^= p_context->Whigh; \
	} while(0)

#define SHABAL_SWAP(v1, v2)   do { \
		unsigned int tmp = (v1); \
		(v1) = (v2); \
		(v2) = tmp; \
	} while(0)

#define SHABAL_SWAP_BC   do { \
		SHABAL_SWAP(p_context->B0, p_context->C0); \
		SHABAL_SWAP(p_context->B1, p_context->C1); \
		SHABAL_SWAP(p_context->B2, p_context->C2); \
		SHABAL_SWAP(p_context->B3, p_context->C3); \
		SHABAL_SWAP(p_context->B4, p_context->C4); \
		SHABAL_SWAP(p_context->B5, p_context->C5); \
		SHABAL_SWAP(p_context->B6, p_context->C6); \
		SHABAL_SWAP(p_context->B7, p_context->C7); \
		SHABAL_SWAP(p_context->B8, p_context->C8); \
		SHABAL_SWAP(p_context->B9, p_context->C9); \
		SHABAL_SWAP(p_context->BA, p_context->CA); \
		SHABAL_SWAP(p_context->BB, p_context->CB); \
		SHABAL_SWAP(p_context->BC, p_context->CC); \
		SHABAL_SWAP(p_context->BD, p_context->CD); \
		SHABAL_SWAP(p_context->BE, p_context->CE); \
		SHABAL_SWAP(p_context->BF, p_context->CF); \
	} while(0)

#define SHABAL_PERM_ELT(xa0, xa1, xb0, xb1, xb2, xb3, xc, xm)   do { \
		xa0 = SHABAL_T32((xa0 \
			^ (((xa1 << 15) | (xa1 >> 17)) * 5U) \
			^ xc) * 3U) \
			^ xb1 ^ (xb2 & ~xb3) ^ xm; \
		xb0 = SHABAL_T32(~(((xb0 << 1) | (xb0 >> 31)) ^ xa0)); \
	} while(0)

#define SHABAL_PERM_STEP_0   do { \
		SHABAL_PERM_ELT(p_context->A0, p_context->AB, p_context->B0, p_context->BD, p_context->B9, p_context->B6, p_context->C8, M0); \
		SHABAL_PERM_ELT(p_context->A1, p_context->A0, p_context->B1, p_context->BE, p_context->BA, p_context->B7, p_context->C7, M1); \
		SHABAL_PERM_ELT(p_context->A2, p_context->A1, p_context->B2, p_context->BF, p_context->BB, p_context->B8, p_context->C6, M2); \
		SHABAL_PERM_ELT(p_context->A3, p_context->A2, p_context->B3, p_context->B0, p_context->BC, p_context->B9, p_context->C5, M3); \
		SHABAL_PERM_ELT(p_context->A4, p_context->A3, p_context->B4, p_context->B1, p_context->BD, p_context->BA, p_context->C4, M4); \
		SHABAL_PERM_ELT(p_context->A5, p_context->A4, p_context->B5, p_context->B2, p_context->BE, p_context->BB, p_context->C3, M5); \
		SHABAL_PERM_ELT(p_context->A6, p_context->A5, p_context->B6, p_context->B3, p_context->BF, p_context->BC, p_context->C2, M6); \
		SHABAL_PERM_ELT(p_context->A7, p_context->A6, p_context->B7, p_context->B4, p_context->B0, p_context->BD, p_context->C1, M7); \
		SHABAL_PERM_ELT(p_context->A8, p_context->A7, p_context->B8, p_context->B5, p_context->B1, p_context->BE, p_context->C0, M8); \
		SHABAL_PERM_ELT(p_context->A9, p_context->A8, p_context->B9, p_context->B6, p_context->B2, p_context->BF, p_context->CF, M9); \
		SHABAL_PERM_ELT(p_context->AA, p_context->A9, p_context->BA, p_context->B7, p_context->B3, p_context->B0, p_context->CE, MA); \
		SHABAL_PERM_ELT(p_context->AB, p_context->AA, p_context->BB, p_context->B8, p_context->B4, p_context->B1, p_context->CD, MB); \
		SHABAL_PERM_ELT(p_context->A0, p_context->AB, p_context->BC, p_context->B9, p_context->B5, p_context->B2, p_context->CC, MC); \
		SHABAL_PERM_ELT(p_context->A1, p_context->A0, p_context->BD, p_context->BA, p_context->B6, p_context->B3, p_context->CB, MD); \
		SHABAL_PERM_ELT(p_context->A2, p_context->A1, p_context->BE, p_context->BB, p_context->B7, p_context->B4, p_context->CA, ME); \
		SHABAL_PERM_ELT(p_context->A3, p_context->A2, p_context->BF, p_context->BC, p_context->B8, p_context->B5, p_context->C9, MF); \
	} while(0)

#define SHABAL_PERM_STEP_1   do { \
		SHABAL_PERM_ELT(p_context->A4, p_context->A3, p_context->B0, p_context->BD, p_context->B9, p_context->B6, p_context->C8, M0); \
		SHABAL_PERM_ELT(p_context->A5, p_context->A4, p_context->B1, p_context->BE, p_context->BA, p_context->B7, p_context->C7, M1); \
		SHABAL_PERM_ELT(p_context->A6, p_context->A5, p_context->B2, p_context->BF, p_context->BB, p_context->B8, p_context->C6, M2); \
		SHABAL_PERM_ELT(p_context->A7, p_context->A6, p_context->B3, p_context->B0, p_context->BC, p_context->B9, p_context->C5, M3); \
		SHABAL_PERM_ELT(p_context->A8, p_context->A7, p_context->B4, p_context->B1, p_context->BD, p_context->BA, p_context->C4, M4); \
		SHABAL_PERM_ELT(p_context->A9, p_context->A8, p_context->B5, p_context->B2, p_context->BE, p_context->BB, p_context->C3, M5); \
		SHABAL_PERM_ELT(p_context->AA, p_context->A9, p_context->B6, p_context->B3, p_context->BF, p_context->BC, p_context->C2, M6); \
		SHABAL_PERM_ELT(p_context->AB, p_context->AA, p_context->B7, p_context->B4, p_context->B0, p_context->BD, p_context->C1, M7); \
		SHABAL_PERM_ELT(p_context->A0, p_context->AB, p_context->B8, p_context->B5, p_context->B1, p_context->BE, p_context->C0, M8); \
		SHABAL_PERM_ELT(p_context->A1, p_context->A0, p_context->B9, p_context->B6, p_context->B2, p_context->BF, p_context->CF, M9); \
		SHABAL_PERM_ELT(p_context->A2, p_context->A1, p_context->BA, p_context->B7, p_context->B3, p_context->B0, p_context->CE, MA); \
		SHABAL_PERM_ELT(p_context->A3, p_context->A2, p_context->BB, p_context->B8, p_context->B4, p_context->B1, p_context->CD, MB); \
		SHABAL_PERM_ELT(p_context->A4, p_context->A3, p_context->BC, p_context->B9, p_context->B5, p_context->B2, p_context->CC, MC); \
		SHABAL_PERM_ELT(p_context->A5, p_context->A4, p_context->BD, p_context->BA, p_context->B6, p_context->B3, p_context->CB, MD); \
		SHABAL_PERM_ELT(p_context->A6, p_context->A5, p_context->BE, p_context->BB, p_context->B7, p_context->B4, p_context->CA, ME); \
		SHABAL_PERM_ELT(p_context->A7, p_context->A6, p_context->BF, p_context->BC, p_context->B8, p_context->B5, p_context->C9, MF); \
	} while(0)

#define SHABAL_PERM_STEP_2   do { \
		SHABAL_PERM_ELT(p_context->A8, p_context->A7, p_context->B0, p_context->BD, p_context->B9, p_context->B6, p_context->C8, M0); \
		SHABAL_PERM_ELT(p_context->A9, p_context->A8, p_context->B1, p_context->BE, p_context->BA, p_context->B7, p_context->C7, M1); \
		SHABAL_PERM_ELT(p_context->AA, p_context->A9, p_context->B2, p_context->BF, p_context->BB, p_context->B8, p_context->C6, M2); \
		SHABAL_PERM_ELT(p_context->AB, p_context->AA, p_context->B3, p_context->B0, p_context->BC, p_context->B9, p_context->C5, M3); \
		SHABAL_PERM_ELT(p_context->A0, p_context->AB, p_context->B4, p_context->B1, p_context->BD, p_context->BA, p_context->C4, M4); \
		SHABAL_PERM_ELT(p_context->A1, p_context->A0, p_context->B5, p_context->B2, p_context->BE, p_context->BB, p_context->C3, M5); \
		SHABAL_PERM_ELT(p_context->A2, p_context->A1, p_context->B6, p_context->B3, p_context->BF, p_context->BC, p_context->C2, M6); \
		SHABAL_PERM_ELT(p_context->A3, p_context->A2, p_context->B7, p_context->B4, p_context->B0, p_context->BD, p_context->C1, M7); \
		SHABAL_PERM_ELT(p_context->A4, p_context->A3, p_context->B8, p_context->B5, p_context->B1, p_context->BE, p_context->C0, M8); \
		SHABAL_PERM_ELT(p_context->A5, p_context->A4, p_context->B9, p_context->B6, p_context->B2, p_context->BF, p_context->CF, M9); \
		SHABAL_PERM_ELT(p_context->A6, p_context->A5, p_context->BA, p_context->B7, p_context->B3, p_context->B0, p_context->CE, MA); \
		SHABAL_PERM_ELT(p_context->A7, p_context->A6, p_context->BB, p_context->B8, p_context->B4, p_context->B1, p_context->CD, MB); \
		SHABAL_PERM_ELT(p_context->A8, p_context->A7, p_context->BC, p_context->B9, p_context->B5, p_context->B2, p_context->CC, MC); \
		SHABAL_PERM_ELT(p_context->A9, p_context->A8, p_context->BD, p_context->BA, p_context->B6, p_context->B3, p_context->CB, MD); \
		SHABAL_PERM_ELT(p_context->AA, p_context->A9, p_context->BE, p_context->BB, p_context->B7, p_context->B4, p_context->CA, ME); \
		SHABAL_PERM_ELT(p_context->AB, p_context->AA, p_context->BF, p_context->BC, p_context->B8, p_context->B5, p_context->C9, MF); \
	} while(0)

#define SHABAL_APPLY_P   do { \
		p_context->B0 = SHABAL_T32(p_context->B0 << 17) | (p_context->B0 >> 15); \
		p_context->B1 = SHABAL_T32(p_context->B1 << 17) | (p_context->B1 >> 15); \
		p_context->B2 = SHABAL_T32(p_context->B2 << 17) | (p_context->B2 >> 15); \
		p_context->B3 = SHABAL_T32(p_context->B3 << 17) | (p_context->B3 >> 15); \
		p_context->B4 = SHABAL_T32(p_context->B4 << 17) | (p_context->B4 >> 15); \
		p_context->B5 = SHABAL_T32(p_context->B5 << 17) | (p_context->B5 >> 15); \
		p_context->B6 = SHABAL_T32(p_context->B6 << 17) | (p_context->B6 >> 15); \
		p_context->B7 = SHABAL_T32(p_context->B7 << 17) | (p_context->B7 >> 15); \
		p_context->B8 = SHABAL_T32(p_context->B8 << 17) | (p_context->B8 >> 15); \
		p_context->B9 = SHABAL_T32(p_context->B9 << 17) | (p_context->B9 >> 15); \
		p_context->BA = SHABAL_T32(p_context->BA << 17) | (p_context->BA >> 15); \
		p_context->BB = SHABAL_T32(p_context->BB << 17) | (p_context->BB >> 15); \
		p_context->BC = SHABAL_T32(p_context->BC << 17) | (p_context->BC >> 15); \
		p_context->BD = SHABAL_T32(p_context->BD << 17) | (p_context->BD >> 15); \
		p_context->BE = SHABAL_T32(p_context->BE << 17) | (p_context->BE >> 15); \
		p_context->BF = SHABAL_T32(p_context->BF << 17) | (p_context->BF >> 15); \
		SHABAL_PERM_STEP_0; \
		SHABAL_PERM_STEP_1; \
		SHABAL_PERM_STEP_2; \
		p_context->AB = SHABAL_T32(p_context->AB + p_context->C6); \
		p_context->AA = SHABAL_T32(p_context->AA + p_context->C5); \
		p_context->A9 = SHABAL_T32(p_context->A9 + p_context->C4); \
		p_context->A8 = SHABAL_T32(p_context->A8 + p_context->C3); \
		p_context->A7 = SHABAL_T32(p_context->A7 + p_context->C2); \
		p_context->A6 = SHABAL_T32(p_context->A6 + p_context->C1); \
		p_context->A5 = SHABAL_T32(p_context->A5 + p_context->C0); \
		p_context->A4 = SHABAL_T32(p_context->A4 + p_context->CF); \
		p_context->A3 = SHABAL_T32(p_context->A3 + p_context->CE); \
		p_context->A2 = SHABAL_T32(p_context->A2 + p_context->CD); \
		p_context->A1 = SHABAL_T32(p_context->A1 + p_context->CC); \
		p_context->A0 = SHABAL_T32(p_context->A0 + p_context->CB); \
		p_context->AB = SHABAL_T32(p_context->AB + p_context->CA); \
		p_context->AA = SHABAL_T32(p_context->AA + p_context->C9); \
		p_context->A9 = SHABAL_T32(p_context->A9 + p_context->C8); \
		p_context->A8 = SHABAL_T32(p_context->A8 + p_context->C7); \
		p_context->A7 = SHABAL_T32(p_context->A7 + p_context->C6); \
		p_context->A6 = SHABAL_T32(p_context->A6 + p_context->C5); \
		p_context->A5 = SHABAL_T32(p_context->A5 + p_context->C4); \
		p_context->A4 = SHABAL_T32(p_context->A4 + p_context->C3); \
		p_context->A3 = SHABAL_T32(p_context->A3 + p_context->C2); \
		p_context->A2 = SHABAL_T32(p_context->A2 + p_context->C1); \
		p_context->A1 = SHABAL_T32(p_context->A1 + p_context->C0); \
		p_context->A0 = SHABAL_T32(p_context->A0 + p_context->CF); \
		p_context->AB = SHABAL_T32(p_context->AB + p_context->CE); \
		p_context->AA = SHABAL_T32(p_context->AA + p_context->CD); \
		p_context->A9 = SHABAL_T32(p_context->A9 + p_context->CC); \
		p_context->A8 = SHABAL_T32(p_context->A8 + p_context->CB); \
		p_context->A7 = SHABAL_T32(p_context->A7 + p_context->CA); \
		p_context->A6 = SHABAL_T32(p_context->A6 + p_context->C9); \
		p_context->A5 = SHABAL_T32(p_context->A5 + p_context->C8); \
		p_context->A4 = SHABAL_T32(p_context->A4 + p_context->C7); \
		p_context->A3 = SHABAL_T32(p_context->A3 + p_context->C6); \
		p_context->A2 = SHABAL_T32(p_context->A2 + p_context->C5); \
		p_context->A1 = SHABAL_T32(p_context->A1 + p_context->C4); \
		p_context->A0 = SHABAL_T32(p_context->A0 + p_context->C3); \
	} while(0)

#define SHABAL_INCR_W   do { \
		if ((p_context->Wlow = SHABAL_T32(p_context->Wlow + 1)) == 0) \
			p_context->Whigh = SHABAL_T32(p_context->Whigh + 1); \
	} while(0)

typedef struct {
	unsigned int A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, AA, AB;
	unsigned int B0, B1, B2, B3, B4, B5, B6, B7, B8, B9, BA, BB, BC, BD, BE, BF;
	unsigned int C0, C1, C2, C3, C4, C5, C6, C7, C8, C9, CA, CB, CC, CD, CE, CF;
	unsigned int Wlow, Whigh;
} shabal_context_t;

void shabal_init(shabal_context_t* p_context) {
	p_context->A0 = 0x52f84552;
	p_context->A1 = 0xe54b7999;
	p_context->A2 = 0x2d8ee3ec;
	p_context->A3 = 0xb9645191;
	p_context->A4 = 0xe0078b86;
	p_context->A5 = 0xbb7c44c9;
	p_context->A6 = 0xd2b5c1ca;
	p_context->A7 = 0xb0d2eb8c;
	p_context->A8 = 0x14ce5a45;
	p_context->A9 = 0x22af50dc;
	p_context->AA = 0xeffdbc6b;
	p_context->AB = 0xeb21b74a;

	p_context->B0 = 0xb555c6ee;
	p_context->B1 = 0x3e710596;
	p_context->B2 = 0xa72a652f;
	p_context->B3 = 0x9301515f;
	p_context->B4 = 0xda28c1fa;
	p_context->B5 = 0x696fd868;
	p_context->B6 = 0x9cb6bf72;
	p_context->B7 = 0x0afe4002;
	p_context->B8 = 0xa6e03615;
	p_context->B9 = 0x5138c1d4;
	p_context->BA = 0xbe216306;
	p_context->BB = 0xb38b8890;
	p_context->BC = 0x3ea8b96b;
	p_context->BD = 0x3299ace4;
	p_context->BE = 0x30924dd4;
	p_context->BF = 0x55cb34a5;

	p_context->C0 = 0xb405f031;
	p_context->C1 = 0xc4233eba;
	p_context->C2 = 0xb3733979;
	p_context->C3 = 0xc0dd9d55;
	p_context->C4 = 0xc51c28ae;
	p_context->C5 = 0xa327b8e1;
	p_context->C6 = 0x56c56167;
	p_context->C7 = 0xed614433;
	p_context->C8 = 0x88b59d60;
	p_context->C9 = 0x60e2ceba;
	p_context->CA = 0x758b4b8b;
	p_context->CB = 0x83e82a7f;
	p_context->CC = 0xbc968828;
	p_context->CD = 0xe6e00bf7;
	p_context->CE = 0xba839e55;
	p_context->CF = 0x9b491c60;

	p_context->Wlow = 1;
	p_context->Whigh = 0;
}

void shabal_update(shabal_context_t* p_context, __global unsigned char* p_data, unsigned int p_offset, unsigned int p_length) {
	__global unsigned int* dataView = (__global unsigned int*)p_data;
	unsigned int M0, M1, M2, M3, M4, M5, M6, M7, M8, M9, MA, MB, MC, MD, ME, MF;
	unsigned int numFullRounds = p_length >> 6;
	unsigned int numRemaining = p_length & 63;

	for (unsigned int i = 0; i < numFullRounds; ++i) {
		unsigned long base = (p_offset >> 2) + (i << 4);
		M0 = dataView[base];
		M1 = dataView[base + 1];
		M2 = dataView[base + 2];
		M3 = dataView[base + 3];
		M4 = dataView[base + 4];
		M5 = dataView[base + 5];
		M6 = dataView[base + 6];
		M7 = dataView[base + 7];
		M8 = dataView[base + 8];
		M9 = dataView[base + 9];
		MA = dataView[base + 10];
		MB = dataView[base + 11];
		MC = dataView[base + 12];
		MD = dataView[base + 13];
		ME = dataView[base + 14];
		MF = dataView[base + 15];

		SHABAL_INPUT_BLOCK_ADD;
		SHABAL_XOR_W;
		SHABAL_APPLY_P;
		SHABAL_INPUT_BLOCK_SUB;
		SHABAL_SWAP_BC;
		SHABAL_INCR_W;
	}

	if (numRemaining == 0) {
		M0 = 0x80;
		M1 = M2 = M3 = M4 = M5 = M6 = M7 = M8 = M9 = MA = MB = MC = MD = ME = MF = 0;

		SHABAL_INPUT_BLOCK_ADD;
		SHABAL_XOR_W;
		SHABAL_APPLY_P;

		for (unsigned int i = 0; i < 3; ++i) {
			SHABAL_SWAP_BC;
			SHABAL_XOR_W;
			SHABAL_APPLY_P;
		}
	}
	else if (numRemaining == 16) {
		unsigned long base = (p_offset >> 2) + (numFullRounds << 4);
		M0 = dataView[base];
		M1 = dataView[base + 1];
		M2 = dataView[base + 2];
		M3 = dataView[base + 3];
		M4 = 0x80;
		M5 = M6 = M7 = M8 = M9 = MA = MB = MC = MD = ME = MF = 0;

		SHABAL_INPUT_BLOCK_ADD;
		SHABAL_XOR_W;
		SHABAL_APPLY_P;

		for (unsigned int i = 0; i < 3; ++i) {
			SHABAL_SWAP_BC;
			SHABAL_XOR_W;
			SHABAL_APPLY_P;
		}
	}
	else {
		unsigned long base = (p_offset >> 2) + (numFullRounds << 4);
		M0 = dataView[base];
		M1 = dataView[base + 1];
		M2 = dataView[base + 2];
		M3 = dataView[base + 3];
		M4 = dataView[base + 4];
		M5 = dataView[base + 5];
		M6 = dataView[base + 6];
		M7 = dataView[base + 7];
		M8 = dataView[base + 8];
		M9 = dataView[base + 9];
		MA = dataView[base + 10];
		MB = dataView[base + 11];
		MC = 0x80;
		MD = ME = MF = 0;

		SHABAL_INPUT_BLOCK_ADD;
		SHABAL_XOR_W;
		SHABAL_APPLY_P;

		for (unsigned int i = 0; i < 3; ++i) {
			SHABAL_SWAP_BC;
			SHABAL_XOR_W;
			SHABAL_APPLY_P;
		}
	}
}

void shabal_digest(shabal_context_t* p_context, unsigned char* p_out, unsigned int p_offset) {
	unsigned int* outView = (unsigned int*)p_out;
	unsigned int base = p_offset >> 2;
	outView[base + 0] = p_context->B8;
	outView[base + 1] = p_context->B9;
	outView[base + 2] = p_context->BA;
	outView[base + 3] = p_context->BB;
	outView[base + 4] = p_context->BC;
	outView[base + 5] = p_context->BD;
	outView[base + 6] = p_context->BE;
	outView[base + 7] = p_context->BF;
}

void memcpy(unsigned char* p_from, unsigned int p_fromOffset, unsigned char* p_to, unsigned int p_toOffset, unsigned int p_length) {
	for (unsigned int i = 0; i < p_length; ++i) {
		p_to[p_toOffset + i] = p_from[p_fromOffset + i];
	}
}

void memcpyFromGlobal(__global unsigned char* p_from, unsigned int p_fromOffset, unsigned char* p_to, unsigned int p_toOffset, unsigned int p_length) {
	for (unsigned int i = 0; i < p_length; ++i) {
		p_to[p_toOffset + i] = p_from[p_fromOffset + i];
	}
}

void memcpyToGlobal(unsigned char* p_from, unsigned int p_fromOffset, __global unsigned char* p_to, unsigned int p_toOffset, unsigned int p_length) {
	for (unsigned int i = 0; i < p_length; ++i) {
		p_to[p_toOffset + i] = p_from[p_fromOffset + i];
	}
}

void memcpyGlobal(__global unsigned char* p_from, unsigned int p_fromOffset, __global unsigned char* p_to, unsigned int p_toOffset, unsigned int p_length) {
	for (unsigned int i = 0; i < p_length; ++i) {
		p_to[p_toOffset + i] = p_from[p_fromOffset + i];
	}
}

unsigned int decodeIntLE(unsigned char* p_buffer, unsigned int p_offset) {
	return
		p_buffer[p_offset] |
		(p_buffer[p_offset + 1] << 8) |
		(p_buffer[p_offset + 2] << 16) |
		(p_buffer[p_offset + 3] << 24);
}

void encodeIntLE(unsigned char* p_buffer, unsigned int p_offset, unsigned int p_value) {
	p_buffer[p_offset] = p_value & 0x0ff;
	p_buffer[p_offset + 1] = (p_value >> 8) & 0x0ff;
	p_buffer[p_offset + 2] = (p_value >> 16) & 0x0ff;
	p_buffer[p_offset + 3] = (p_value >> 24) & 0x0ff;
}

unsigned long decodeLongLE(unsigned char* p_buffer, unsigned int p_offset) {
	return
		p_buffer[p_offset] |
		((unsigned long)p_buffer[p_offset + 1] << 8) |
		((unsigned long)p_buffer[p_offset + 2] << 16) |
		((unsigned long)p_buffer[p_offset + 3] << 24) |
		((unsigned long)p_buffer[p_offset + 4] << 32) |
		((unsigned long)p_buffer[p_offset + 5] << 40) |
		((unsigned long)p_buffer[p_offset + 6] << 48) |
		((unsigned long)p_buffer[p_offset + 7] << 56);
}

void encodeLongLE(unsigned char* p_buffer, unsigned int p_offset, unsigned long p_value) {
	p_buffer[p_offset] = p_value & 0x0ff;
	p_buffer[p_offset + 1] = (p_value >> 8) & 0x0ff;
	p_buffer[p_offset + 2] = (p_value >> 16) & 0x0ff;
	p_buffer[p_offset + 3] = (p_value >> 24) & 0x0ff;
	p_buffer[p_offset + 4] = (p_value >> 32) & 0x0ff;
	p_buffer[p_offset + 5] = (p_value >> 40) & 0x0ff;
	p_buffer[p_offset + 6] = (p_value >> 48) & 0x0ff;
	p_buffer[p_offset + 7] = (p_value >> 56) & 0x0ff;
}

unsigned long decodeIntBEGlobal(__global unsigned char* p_buffer, unsigned int p_offset) {
	return
		(p_buffer[p_offset] << 24) |
		(p_buffer[p_offset + 1] << 16) |
		(p_buffer[p_offset + 2] << 8) |
		p_buffer[p_offset + 3];
}

void encodeIntBEGlobal(__global unsigned char* p_buffer, unsigned int p_offset, unsigned long p_value) {
	p_buffer[p_offset] = (p_value >> 24) & 0x0ff;
	p_buffer[p_offset + 1] = (p_value >> 16) & 0x0ff;
	p_buffer[p_offset + 2] = (p_value >> 8) & 0x0ff;
	p_buffer[p_offset + 3] = p_value & 0x0ff;
}

unsigned long decodeLongBE(unsigned char* p_buffer, unsigned int p_offset) {
	return
		((unsigned long)p_buffer[p_offset] << 56) |
		((unsigned long)p_buffer[p_offset + 1] << 48) |
		((unsigned long)p_buffer[p_offset + 2] << 40) |
		((unsigned long)p_buffer[p_offset + 3] << 32) |
		((unsigned long)p_buffer[p_offset + 4] << 24) |
		((unsigned long)p_buffer[p_offset + 5] << 16) |
		((unsigned long)p_buffer[p_offset + 6] << 8) |
		p_buffer[p_offset + 7];
}

void encodeLongBE(unsigned char* p_buffer, unsigned int p_offset, unsigned long p_value) {
	p_buffer[p_offset] = (p_value >> 56) & 0x0ff;
	p_buffer[p_offset + 1] = (p_value >> 48) & 0x0ff;
	p_buffer[p_offset + 2] = (p_value >> 40) & 0x0ff;
	p_buffer[p_offset + 3] = (p_value >> 32) & 0x0ff;
	p_buffer[p_offset + 4] = (p_value >> 24) & 0x0ff;
	p_buffer[p_offset + 5] = (p_value >> 16) & 0x0ff;
	p_buffer[p_offset + 6] = (p_value >> 8) & 0x0ff;
	p_buffer[p_offset + 7] = p_value & 0x0ff;
}

void encodeLongBEGlobal(__global unsigned char* p_buffer, unsigned int p_offset, unsigned long p_value) {
	p_buffer[p_offset] = (p_value >> 56) & 0x0ff;
	p_buffer[p_offset + 1] = (p_value >> 48) & 0x0ff;
	p_buffer[p_offset + 2] = (p_value >> 40) & 0x0ff;
	p_buffer[p_offset + 3] = (p_value >> 32) & 0x0ff;
	p_buffer[p_offset + 4] = (p_value >> 24) & 0x0ff;
	p_buffer[p_offset + 5] = (p_value >> 16) & 0x0ff;
	p_buffer[p_offset + 6] = (p_value >> 8) & 0x0ff;
	p_buffer[p_offset + 7] = p_value & 0x0ff;
}



#endif
