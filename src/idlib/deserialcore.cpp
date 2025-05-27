#include "deserialcore.h"
#include "io/BinaryReader.h"
#include <cassert>

void deserial::ds_pointerbase(BinaryReader& reader, std::string& writeTo)
{
	// We assume all pointer variables are file hashes...for simplicity 
	uint8_t leaf;
	uint32_t len;
	uint64_t hash;
	reader.ReadLE(leaf);
	reader.ReadLE(len);
	reader.ReadLE(hash);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 8);
	#endif

	writeTo.push_back('"');
	writeTo.append(std::to_string(hash)); // TEMPORARY
	writeTo.append("\";\n");
}

void deserial::ds_enumbase(BinaryReader& reader, std::string& writeTo, const std::unordered_map<uint64_t, const char*>& enumMap)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len % 8 == 0);
	#endif

	writeTo.push_back('"');
	while (len > 0) {
		uint64_t hash;
		reader.ReadLE(hash);

		auto pair = enumMap.find(hash);
		if (pair != enumMap.end()) {
			writeTo.append(pair->second);
			writeTo.push_back(' ');
		}

		len -= 8;
	}
	if(writeTo.back() == ' ')
		writeTo.pop_back();
	writeTo.append("\";\n");
}

void deserial::ds_blockbase(BinaryReader& reader, std::string& writeTo, const std::unordered_map<uint64_t, deserializer>& propMap)
{

}

void deserial::ds_structbase(BinaryReader& reader, std::string& writeTo, const std::unordered_map<uint64_t, deserializer>& propMap)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 0);
	#endif

	// TODO: ACCOUNT FOR Skipping Bytes
	uint8_t nextType;
	uint64_t hash;
	while (len > 0) {
		reader.ReadLE(nextType);
		// If 0 - expect an ID. If 1 - read until we get a length
	}

}

void deserial::ds_idList(BinaryReader& reader, std::string& writeTo, void(*callback)(BinaryReader& reader, std::string writeTo))
{
}

void deserial::ds_staticList(BinaryReader& reader, std::string& writeTo, void(*callback)(BinaryReader& reader, std::string writeTo))
{
}

void deserial::ds_bool(BinaryReader& reader, std::string& writeTo)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 1);
	#endif

	uint8_t val;
	reader.ReadLE(val);

	writeTo.append(val > 0 ? "true;\n" : "false;\n");
}

void deserial::ds_char(BinaryReader& reader, std::string& writeTo)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 1);
	#endif;

	int8_t val;
	reader.ReadLE(val);

	writeTo.append(std::to_string(val)); // Should be promoted to integer
	writeTo.append(";\n");
}

void deserial::ds_unsigned_char(BinaryReader& reader, std::string& writeTo)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 1);
	#endif;

	uint8_t val;
	reader.ReadLE(val);

	writeTo.append(std::to_string(val)); // Should be promoted to integer
	writeTo.append(";\n");
}

void deserial::ds_wchar_t(BinaryReader& reader, std::string& writeTo)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 2);
	#endif;

	uint16_t val;
	reader.ReadLE(val);

	writeTo.append(std::to_string(val)); // Should be promoted to integer
	writeTo.append(";\n");
}

void deserial::ds_short(BinaryReader& reader, std::string& writeTo)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 2);
	#endif;

	int16_t val;
	reader.ReadLE(val);

	writeTo.append(std::to_string(val)); // Should be promoted to integer
	writeTo.append(";\n");
}

void deserial::ds_unsigned_short(BinaryReader& reader, std::string& writeTo)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 2);
	#endif;

	uint16_t val;
	reader.ReadLE(val);

	writeTo.append(std::to_string(val)); // Should be promoted to integer	
	writeTo.append(";\n");
}

void deserial::ds_int(BinaryReader& reader, std::string& writeTo)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 4);
	#endif;

	int32_t val;
	reader.ReadLE(val);

	writeTo.append(std::to_string(val));
	writeTo.append(";\n");
}

void deserial::ds_unsigned_int(BinaryReader& reader, std::string& writeTo)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 4);
	#endif;

	uint32_t val;
	reader.ReadLE(val);

	writeTo.append(std::to_string(val));
	writeTo.append(";\n");
}

void deserial::ds_long(BinaryReader& reader, std::string& writeTo)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 4);
	#endif;

	int32_t val;
	reader.ReadLE(val);

	writeTo.append(std::to_string(val));
	writeTo.append(";\n");
}

void deserial::ds_long_long(BinaryReader& reader, std::string& writeTo)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 8);
	#endif;

	int64_t val;
	reader.ReadLE(val);

	writeTo.append(std::to_string(val));
	writeTo.append(";\n");
}

void deserial::ds_unsigned_long(BinaryReader& reader, std::string& writeTo)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 4);
	#endif;

	uint32_t val;
	reader.ReadLE(val);

	writeTo.append(std::to_string(val));
	writeTo.append(";\n");
}

void deserial::ds_unsigned_long_long(BinaryReader& reader, std::string& writeTo)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 8);
	#endif;

	uint64_t val;
	reader.ReadLE(val);

	writeTo.append(std::to_string(val));
	writeTo.append(";\n");
}

void deserial::ds_float(BinaryReader& reader, std::string& writeTo)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 4);
	#endif;

	float val;
	reader.ReadLE(val);

	writeTo.append(std::to_string(val));
	writeTo.append(";\n");
}

void deserial::ds_double(BinaryReader& reader, std::string& writeTo)
{
	uint8_t leaf;
	uint32_t len;
	reader.ReadLE(leaf);
	reader.ReadLE(len);

	#ifdef _DEBUG
	assert(leaf == 1);
	assert(len == 8);
	#endif;

	double val;
	reader.ReadLE(val);

	writeTo.append(std::to_string(val));
	writeTo.append(";\n");
}
