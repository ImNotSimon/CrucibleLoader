#include "HashLib.h"

uint64_t HashLib::FingerPrint(uint64_t hi, uint64_t lo) {
	uint64_t kMul = 0x9DDFEA08EB382D69L;
	uint64_t a = (lo ^ hi) * kMul;
	a ^= a >> 47;

	uint64_t b = (hi ^ a) * kMul;
	b ^= b >> 44;
	b *= kMul;
	b ^= b >> 41;
	b *= kMul;
	return b;
}

uint64_t HashLib::DeclHash(std::string_view type, std::string_view name) {
	uint64_t lo = FarmHash64(type.data(), type.length());
	uint64_t hi = FarmHash64(name.data(), name.length());
	uint64_t v10 = FingerPrint(hi, lo);

	return v10;
}