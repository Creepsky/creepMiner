//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "MinerShabal.hpp"
#include <Poco/ByteOrder.h>

/*Burst::Shabal256::Shabal256()
{
	sph_shabal256_init(&this->context);
}

void Burst::Shabal256::update(const void* data, size_t length)
{
	sph_shabal256(&this->context, data, length);
}

void Burst::Shabal256::update(const uint64_t data)
{
	auto result = Poco::ByteOrder::flipBytes(data);
	this->update(&result, sizeof(uint64_t));
}

void Burst::Shabal256::close(void* outData)
{
	sph_shabal256_close(&this->context, outData);
}

Burst::Shabal256_AVX1::Shabal256_AVX1()
{
	avx1_mshabal_init(&context_, 256);
}

void Burst::Shabal256_AVX1::update(const void* data1, const void* data2, const void* data3, const void* data4, size_t length)
{
	avx1_mshabal(&context_, data1, data2, data3, data4, length);
}

void Burst::Shabal256_AVX1::update(const uint64_t data)
{
	auto result = Poco::ByteOrder::flipBytes(data);
	update(&result, nullptr, nullptr, nullptr, sizeof(uint64_t));
}

void Burst::Shabal256_AVX1::close(void* out1, void* out2, void* out3, void* out4)
{
	avx1_mshabal_close(&context_, 0, 0, 0, 0, 0, out1, out2, out3, out4);
}

Burst::Shabal256_AVX2::Shabal256_AVX2()
{
	avx2_mshabal_init(&context_, 256);
}

void Burst::Shabal256_AVX2::update(const void* data1, const void* data2, const void* data3, const void* data4, size_t length)
{
	avx2_mshabal(&context_, data1, data2, data3, data4, length);
}

void Burst::Shabal256_AVX2::update(const uint64_t data)
{
	auto result = Poco::ByteOrder::flipBytes(data);
	update(&result, nullptr, nullptr, nullptr, sizeof(uint64_t));
}

void Burst::Shabal256_AVX2::close(void* out1, void* out2, void* out3, void* out4)
{
	avx2_mshabal_close(&context_, 0, 0, 0, 0, 0, out1, out2, out3, out4);
}*/
