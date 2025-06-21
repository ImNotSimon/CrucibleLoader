#include <filesystem>
#include <iostream>
#include <chrono>
#include "idlib/reflector.h"
#include "idlib/cleaner.h"
#include "idlib/deserialcore.h"
#include <unordered_map>
#include "io/BinaryWriter.h"
#include "io/BinaryReader.h"
#include "hash/HashLib.h"
#include "idlib/staticsparser.h"
#include <cassert>

#define TIMESTART(ID) auto EntityProfiling_ID  = std::chrono::high_resolution_clock::now();

#define TIMESTOP(ID, msg) { \
	auto timeStop = std::chrono::high_resolution_clock::now(); \
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(timeStop - EntityProfiling_ID); \
	printf("%s: %zu", msg, duration.count());\
}

// TODO:
// Manually implement idLogicProperties - it's a fake list with no count - property hash is the logicProperty_t identifier var (which is then repeated in the struct)
// 
// idTypeInfoObjectPtr - A virtual class system:
// Two properties: className - a class hash
// Value

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
		//std::cout << entry.path() << '\n';
		if(++i % 100 == 0)
			printf("%d\n", i);

		if(entry.is_directory())
			continue;

		BinaryOpener opener = BinaryOpener(entry.path().string());
		BinaryReader reader = opener.ToReader();
		derp.push_back('"');
		derp.append(entry.path().string());
		derp.append("\" = {");
		deserial::ds_start_entitydef(reader, derp);
		derp.append("}\n");
	}
	std::ofstream output;
	output.open("input/editorvars.txt", std::ios_base::binary);
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

void GenerateIdlib() {
	//idlibCleaning::Pass1();
	idlibCleaning::Pass2();
	idlibReflection::Generate();
}

void StaticsTest() {
	BinaryOpener filedata("input/m2_hebeth.mapentities");
	BinaryReader r = filedata.ToReader();

	StaticsParser::Parse(r);
}

int main() {
	//GenerateIdlib();
	deserialTest();


	//StaticsTest();
	//HashTests();
	


	// OLD

	//float f = 234.34236435234;
	//std::ostringstream test;

	//test << f;
	//std::cout << test.str() << '\n';
	//std::cout << f << '\n' << std::to_string(f);
}