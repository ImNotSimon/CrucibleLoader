#include <iostream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cassert>
#include <set>
#include "archives/ResourceStructs.h"
#include "archives/PackageMapSpec.h"
#include "staticsparser.h"
#include "io/BinaryReader.h"
#include "deserialcore.h"
#include "hash/HashLib.h"

#ifndef _DEBUG
#undef assert
#define assert(op) (op)
#endif

#define TIMESTART(ID) auto EntityProfiling_ID  = std::chrono::high_resolution_clock::now();

#define TIMESTOP(ID, msg) { \
	auto timeStop = std::chrono::high_resolution_clock::now(); \
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(timeStop - EntityProfiling_ID); \
	printf("%s: %zu", msg, duration.count());\
}

void StaticsTest() {
	BinaryOpener filedata("input/m2_hebeth.mapentities");
	BinaryReader r = filedata.ToReader();

	StaticsParser::Parse(r);
}

void AddDeclHash(const char* typestring, const char* namestring)
{
	uint64_t filehash = HashLib::DeclHash(typestring, namestring);
	std::string hashString = typestring;
	hashString.push_back('/');
	hashString.append(namestring);

	const auto& iter = deserial::declHashMap.find(filehash);
	assert(iter == deserial::declHashMap.end() || iter->second == hashString);
	deserial::declHashMap.emplace(filehash, hashString);
}

void BuildDeclHashmap(const fspath gamedir) {
	std::cout << "Building Decl Farmhash Map\n";
	const std::vector<std::string> archiveList = PackageMapSpec::GetPrioritizedArchiveList(gamedir);
	const fspath basepath = gamedir / "base";
	deserial::declHashMap.reserve(15000);

	const std::set<std::string> ValidTypes = {"mapentities", "entityDef", "logicFX", "logicLibrary", "logicClass", "logicUIWidget", "logicEntity"};

	for (const std::string& archivename : archiveList) {
		ResourceArchive r;
		Read_ResourceArchive(r, basepath / archivename, RF_SkipData);

		for (uint32_t i = 0; i < r.header.numResources; i++) {
			const ResourceEntry& e = r.entries[i];

			const char* typestring, *namestring;
			Get_EntryStrings(r, e, typestring, namestring);

			if(ValidTypes.count(typestring) == 0)
				continue;

			AddDeclHash(typestring, namestring);
			
			for (uint32_t k = 0; k < e.numDependencies; k++) {
				uint32_t depindex = r.dependencyIndex[e.depIndices + k];
				const ResourceDependency& d = r.dependencies[depindex];

				const char* deptypestring = r.stringChunk.dataBlock + r.stringChunk.offsets[d.type];
				const char* depnamestring = r.stringChunk.dataBlock + r.stringChunk.offsets[d.name];

				AddDeclHash(deptypestring, depnamestring);
			}
		}
	}

	std::cout << "Decl Hash Map Size: " << deserial::declHashMap.size() << "\n";
	//for (const auto& pair : deserial::declHashMap) {
	//	std::cout << pair.second << "\n";
	//}
}

void BuildEntityClassMap(const fspath entitydir) {
	std::cout << "Building Entity Class Map\n";

	deserial::entityclassmap.reserve(5000);

	/*
	* STEP 1: Iterate through all entityDef files and extract their class
	* information if it exists
	*/
	using namespace std::filesystem;
	for (const directory_entry& decl : recursive_directory_iterator(entitydir)) {
		if(is_directory(decl))
			continue;

		std::string declpath = decl.path().string();
		declpath = declpath.substr(entitydir.string().size() + 1); // +1 to eliminate the leading slash
		for (char& c : declpath) {
			if(c == '\\')
				c = '/';
		}

		size_t periodindex = declpath.size() - 5; // length of ".decl"
		assert(declpath[periodindex] == '.');
		
		std::string_view typeview = "entityDef";
		std::string_view nameview(declpath.data(), periodindex);
		//std::cout << nameview << "\n";
		uint64_t farmhash = HashLib::DeclHash("entityDef", nameview);

		/*
		* Perform the initial population of the entity class map
		* To do this we do a hacky read-through of the entitydef
		* to extract the inheritance hash and class hash (if it exists)
		*/
		{
			BinaryOpener open(decl.path().string());
			assert(open.Okay());

			entityclass_t classdef;
			classdef.filepath = decl.path().string();
			uint64_t temphash;
			uint32_t length;

			BinaryReader reader = open.ToReader();
			assert(reader.GoRight(5));              // Skip null byte and file length
			assert(reader.ReadLE(classdef.parent)); // Read the inherit hash
			assert(reader.GoRight(6)); // ExpandInheritance(?), 5 padding bytes
			assert(reader.ReadLE(length)); // Length of editorVars block
			assert(reader.GoRight(length)); // Skip editorVars block
			assert(reader.GoRight(5));      // Skip padding
			assert(reader.ReadLE(length)); // Read system variables block length


			if (length > 0) {
				assert(length > 14); // Ensure there's at least one property
				assert(reader.GoRight(15)); // Edit block hash + sub-length of block + 0 byte code
				assert(reader.ReadLE(temphash));

				// Farmhash of "entityClass" - *should* always be first if it exists
				// Todo: might be risky to assume it will always be first
				if (temphash == 17091029760865742588UL) { 
					assert(reader.GoRight(5)); // Leaf node byte + Length
					assert(reader.ReadLE(classdef.typehash));
				}
			}
			deserial::entityclassmap.emplace(farmhash, classdef);
			//std::cout << declpath << " " << classdef.typehash << "\n";
			//if (classdef.typehash == 0)
			//	std::cout << "MISSING\n";
		}
	}

	/*
	* STEP 2: Populate inherited typehash information
	*/
	for (auto& current : deserial::entityclassmap) {
		if(current.second.typehash != 0)
			continue;

		auto parentiter = deserial::entityclassmap.find(current.second.parent);
		while (true) {
			if(parentiter == deserial::entityclassmap.end())
				assert(0);

			if (parentiter->second.typehash == 0) {
				parentiter = deserial::entityclassmap.find(parentiter->second.parent);
			}
			else {
				current.second.typehash = parentiter->second.typehash;
				break;
			}
		}
	}

	for (auto& current : deserial::entityclassmap) {
		assert(current.second.typehash != 0);
	}
}

void DeserializeEntitydefs() {
	deserial::SetDeserialMode(DeserialMode::entitydef);
	std::string derp;

	int totaldeserialized = 0;

	while (totaldeserialized < deserial::entityclassmap.size()) {

		for (auto& entitydef : deserial::entityclassmap) {

			// Skip this entity if it's already been deserialized
			if(entitydef.second.deserialized)
				continue;

			// Do not deserialize if the parent isn't deserialized
			const auto parententity = deserial::entityclassmap.find(entitydef.second.parent);
			if (parententity != deserial::entityclassmap.end()) {
				if(!parententity->second.deserialized)
					continue;
			}


			int previousWarningCount = deserial::ds_debugWarningCount();

			BinaryOpener opener = BinaryOpener(entitydef.second.filepath);
			assert(opener.Okay());
			BinaryReader reader = opener.ToReader();
			derp.push_back('"');
			derp.append(entitydef.second.filepath);
			derp.append("\" = {");
			deserial::ds_start_entitydef(reader, derp, entitydef.first);
			derp.append("}\n");

			if (previousWarningCount != deserial::ds_debugWarningCount())
				printf("%s\n", entitydef.second.filepath.c_str());
			totaldeserialized++;
			entitydef.second.deserialized = true;
		}
	}

	std::ofstream output;
	output.open("../input/deserialoutput.txt", std::ios_base::binary);
	output << derp;
	output.close();
	deserial::ds_debugging();
}

void DeserializeMapEntities() {
	deserial::SetDeserialMode(DeserialMode::mapentities);

	const fspath dir = "D:/DA/atlan/mapentities";
	const fspath outdir = "../input/ents";

	using namespace std::filesystem;
	for (const directory_entry& entry : recursive_directory_iterator(dir)) {
		if(is_directory(entry))
			continue;

		// Finding a fake submap
		if(entry.path().string().find("styx") != -1)
			continue;

		const fspath file = entry.path();
		std::cout << file << "\n";

		BinaryOpener open(file.string());
		assert(open.Okay());
		BinaryReader reader = open.ToReader();
		std::string outtext;
		outtext.reserve(30000000);
		deserial::ds_start_mapentities(reader, outtext);

		fspath outpath = outdir / file.filename();
		outpath = outpath.replace_extension("txt");
		std::ofstream outfile(outpath, std::ios_base::binary);
		outfile << outtext;
		outfile.close();
	}

	std::cout << deserial::ds_debugWarningCount();
}

void DeserializeLogicdecls() {
	deserial::SetDeserialMode(DeserialMode::logic);

	const fspath atlandir = "D:/DA/atlan";
	const fspath classfolders[LT_MAXIMUM] = {"logicClass", "logicEntity", "logicFX", "logicLibrary", "logicUIWidget"};
	const fspath outpaths[LT_MAXIMUM] = {"../input/atlan_logicclass.txt", "../input/atlan_logicentity.txt", "../input/atlan_logicfx.txt", "../input/atlan_logiclibrary.txt", "../input/atlan_logicwidget.txt"};


	for (int lt = LT_LogicClass; lt < LT_MAXIMUM; lt++) {
		using namespace std::filesystem;
		const fspath dir = atlandir / classfolders[lt];
		assert(is_directory(dir));

		std::cout << dir << "\n";

		std::string outputText;
		outputText.reserve(1000000);

		for (const directory_entry& entry : recursive_directory_iterator(dir)) {
			if(is_directory(entry))
				continue;

			int warningCount = deserial::ds_debugWarningCount();
			outputText.push_back('"');
			outputText.append(entry.path().string());
			outputText.append("\" = {");

			std::string filepath = entry.path().string();
			BinaryOpener open(filepath);
			assert(open.Okay());
			BinaryReader reader = open.ToReader();

			deserial::ds_start_logicdecl(reader, outputText, (LogicType)lt);
			outputText.append("}\n");

			int newWarningCount = deserial::ds_debugWarningCount();
			if(newWarningCount != warningCount)
				std::cout << filepath << "\n";
		}

		std::ofstream outwriter(outpaths[lt], std::ios_base::binary);
		outwriter << outputText;
		outwriter.close();

		//const fspath outdir = "../input/";
		//outdir.
	}
}

int main() {
	BuildDeclHashmap("D:/Steam/steamapps/common/DOOMTheDarkAges");
	BuildEntityClassMap("D:/DA/atlan/entityDef");
	//DeserializeEntitydefs();
	//DeserializeLogicdecls();

	DeserializeMapEntities();

	return 0;
}