//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "MinerShabal.hpp"
#include <Poco/ByteOrder.h>

Burst::Shabal256::Shabal256()
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
