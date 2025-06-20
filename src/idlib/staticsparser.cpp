#include "staticsparser.h"
#include "io/BinaryReader.h"
#include <cassert>
#include <iostream>
#include <iomanip>

template<typename T, typename N>
struct StaticList {
	T* data = nullptr;
	N length = 0;

	~StaticList() {
		delete[] data;
	}

	void ReserveFrom(BinaryReader& reader) {
		reader.ReadLE(length);
		delete[] data;
		if(length > 0)
			data = new T[length];
	}

	void ReadFrom(BinaryReader& reader) {
		for(N i = 0; i < length; i++)
			data[i].ReadFrom(reader);
	}

	void ReserveRead(BinaryReader& reader) {
		ReserveFrom(reader);
		ReadFrom(reader);
	}
};

struct weakString_t {
	uint32_t length = 0;
	const char* data = nullptr;

	void ReadFrom(BinaryReader& reader) {
		assert(reader.ReadLE(length));
		assert(reader.ReadBytes(data, length));
	}
};

typedef StaticList<weakString_t, uint32_t> stringList_t;

void PrintTable(stringList_t& table) {
	for(uint32_t i = 0; i < table.length; i++)
		printf("\"%.*s\"\n", table.data[i].length, table.data[i].data);
}

struct headerEntry_t {
	uint32_t u0;
	uint32_t u1;
	uint32_t u2;
	uint32_t u3;
	uint32_t u4;
	uint32_t u5;
	uint32_t u6;
	uint32_t u7;

	void ReadFrom(BinaryReader& reader) {
		reader.ReadLE(u0);
		reader.ReadLE(u1);
		reader.ReadLE(u2);
		reader.ReadLE(u3);
		reader.ReadLE(u4);
		reader.ReadLE(u5);
		reader.ReadLE(u6);
		reader.ReadLE(u7);
	}
};

struct headerChunk_t {
	StaticList<headerEntry_t, uint32_t> headerStructs;
	uint32_t always0;
	uint32_t worldEntId;
	uint32_t firstTableCount;

	void ReadFrom(BinaryReader& reader) {
		headerStructs.ReserveFrom(reader);
		reader.ReadLE(always0);
		assert(always0 == 0);
		reader.ReadLE(worldEntId);
		reader.ReadLE(firstTableCount);
		headerStructs.ReadFrom(reader);
	}
};

void Crash(BinaryReader& reader) {
	printf("%zu\n", reader.GetPosition());
	assert(0);
	//std::cout << std::hex << std::setfill('0') << std::setw(16) << (const void*)(after - before) << std::endl;
}

struct StaticsSection
{
	headerChunk_t header;
	stringList_t st0;
	stringList_t st1;

	void ReadFrom(BinaryReader& reader)
	{
		header.ReadFrom(reader);

		st0.ReserveRead(reader);
		assert(st0.length == header.firstTableCount);

		while (true)
		{
			uint32_t bytecode, extra0, extra1;

			assert(reader.ReadLE(bytecode));
			if (bytecode == 0) {
				reader.ReadLE(extra0);
				reader.ReadLE(extra1);
				assert(extra0 == 0 && extra1 == 0);
			}
			else Crash(reader);
		}

		//PrintTable(st0);
		//const char* before = reader.GetNext();
		//const char* after = reader.GetNext();
		//std::cout << std::hex << std::setfill('0') << std::setw(16) << (const void*)(after - before) << std::endl;
	}
};

void StaticsParser::Parse(BinaryReader& reader)
{
	StaticsSection section;
	section.ReadFrom(reader);
}
