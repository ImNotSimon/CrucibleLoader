#include <filesystem>
#include <sstream>
#include <iostream>
#include <chrono>
#include "idlib/reflector.h"
#include "idlib/cleaner.h"
#include "idlib/deserialcore.h"
#include <unordered_map>
#include "io/BinaryWriter.h"
#include "io/BinaryReader.h"
#include "hash/HashLib.h"

//std::unordered_map<std::string, int> testMap = {
//	{"bitches", 3}
//};
//
//template<typename T, std::unordered_map<std::string, T> mapperino>
//void TSerializeEnum(std::unord) {
//	std::cout << mapperino["bitches"] << '\n';
//
//}

//template<typename T>
//void SerializeEnum(BinaryWriter& writer, std::string_view value, const std::unordered_map<std::string, T>& map) {
//	constexpr int32_t length = sizeof(T);
//	T value = 0;
//
//	writer.WriteLE(length);
//	writer.WriteLE(value);
//}

//void testfunc(BinaryReader& reader, std::string& writeto) {
//	std::cout << "test func called";
//}
//
//enum deserialFlags {
//	deserial_staticlist = 1 << 0,
//	deserial_idlist    = 1 << 1
//};
//
//struct deserializer {
//	void (*callback)(BinaryReader& reader, std::string& writeto) = nullptr;
//	const char* name = nullptr;
//	int arraylength = 0;
//
//	void Exec(BinaryReader& reader, std::string& writeTo) {
//
//		if (arraylength > 0) {
//			// Call static array function
//		}
//		writeTo.append(name);
//		writeTo.append(" = ");
//		callback(reader, writeTo);
//		writeTo.append(";\n");
//	}
//};
//
//struct recurs {
//	int i = 0;
//	recurs* derp = nullptr;
//};

#define TIMESTART(ID) auto EntityProfiling_ID  = std::chrono::high_resolution_clock::now();

#define TIMESTOP(ID, msg) { \
	auto timeStop = std::chrono::high_resolution_clock::now(); \
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(timeStop - EntityProfiling_ID); \
	printf("%s: %zu", msg, duration.count());\
}

void deserialTest() {
	const char* dir = "D:/Modding/dark ages/decls/entitydef/";
	
	int i = 0;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
		//std::cout << entry.path() << '\n';
		if(++i % 100 == 0)
			printf("%d\n", i);

		if(entry.is_directory())
			continue;

		BinaryReader reader = BinaryReader(entry.path().string());
		std::string derp;
		deserial::ds_start_entitydef(reader, derp);
	}
}

void HashTests() {
	//uint64_t hashTest = HashLib::FarmHash64("test", 4);
	//uint64_t result = 0x7717383daa85b5b2L;
	//printf("%d\n", hashTest == result);

	//std::string type = "ability_Dash";
	//std::string name = "ability_dash";
	//uint64_t v10 = HashLib::DeclHash(type, name);

	//std::cout << std::hex << std::setfill('0') << std::setw(16) << v10 << std::endl;
}

void GenerateIdlib() {
	//idlibCleaning::Pass1();
	idlibCleaning::Pass2();
	idlibReflection::Generate();
}

int main() {

	//deserialTest();
	//GenerateIdlib();

	//recurs s = {2, new recurs{34, new recurs}}
	//TSerializeEnum<int, testMap>();



	//const char* derp = "asdf";
	//BinaryReader reader = BinaryReader((char*)derp, 1);
	//std::string chungus;

	//deserializer t;
	//t.callback = &testfunc;
	//t.callback(reader, chungus);



	// OLD

	//float f = 234.34236435234;
	//std::ostringstream test;

	//test << f;
	//std::cout << test.str() << '\n';
	//std::cout << f << '\n' << std::to_string(f);
}