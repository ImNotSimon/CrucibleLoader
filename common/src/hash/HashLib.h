#pragma once
#include <cstdint>
#include <string_view>

namespace HashLib {
	uint64_t FarmHash64(const char* data, size_t length);
	uint64_t FingerPrint(uint64_t hi, uint64_t lo);
	uint64_t DeclHash(std::string_view type, std::string_view name);
	uint64_t ResourceMurmurHash(std::string_view data);
}