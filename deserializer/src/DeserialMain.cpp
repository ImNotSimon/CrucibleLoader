#include <iostream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cassert>
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

void deserialTest() {
	const char* dir = "../input/darkages/atlanextractor/entityDef";
	std::string derp;

	int totaldeserialized = 0;
	int i = 0;

	while (totaldeserialized < deserial::entityclassmap.size()) {
		for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
			//if(++i % 100 == 0)
			//	printf("%d\n", i);
			if (entry.is_directory())
				continue;


			std::string pathString = entry.path().string();
			uint64_t farmhash;
			{
				std::string hashstring = pathString.substr(pathString.find("entityDef") + 10); // Get past the slash
				for (char& c : hashstring) {
					if(c == '\\')
						c = '/';
				}
				
				size_t periodIndex = hashstring.find_last_of('.');
				std::string_view typeview = "entityDef";
				std::string_view nameview(hashstring.data(), periodIndex);
				//std::cout << nameview << "\n";
				farmhash = HashLib::DeclHash(typeview, nameview);
			}

			// Skip if this has already been deserialized
			auto iter_current = deserial::entityclassmap.find(farmhash);
			assert(iter_current != deserial::entityclassmap.end());
			if(iter_current->second.deserialized)
				continue;

			// Do not deserialize if the parent isn't deserialized
			auto iter_parent = deserial::entityclassmap.find(iter_current->second.parent);
			if(iter_parent != deserial::entityclassmap.end()) {
				if (!iter_parent->second.deserialized)
					continue;
			}


			int previousWarningCount = deserial::ds_debugWarningCount();

			BinaryOpener opener = BinaryOpener(entry.path().string());
			BinaryReader reader = opener.ToReader();
			derp.push_back('"');
			derp.append(entry.path().string());
			derp.append("\" = {");
			deserial::ds_start_entitydef(reader, derp, iter_current->second.typehash);
			derp.append("}\n");

			if (previousWarningCount != deserial::ds_debugWarningCount())
				printf("%s\n", pathString.c_str());
			totaldeserialized++;
			iter_current->second.deserialized = true;
		}
	}

	std::ofstream output;
	output.open("../input/deserialoutput.txt", std::ios_base::binary);
	output << derp;
	output.close();
	deserial::ds_debugging();
}

void StaticsTest() {
	BinaryOpener filedata("input/m2_hebeth.mapentities");
	BinaryReader r = filedata.ToReader();

	StaticsParser::Parse(r);
}

void BuildDeclHashmap(const fspath decldir, const char* prepend, const std::unordered_map<std::string, std::string>& deptypemap) {
	using namespace std::filesystem;

	bool isentity = strcmp(prepend, "entitydef") == 0;
	if(isentity)
		deserial::entityclassmap.reserve(5000);

	for (const directory_entry& decl : recursive_directory_iterator(decldir)) {
		if(is_directory(decl))
			continue;

		std::string declpath = decl.path().string();
		declpath = declpath.substr(decldir.string().size());
		declpath.insert(0, prepend);
		for (char& c : declpath) {
			if(c == '\\')
				c = '/';
		}

		size_t periodindex = declpath.size() - 5; // length of ".decl"
		size_t slashindex = declpath.find('/');
		assert(declpath[periodindex] == '.');
		assert(slashindex != std::string_view::npos);
		
		std::string_view typeview(declpath.data(), slashindex);
		std::string_view nameview(declpath.data() + slashindex + 1, periodindex - slashindex - 1);

		// Get the camelcased type
		const auto& iter = deptypemap.find(std::string(typeview));
		assert(iter != deptypemap.end());
		typeview = std::string_view(iter->second.data(), iter->second.size());



		uint64_t farmhash = HashLib::DeclHash(typeview, nameview);
		std::string mapstring = std::string(typeview);
		mapstring.push_back('/');
		mapstring.append(nameview);

		deserial::declHashMap.emplace(farmhash, mapstring);

		//std::cout << declpath << "\n";
		//std::cout << typeview << " " << nameview << "\n";
		//std::cout << mapstring << "\n";

		/*
		* Perform the initial population of the entity class map
		* To do this we do a hacky read-through of the entitydef
		* to extract the inheritance hash and class hash (if it exists)
		*/
		if (isentity) {
			
			BinaryOpener open(decl.path().string());
			assert(open.Okay());

			entityclass_t classdef;
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
}

void BuildDeclHashmap(const fspath gamedir) {
	std::cout << "Building Decl Farmhash Map\n";
	const std::vector<std::string> archiveList = PackageMapSpec::GetPrioritizedArchiveList(gamedir);
	const fspath basepath = gamedir / "base";

	// Map of lowercase decl folder types to their camelcased versions
	std::unordered_map<std::string, std::string> deptypemap;

	for(const std::string& archivename : archiveList) {
		ResourceArchive r;
		Read_ResourceArchive(r, basepath / archivename, RF_SkipData);

		for(uint32_t i = 0; i < r.header.numDependencies; i++) {
			const ResourceDependency& d = r.dependencies[i];

			const char* typeString = r.stringChunk.dataBlock + r.stringChunk.offsets[d.type];

			std::string lowercase = typeString;
			for(char & c : lowercase) {
				if(c <= 'Z' && c >= 'A')
					c += 32;
			}
			deptypemap.emplace(lowercase, typeString);
		}
	}

	//for(const std::string& s : deptypes)
	//	std::cout << s << "\n";

	using namespace std::filesystem;
	const fspath decldir = "../input/darkages/atlanextractor/rs_streamfile/generated/decls/";
	assert(is_directory(decldir));

	// Add all decl folders missing from the map - assuming an all-lowercase camelcasing
	for(const directory_entry& decldir : directory_iterator(decldir)) {
		std::string dirstring = decldir.path().filename().string();
		if(deptypemap.count(dirstring) == 0) {
			//std::cout << decldir.path().filename() << "\n";
			deptypemap.emplace(dirstring, dirstring);
		}	
	}

	// TODO: This would be the place to manually adjust any camelcasings if needed

	const fspath entitydir = "../input/darkages/atlanextractor/entityDef";
	const fspath logicentitydir = "../input/darkages/atlanextractor/logicEntity";
	const fspath logicfxdir = "../input/darkages/atlanextractor/logicFX";
	const fspath logicwidgetdir = "../input/darkages/atlanextractor/logicUIWidget";
	const fspath logicclassdir = "../input/darkages/atlanextractor/logicClass";
	const fspath logiclibrarydir = "../input/darkages/atlanextractor/logicLibrary";

	deserial::declHashMap.reserve(100000);
	// Get every decl hash
	BuildDeclHashmap(decldir, "", deptypemap);
	BuildDeclHashmap(entitydir, "entitydef", deptypemap);
	BuildDeclHashmap(logicentitydir, "logicentity", deptypemap);
	BuildDeclHashmap(logicfxdir, "logicfx", deptypemap);
	BuildDeclHashmap(logicwidgetdir, "logicuiwidget", deptypemap);
	BuildDeclHashmap(logicclassdir, "logicclass", deptypemap);
	BuildDeclHashmap(logiclibrarydir, "logiclibrary", deptypemap);

	std::cout << "Farmhash Map Size: " << deserial::declHashMap.size() << "\n";
}

void CompleteEntityClassMap() {
	std::cout << "Completing Entity Class Map\n";
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

int main() {
	BuildDeclHashmap("D:/Steam/steamapps/common/DOOMTheDarkAges");
	CompleteEntityClassMap();
	deserialTest();
	return 0;
}