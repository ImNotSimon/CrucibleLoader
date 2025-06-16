#include "deserialcore.h"
#include "io/BinaryReader.h"
#include <cassert>

#ifndef _DEBUG
#undef assert
#define assert(OP) (OP)
#endif

// Used for stack tracing
thread_local std::vector<std::string_view> propertyStack;

void LogWarning(std::string_view msg) {
	std::string propString;
	propString.reserve(200);
	for (std::string_view s : propertyStack) {
		propString.append(s);
		propString.push_back('/');
	}

	if(!propString.empty())
		propString.pop_back();

	printf("WARNING: %.*s %.*s", (int)propString.length(), propString.data(), (int)msg.length(), msg.data());
		
}

//void DeserialFailure(std::string writeTo, const char* error) {
//	assert(0);
//}
//
//#define checkread(VAR, MSG) { if(!reader.ReadLE(VAR)) {DeserialFailure(writeTo, MSG);}  }

void deserializer::Exec(BinaryReader& reader, std::string& writeTo) const {
	propertyStack.emplace_back(name);

	writeTo.append(name);
	writeTo.append(" = ");

	// A property hash is immediately followed by the stem/leaf byte, and it's block length
	uint8_t hasChildren;
	uint32_t length;
	assert(reader.ReadLE(hasChildren));
	assert(reader.ReadLE(length));
	assert(hasChildren == 0 || hasChildren == 1);

	// Create a new binary reader for the property to be read and skip ahead in the original
	BinaryReader propReader(reader.GetNext(), length);
	assert(reader.GoRight(length));

	if (arrayLength > 0) {
		deserial::ds_staticList(propReader, writeTo, *this);
	}
	else {
		callback(propReader, writeTo);
	}

	propertyStack.pop_back();
}

void deserial::ds_start_entitydef(BinaryReader& reader, std::string& writeTo)
{
	uint8_t bytecode;
	uint32_t filelength;
	uint64_t inherits;

	assert(reader.ReadLE(bytecode));
	assert(bytecode == 0);
	assert(reader.ReadLE(filelength));
	assert(filelength == reader.GetRemaining());
	assert(reader.ReadLE(inherits));
	
	// In map entities this byte is 0
	assert(reader.ReadLE(bytecode));
	assert(bytecode == 1);

	while (reader.GetRemaining() > 0) {
		assert(reader.ReadLE(bytecode));

		if (bytecode == 0) {
			assert(reader.ReadLE(filelength));

			if (filelength > 0) {
				assert(reader.ReadLE(bytecode));
				assert(bytecode == 1 || bytecode == 'e');
				assert(reader.GoRight(filelength - 1));
			}

			//assert(reader.GoRight(filelength));

		}
		else if (bytecode == 1) {
			assert(reader.ReadLE(filelength));
			assert(filelength == 0);

			assert(reader.ReadLE(filelength));

			if (filelength > 0) {
				assert(reader.ReadLE(bytecode));
				assert(bytecode == 0 || bytecode == 1);
				assert(reader.GoRight(filelength - 1));
			}

			//assert(reader.GoRight(filelength));
		}
		else {
			assert(0);
		}
	}
	
}

void deserial::ds_pointerbase(BinaryReader& reader, std::string& writeTo)
{
	LogWarning("Called ds_pointerbase - deserialization algorithm unknown");
	writeTo.append("\"ds_pointerbase_unknown\";\n");
}

void deserial::ds_pointerdecl(BinaryReader& reader, std::string& writeTo)
{
	assert(*(reader.GetBuffer() - 5) == 1); // Leaf node
	assert(reader.GetLength() == 8);

	uint64_t hash;
	assert(reader.ReadLE(hash));

	// Todo: Generate a map of hashes to decl filepaths that includes camelcased type
	// Query this map, append the filepath
	writeTo.push_back('"');
	if (true) { // TEMPORARY
		writeTo.append(std::to_string(hash));
	}
	else {
		char hashString[9];
		snprintf(hashString, 9, "%I64X", hash);
		std::string msg = "Unknown Decl Hash ";
		msg.append(hashString, 8);
		LogWarning(msg);
	}
	writeTo.append("\";\n");
}

void deserial::ds_enumbase(BinaryReader& reader, std::string& writeTo, const std::unordered_map<uint64_t, const char*>& enumMap)
{
	assert(*(reader.GetBuffer() - 5) == 1); // Leaf node

	size_t len = reader.GetLength();
	assert(len % 8 == 0);

	writeTo.push_back('"');
	while (len > 0) {
		uint64_t hash;
		assert(reader.ReadLE(hash));

		auto pair = enumMap.find(hash);
		if (pair != enumMap.end()) {
			writeTo.append(pair->second);
			writeTo.push_back(' ');
		}
		else {
			char hashString[9];
			snprintf(hashString, 9, "%I64X", hash);
			std::string msg = "Unknown Enum Hash ";
			msg.append(hashString, 8);
			LogWarning(msg);
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
	assert(*(reader.GetBuffer() - 5) == 0); // Stem node

	writeTo.append("{\n");
	while (reader.GetRemaining() > 0) {
		uint8_t bytecode;
		uint64_t hash;

		assert(reader.ReadLE(bytecode));
		assert(reader.ReadLE(hash));
		assert(bytecode == 0);

		auto iter = propMap.find(hash);
		if (iter != propMap.end()) {
			iter->second.Exec(reader, writeTo);
		}
		else {
			char hashString[9];
			snprintf(hashString, 9, "%I64X", hash);
			std::string msg = "Unknown Property Hash ";
			msg.append(hashString, 8);
			LogWarning(msg);
		}
	}
	writeTo.append("}\n");
}

void deserial::ds_idList(BinaryReader& reader, std::string& writeTo, void(*callback)(BinaryReader& reader, std::string& writeTo))
{
	assert(*(reader.GetBuffer() - 5) == 0);
	
	#define HASH_NUM 0x1437944E8D38F7D9UL

	uint8_t bytecode;
	uint16_t shortlength;
	uint32_t length;
	uint64_t numHash;

	// Read The element count
	assert(reader.ReadLE(bytecode));
	assert(bytecode == 0);
	assert(reader.ReadLE(numHash));
	assert(numHash == HASH_NUM);
	assert(reader.ReadLE(length));
	assert(length == 2);
	assert(reader.ReadLE(shortlength));

	writeTo.append("{\nnum = ");
	writeTo.append(std::to_string(shortlength));
	writeTo.append(";\n");

	// FORMAT of elements:
	// 4 Bytes:
	// - Always 1
	// - 2 Byte index
	// - 0 and 1 --> Denotes stem/leaf
	while (reader.GetRemaining() > 0) {
		
		assert(reader.ReadLE(bytecode));
		assert(bytecode == 1);
		assert(reader.ReadLE(shortlength));

		// At this point we're basically replicating
		// a deserializer's exec function

		std::string propName = "item[";
		propName.append(std::to_string(shortlength));
		propName.push_back(']');
		writeTo.append(propName);
		writeTo.append(" = ");
		propertyStack.emplace_back(propName);

		assert(reader.ReadLE(bytecode));
		assert(bytecode == 0 || bytecode == 1);
		assert(reader.ReadLE(length));

		BinaryReader propReader(reader.GetNext(), length);
		assert(reader.GoRight(length));

		callback(propReader, writeTo);

		propertyStack.pop_back();
	}
	writeTo.append("}\n");
}

void deserial::ds_staticList(BinaryReader& reader, std::string& writeTo, deserializer basetype)
{
	// Static lists are serialized the exact same way as idLists. They just don't have a length property
	// and elements use the property's original name
	// Todo: Possibly find a way to consolidate code across list/staticlist/deserializer functions? But probably not going to happen
	assert(*(reader.GetBuffer() - 5) == 0);

	uint8_t bytecode;
	uint16_t index;
	uint32_t length;

	writeTo.append("{\n");
	while (reader.GetRemaining() > 0) {
		assert(reader.ReadLE(bytecode));
		assert(bytecode == 1);
		assert(reader.ReadLE(index));

		// At this point we're basically replicating
		// a deserializer's exec function

		std::string propName = basetype.name;
		propName.push_back('[');
		propName.append(std::to_string(index));
		propName.push_back(']');
		writeTo.append(propName);
		writeTo.append(" = ");
		propertyStack.emplace_back(propName);

		assert(reader.ReadLE(bytecode));
		assert(bytecode == 0 || bytecode == 1);
		assert(reader.ReadLE(length));

		BinaryReader propReader(reader.GetNext(), length);
		assert(reader.GoRight(length));

		basetype.callback(propReader, writeTo);

		propertyStack.pop_back();
	}
	writeTo.append("}\n");
}

void deserial::ds_idListMap(BinaryReader& reader, std::string& writeTo, void(*keyfunc)(BinaryReader& reader, std::string& writeTo), void(*valuefunc)(BinaryReader& reader, std::string& writeTo))
{
	assert(*(reader.GetBuffer() - 5) == 0);
	writeTo.append("{\n");

	uint8_t bytecode;
	uint16_t shortcode;
	uint32_t length;

	int currentElement = 0;
	while (reader.GetRemaining() > 0) {


		// TODO: Monitor to ensure this is always 3
		assert(reader.ReadLE(shortcode));
		assert(shortcode == 3);

		/*
		* Deserialize the pair's Key
		*/
		std::string propName = "key[";
		propName.append(std::to_string(currentElement));
		propName.push_back(']');
		propertyStack.emplace_back(propName);

		assert(reader.ReadLE(bytecode));
		assert(bytecode == 0 || bytecode == 1);
		assert(reader.ReadLE(length));

		BinaryReader propReader(reader.GetNext(), length);
		assert(reader.GoRight(length));
		keyfunc(propReader, writeTo);
		propertyStack.pop_back();

		/*
		* Deserialize the pair's Value
		*/

		writeTo.append(" = ");

		propName = "value[";
		propName.append(std::to_string(currentElement));
		propName.push_back(']');
		propertyStack.emplace_back(propName);


		assert(reader.ReadLE(bytecode));
		assert(bytecode == 0 || bytecode == 1);
		assert(reader.ReadLE(length));

		propReader = BinaryReader(reader.GetNext(), length);
		assert(reader.GoRight(length));
		valuefunc(propReader, writeTo);
		propertyStack.pop_back();

		currentElement++;
	}
	writeTo.append("}\n");
}

void deserial::ds_idStr(BinaryReader& reader, std::string& writeTo)
{
	assert(*(reader.GetBuffer() - 5) == 1);

	// Todo: Monitor and figure out what the exact rules for this are
	uint8_t terminal;
	uint32_t stringLength;
	assert(reader.ReadLE(stringLength));

	writeTo.push_back('"');
	if (stringLength > 0) {
		const char* bytes = nullptr;
		assert(reader.ReadBytes(bytes, stringLength));
		writeTo.append(bytes, stringLength);

		assert(reader.ReadLE(terminal));
		assert(terminal == 0);
	}
	writeTo.append("\"\n");

	assert(reader.GetRemaining() == 0);

}

void deserial::ds_bool(BinaryReader& reader, std::string& writeTo)
{
	assert(*(reader.GetBuffer() - 5) == 1);
	assert(reader.GetLength() == 1);

	uint8_t val;
	reader.ReadLE(val);

	writeTo.append(val > 0 ? "true;\n" : "false;\n");
}

template<typename T>
__forceinline void ds_num(BinaryReader& reader, std::string& writeTo)
{
	assert(*(reader.GetBuffer() - 5) == 1); // Stem and leaf
	assert(reader.GetLength() == sizeof(T));

	T val;
	reader.ReadLE(val);

	writeTo.append(std::to_string(val)); // Should be promoted to integer when it's a character
	writeTo.append(";\n");
}

void deserial::ds_char(BinaryReader& reader, std::string& writeTo)
{
	ds_num<int8_t>(reader, writeTo);
}

void deserial::ds_unsigned_char(BinaryReader& reader, std::string& writeTo)
{
	ds_num<uint8_t>(reader, writeTo);
}

void deserial::ds_wchar_t(BinaryReader& reader, std::string& writeTo)
{
	ds_num<uint16_t>(reader, writeTo);
}

void deserial::ds_short(BinaryReader& reader, std::string& writeTo)
{
	ds_num<int16_t>(reader, writeTo);
}

void deserial::ds_unsigned_short(BinaryReader& reader, std::string& writeTo)
{
	ds_num<uint16_t>(reader, writeTo);
}

void deserial::ds_int(BinaryReader& reader, std::string& writeTo)
{
	ds_num<int32_t>(reader, writeTo);
}

void deserial::ds_unsigned_int(BinaryReader& reader, std::string& writeTo)
{
	ds_num<uint32_t>(reader, writeTo);
}

void deserial::ds_long(BinaryReader& reader, std::string& writeTo)
{
	ds_num<int32_t>(reader, writeTo);
}

void deserial::ds_long_long(BinaryReader& reader, std::string& writeTo)
{
	ds_num<int64_t>(reader, writeTo);
}

void deserial::ds_unsigned_long(BinaryReader& reader, std::string& writeTo)
{
	ds_num<uint32_t>(reader, writeTo);
}

void deserial::ds_unsigned_long_long(BinaryReader& reader, std::string& writeTo)
{
	ds_num<uint64_t>(reader, writeTo);
}

void deserial::ds_float(BinaryReader& reader, std::string& writeTo)
{
	ds_num<float>(reader, writeTo);
}

void deserial::ds_double(BinaryReader& reader, std::string& writeTo)
{
	ds_num<double>(reader, writeTo);
}
