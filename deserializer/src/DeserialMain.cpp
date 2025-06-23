#include <iostream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <chrono>
#include "staticsparser.h"
#include "io/BinaryReader.h"
#include "deserialcore.h"
#include "hash/HashLib.h"

#define TIMESTART(ID) auto EntityProfiling_ID  = std::chrono::high_resolution_clock::now();

#define TIMESTOP(ID, msg) { \
	auto timeStop = std::chrono::high_resolution_clock::now(); \
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(timeStop - EntityProfiling_ID); \
	printf("%s: %zu", msg, duration.count());\
}

void deserialTest() {
	
	if (0) {
		const char* entity = "D:/Modding/dark ages/decls/entitydef/ai/boss/cthulhu_mecha/flying_slam_ground_tentacles.entityDef";

		BinaryOpener opener = BinaryOpener(entity);
		BinaryReader reader = opener.ToReader();

		std::string no;
		deserial::ds_start_entitydef(reader, no);
		return;
	}


	const char* dir = "D:/Modding/dark ages/decls/entitydef/";
	std::string derp;
	int i = 0;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
		if(++i % 100 == 0)
			printf("%d\n", i);
		if(entry.is_directory())
			continue;


		std::string pathString = entry.path().string();
		int previousWarningCount = deserial::ds_debugWarningCount();

		BinaryOpener opener = BinaryOpener(entry.path().string());
		BinaryReader reader = opener.ToReader();
		derp.push_back('"');
		derp.append(entry.path().string());
		derp.append("\" = {");
		deserial::ds_start_entitydef(reader, derp);
		derp.append("}\n");

		if(previousWarningCount != deserial::ds_debugWarningCount())
			printf("%s\n", pathString.c_str());
	}
	std::ofstream output;
	output.open("../input/editorvars.txt", std::ios_base::binary);
	output << derp;
	output.close();
	deserial::ds_debugging();
}

void HashTests() {
	//uint64_t hashTest = HashLib::FarmHash64("test", 4);
	//uint64_t result = 0x7717383daa85b5b2L;
	//printf("%d\n", hashTest == result);

	//std::string type = "ability_Dash";
	//std::string name = "default";
	////uint64_t v10 = HashLib::DeclHash(type, name);

	std::string val = "logicEntityList";
	uint64_t v10 = HashLib::FarmHash64(val.data(), val.length());

	//uint32_t* ptr = reinterpret_cast<uint32_t*>(&v10);
	//uint32_t lo = ptr[0];
	//uint32_t hi = ptr[1];

	//uint32_t smol = lo ^ hi;


	std::cout << std::hex << std::setfill('0') << std::setw(16) << v10 << std::endl;
}

void StaticsTest() {
	BinaryOpener filedata("input/m2_hebeth.mapentities");
	BinaryReader r = filedata.ToReader();

	StaticsParser::Parse(r);
}

int main() {
	deserialTest();
	return 1;
}