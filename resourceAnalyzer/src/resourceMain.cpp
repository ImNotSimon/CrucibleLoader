#include "io/BinaryReader.h"
#include "io/BinaryWriter.h"
#include "hash/HashLib.h"
#include "archives/ResourceStructs.h"
#include "entityslayer/Oodle.h"
#include "archives/PackageMapSpec.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <set>
#include <thread>
#include <chrono>

#ifndef _DEBUG
#undef assert
#define assert(OP) (OP)
#endif

typedef std::filesystem::path fspath;

struct ResTestParms {
	fspath gamedir;
	fspath outputdir;

	void(*runFunction)() = nullptr;
};

/*
* GETTERS / DATA MANIPULATION
*/

/*
* DIAGNOSTICS
*/

struct TypeAuditData {
	std::set<std::string> fileExtensions;
};

struct ExtensionData {
	std::unordered_map<std::string, TypeAuditData> typeData;
};

void Get_ExtensionData(const ResourceArchive& r, ExtensionData& log) {
	for (uint64_t i = 0; i < r.header.numResources; i++) {

		const ResourceEntry& e = r.entries[i];
		const char* typeString = nullptr, * nameString = nullptr;
		Get_EntryStrings(r, e, typeString, nameString);

		/*
		* Collect file extension data
		*/
		std::string_view nameView(nameString);
		size_t period = nameView.find('.');
		if (period != std::string_view::npos) {
			std::string_view extString = nameView.substr(period);
			log.typeData[typeString].fileExtensions.insert(std::string(extString));
		}
		else {
			log.typeData[typeString].fileExtensions.insert("<NO EXTENSION>");
		}
	}
}

/*
* READING FUNCTIONS
*/

/*
* STRING FUNCTIONS
*/

#define ts(NAME) writeTo.append(#NAME); writeTo.append(" = "); writeTo.append(std::to_string(h.NAME)); writeTo.append(";\n");

void String_ResourceHeader(const ResourceHeader& h, std::string& writeTo) {
	writeTo.append("header = {\n");

	writeTo.append("magic = \"");
	writeTo.push_back(h.magic[0]);
	writeTo.push_back(h.magic[1]);
	writeTo.push_back(h.magic[2]);
	writeTo.push_back(h.magic[3]);
	writeTo.append("\";\n");

	ts(version);
	ts(flags);
	ts(numSegments);
	ts(segmentSize);
	ts(metadataHash);
	ts(numResources);
	ts(numDependencies);
	ts(numDepIndices);
	ts(numStringIndices);
	ts(numSpecialHashes);
	ts(numMetaEntries);
	ts(stringTableSize);
	ts(metaEntriesSize);

	ts(stringTableOffset);
	ts(metaEntriesOffset);
	ts(resourceEntriesOffset);
	ts(resourceDepsOffset);
	ts(resourceSpecialHashOffset);

	ts(dataOffset);

	#ifdef DOOMETERNAL
	ts(unknown);
	ts(metaOffset);
	#endif

	writeTo.append("}\n");
}

void String_StringChunk(const StringChunk& s, std::string& writeTo) {
	writeTo.append("strings = {\n");

	for (uint64_t i = 0; i < s.numStrings; i++) {
		writeTo.push_back('"');
		writeTo.append(s.dataBlock + s.offsets[i]);
		writeTo.append("\"\n");
	}
	writeTo.append("}\n");
	writeTo.append("stringChunkPadding = ");
	writeTo.append(std::to_string(s.paddingCount));
	writeTo.append("\n");
}

void String_ResourceArchive(const ResourceArchive& r, std::string& writeTo) {
	String_ResourceHeader(r.header, writeTo);
	String_StringChunk(r.stringChunk, writeTo);

	writeTo.append("files = {\n");

	for (uint64_t i = 0; i < r.header.numResources; i++) {
		ResourceEntry& h = r.entries[i];

		const char *typeString = nullptr, *nameString = nullptr;
		Get_EntryStrings(r, h, typeString, nameString);

		writeTo.push_back('"');
		writeTo.append(typeString);
		writeTo.append("\" \"");
		writeTo.append(nameString);
		writeTo.append("\" {\n");

		ts(resourceTypeString);
		ts(nameString);
		ts(descString);
		ts(depIndices);
		ts(strings);
		ts(specialHashes);
		ts(metaEntries);
		ts(dataOffset);
		ts(dataSize);

		ts(uncompressedSize);
		ts(dataCheckSum);
		ts(generationTimeStamp);
		ts(defaultHash);
		ts(version);
		ts(flags);
		ts(compMode);
		ts(reserved0);
		ts(variation);
		ts(reserved2);
		ts(reservedForVariations);

		ts(numStrings);
		ts(numSources);
		ts(numDependencies);
		ts(numSpecialHashes);
		ts(numMetaEntries);

		writeTo.append("dependencies = {\n");

		uint32_t* depPtr = r.dependencyIndex + h.depIndices;
		for(uint64_t depIndex = 0; depIndex < h.numDependencies; depIndex++) {
			const ResourceDependency& d = r.dependencies[depPtr[depIndex]];

			const char* dType = r.stringChunk.dataBlock + r.stringChunk.offsets[d.type];
			const char* dName = r.stringChunk.dataBlock + r.stringChunk.offsets[d.name];

			writeTo.append("\"");
			writeTo.append(dType);
			writeTo.append("\" \"");
			writeTo.append(dName);
			writeTo.append("\" {\n");
			//writeTo.append("{\n");
			//writeTo.append("type = "); writeTo.append(std::to_string(d.type));
			//writeTo.append("\nname = "); writeTo.append(std::to_string(d.name));
			
			writeTo.append("\ndepType = "); writeTo.append(std::to_string(d.depType)); 
			writeTo.append("\ndepSubType = "); writeTo.append(std::to_string(d.depSubType));
			//writeTo.append("\ntimestampOrHash = "); writeTo.append(std::to_string(d.hashOrTimestamp));
			writeTo.append("\nfirstInt = "); writeTo.append(std::to_string(d.firstInt));
			writeTo.append("\nsecondInt = "); writeTo.append(std::to_string(d.secondInt));

			writeTo.append("\n}\n");
		}

		writeTo.append("}\n}\n");
	}


	writeTo.append("}\n");
}

/*
* Testing
*/


void Test_DumpAllHeaders(const fspath gamedir, const fspath outdir) {

	const fspath outputPath = outdir / "archiveHeaders_DarkAges.txt";
	
	std::string text;

	text.append("Headers = {\n");
	for (const auto& entry : std::filesystem::recursive_directory_iterator(gamedir)) {

		if (entry.is_directory())
			continue;
		if(entry.path().extension() != ".resources")
			continue;

		std::string filename = entry.path().filename().string();

		printf("%.*s\n", (int)filename.length(), filename.data());
		text.push_back('"');
		text.append(filename);
		text.append("\" = {");

		ResourceArchive archive;
		Read_ResourceArchive(archive, entry.path().string(), RF_HeaderOnly);
		String_ResourceHeader(archive.header, text);
		text.append("}\n");
	}
	text.append("}\n");
	std::ofstream output;
	output.open(outputPath, std::ios_base::binary);
	output << text;
	output.close();
}

void Test_DumpContainerMaskHashes(const fspath gamedir, const fspath outdir) {
	const fspath outpath = outdir / "container_mask_hashes.txt";
	std::string text;

	using namespace std::filesystem;
	for(const directory_entry& entry : recursive_directory_iterator(gamedir)) {
		if(entry.is_directory())
			continue;
		if(entry.path().extension() != ".resources")
			continue;

		std::string filename = entry.path().filename().string();

		uint64_t hash = GetContainerMaskHash(entry.path()).hash;

		text.push_back('"');
		text.append(filename);
		text.append("\" = ");
		text.append(std::to_string(hash));
		text.append("\n");
	}

	std::ofstream outwriter(outpath, std::ios_base::binary);
	outwriter << text;
	outwriter.close();
}

/*
* DUMP PRIORITY MANIFEST
*/
void Test_DumpPriorityManifest()
{
	struct PackageManifest {
		std::string name;         // Name - No slashes or extension
		std::string relativePath; // Verbatim from packagemapspec
		std::set<std::string> files;
	};

	// STL Container happy fun time
	std::vector<std::string> packages = PackageMapSpec::GetPrioritizedArchiveList("D:/steam/steamapps/common/DOOMTheDarkAges", false);
	std::unordered_map<std::string, std::vector<PackageManifest>> groupedPackages; // First in vector = higher priority
	std::vector<std::string> packageNameList;

	// Group patches by their "true" name
	for (const std::string& p : packages) {
		int lastSlash = (int)p.rfind('/'); // Will be index of final slash, or -1
		int period = (int)p.rfind('.');
		assert(period > lastSlash);

		int patchIndex = (int)p.find("_patch", lastSlash + 1);
		std::string fullName = p.substr(lastSlash + 1, period - lastSlash - 1);
		std::string patchlessName;
		if (patchIndex > -1) {
			assert(patchIndex + 7 == period); // Length of _patch#
			patchlessName = p.substr(lastSlash + 1, patchIndex - lastSlash - 1);
		}
		else {
			patchlessName = p.substr(lastSlash + 1, period - lastSlash - 1);
		}
		groupedPackages[patchlessName].push_back({fullName, p});
		packageNameList.push_back(fullName);
	}

	//for (auto& pair : groupedPackages) {
	//	std::cout << "-----\n" <<  pair.first << "\n";

	//	for (PackageManifest& manifest : pair.second) {
	//		std::cout << "   " << manifest.name << "\n";
	//	}
	//}

	const std::filesystem::path baseDir = "D:/steam/steamapps/common/DOOMTheDarkAges/base"; //wrong, we're doing it for Eternal
	int maxNameLength = 0;
	std::string longestName;
	// Now we build a manifest
	for (auto& pair : groupedPackages) 
	{
		for (int index = 0; index < pair.second.size(); index++) 
		{
			PackageManifest& manifest = pair.second[index];
			int numOverrides = 0;

			ResourceArchive archive;
			Read_ResourceArchive(archive, (baseDir / manifest.relativePath).string(), RF_SkipData);
			

			for (uint64_t entryIndex = 0; entryIndex < archive.header.numResources; entryIndex++)
			{
				ResourceEntry& e = archive.entries[entryIndex];
				const char* typeString, *nameString;
				Get_EntryStrings(archive, e, typeString, nameString);
				int nameLength = (int)strlen(nameString);
				if(nameLength > maxNameLength){
					if(strcmp(typeString, "image") == 0) {
						maxNameLength = nameLength;
						longestName = nameString;
					}

				}

				std::string setString = typeString;
				setString.push_back('/');
				setString.append(nameString);

				bool inHigherPatch = false;
				for (int i = 0; i < index; i++) {
					if (pair.second[i].files.find(setString) != pair.second[i].files.end()) {
						numOverrides++;
						inHigherPatch = true;
						break;
					}
				}

				if(inHigherPatch)
					continue;

				manifest.files.insert(setString);
			}

			std::cout << manifest.name << " " << numOverrides << "\n";
		}
	}

	std::cout << "Longest Name Length " << longestName << " " << maxNameLength << "\n";
	std::cout << "Putting it all together\n";
	std::unordered_map<std::string, std::vector<std::string>> filesToArchives; // ME DYNAMIC MEMORY MANAGEMENT SO GOOD AAAAAAGGGGGGGHHHHH

	for (auto& pair : groupedPackages) {
		for (PackageManifest& manifest : pair.second) {
			for (const std::string& s : manifest.files) {
				filesToArchives[s].push_back(manifest.name);
			}
		}
	}

	// Output the plaintext version of the manifest
	{
		std::string textForm;
		textForm.reserve(10000000);

		for (auto& pair : filesToArchives) {
			textForm.push_back('"');
			textForm.append(pair.first);
			textForm.append("\" \"");

			for (std::string& archiveName : pair.second) {
				textForm.append(archiveName);
				textForm.push_back(' ');
			}
			textForm.pop_back();
			textForm.append("\"\n");
		}

		const char* outputPath = "D:/Modding/dark ages/EntityAtlan/input/autoMappingNewReader.txt";
		std::ofstream open(outputPath, std::ios_base::binary);
		open << textForm;
		open.close();
	}

	// Output a binary version of the manifest
	std::cout << "Outputing Binary Version\n";
	{
		BinaryWriter writer(2500000, 2.0f);
		uint32_t versionNumber = 1;
		uint32_t archiveCount = (uint32_t)packageNameList.size();
		uint32_t archiveMappingCount = (uint32_t)filesToArchives.size();

		writer << versionNumber << archiveCount << archiveMappingCount;

		std::set<uint64_t> farmHashes;
		std::unordered_map<std::string, short> archiveIndexMap;
		for (size_t i = 0; i < packageNameList.size(); i++) {
			std::string& s = packageNameList[i];
			archiveIndexMap[s] = static_cast<short>(i);
			writer << static_cast<short>(s.length());
			writer.WriteBytes(s.data(), s.length());
		}

		for (auto& pair : filesToArchives) {
			uint64_t pathHash = HashLib::FarmHash64(pair.first.data(), pair.first.length());
			assert(farmHashes.find(pathHash) == farmHashes.end());
			farmHashes.insert(pathHash);

			writer << pathHash << static_cast<uint16_t>(pair.second.size());
			for (std::string& arc : pair.second) {
				auto iter = archiveIndexMap.find(arc);
				assert(iter != archiveIndexMap.end());
				writer << iter->second;
			}
			
		}

		writer.SaveTo("../input/autoMappingBinary.txt");
	}
}

/*
* DUMPS MANIFEST FILES FOR ALL RESOURCE ARCHIVES IN THE GAME
*/
void Test_DumpManifests(fspath installDir, fspath outputDir) {
	std::vector<std::string> packages = PackageMapSpec::GetPrioritizedArchiveList(installDir, false);
	ExtensionData audit;

	for (int i = 0; i < packages.size(); i++) {
		std::cout << packages[i] << "\n";
		fspath resourcePath = installDir / "base" / packages[i];
		fspath manifestPath = outputDir / "manifests" / resourcePath.stem();
		manifestPath.concat(".txt");

		ResourceArchive archive;
		Read_ResourceArchive(archive, resourcePath, RF_SkipData);

		// AUDIT ARCHIVES
		Get_ExtensionData(archive, audit);
		//Audit_ResourceArchive(archive, audit);
		std::string auditText;
		for(const auto& pair : audit.typeData) {
			auditText.append(pair.first);
			auditText.append(" = {\n");

			for(const std::string& ext : pair.second.fileExtensions) {
				auditText.push_back('"');
				auditText.append(ext);
				auditText.append("\"\n");
			}

			auditText.append("}\n");
		}
		std::ofstream auditOutput(outputDir / "auditResults.txt", std::ios_base::binary);
		auditOutput << auditText;
		auditOutput.close();

		// BUILD MANIFEST
		std::string manifestString;
		String_ResourceArchive(archive, manifestString);
		std::ofstream output(manifestPath, std::ios_base::binary);
		output << manifestString;
		output.close();
	}
}

void Test_ContainerMask() {
	std::set<uint64_t> maskhashes;

	BinaryOpener opener("../input/container.mask");
	assert(opener.Okay());
	BinaryReader r = opener.ToReader();

	uint32_t hashCount = 0;
	assert(r.ReadLE(hashCount));

	for(uint32_t i = 0; i < hashCount; i++) {
		uint64_t hash;
		uint32_t paddingCount;
		const char* padding;

		assert(r.ReadLE(hash));
		assert(hash != -1);
		assert(r.ReadLE(paddingCount));
		assert(r.ReadBytes(padding, paddingCount * sizeof(uint64_t)));
		//std::cout << r.GetRemaining() << "\n";

		maskhashes.insert(hash);
	}
	assert(r.GetRemaining() == 0);

	
	{
		using namespace std::filesystem;

		for(const directory_entry& entry : recursive_directory_iterator("D:/Steam/steamapps/common/DOOMTheDarkAges")) {
			if(entry.is_directory())
				continue;
			if(entry.path().extension() != ".resources")
				continue;
			
			uint64_t hash = GetContainerMaskHash(entry.path()).hash;
			if(maskhashes.count(hash) == 0)
				std::cout << "Missing Archive: " << entry.path().filename();
		}
	}
}

struct entryvariations_t {
	std::set<uint8_t> compmode;
	std::set<uint32_t> version;
	std::set<uint32_t> flags;
	std::set<uint16_t> variation;
	std::set<uint16_t> numDependencies;
};

void Test_AuditAllArchives(fspath installDir) {
	std::vector<std::string> packages = PackageMapSpec::GetPrioritizedArchiveList(installDir, false);

	std::unordered_map<std::string, entryvariations_t> variations;
	variations.reserve(100);

	for (int i = 0; i < packages.size(); i++) {
		std::cout << packages[i] << "\n";
		fspath resourcePath = installDir / "base" / packages[i];

		ResourceArchive archive;
		Read_ResourceArchive(archive, resourcePath, RF_SkipData);
		Audit_ResourceArchive(archive);

		for(int k = 0; k < archive.header.numResources; k++) {
			const ResourceEntry& e = archive.entries[k];

			const char* typestring, *namestring;
			Get_EntryStrings(archive, e, typestring, namestring);

			variations[std::string(typestring)];

		}
	}
}


int main(int argc, char* argv[]) {
	#ifdef DOOMETERNAL
	fspath gamedir = "D:/SteamLibrary/steamapps/common/DOOMEternal";
	fspath outputdir = "../input/eternal";
	#else
	fspath gamedir = "D:/Steam/steamapps/common/DOOMTheDarkAges";
	fspath outputdir = "../input/darkages";
	#endif

	fspath testgamedir = "../input/darkages/injectortest";

	Test_AuditAllArchives(gamedir);


	//Test_ContainerMask();
	PackageMapSpec::ToString(gamedir);
	Test_DumpContainerMaskHashes(gamedir, outputdir);
	Test_DumpManifests(gamedir, outputdir);
	Test_DumpAllHeaders(gamedir, outputdir);
	Test_DumpPriorityManifest();
}