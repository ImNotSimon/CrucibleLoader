#include "io/BinaryReader.h"
#include "io/BinaryWriter.h"
#include "hash/HashLib.h"
#include "entityslayer/EntityParser.h"
#include "ResourceStructs.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <set>

#ifndef _DEBUG
#define assert(OP) (OP)
#endif


/*
* GETTERS / DATA MANIPULATION
*/

void Get_EntryStrings(const ResourceArchive& r, const ResourceEntry& e, const char*& typeString, const char*& nameString) {
	uint64_t* stringBuffer = r.stringIndex + e.strings;

	assert(e.numStrings == 2);
	assert(e.resourceTypeString == 0);
	assert(e.nameString == 1);
	assert(e.descString == -1);

	typeString = r.stringChunk.values[*stringBuffer];
	nameString = r.stringChunk.values[*(stringBuffer + 1)];
}

void ExtractFiles(std::filesystem::path outputDir, const ResourceArchive& r) {

	int fileCount = 0;

	std::filesystem::path declDir = outputDir / "rs_streamfile";
	std::filesystem::path entityDir = outputDir / "entityDef";

	for (uint64_t i = 0; i < r.header.numResources; i++) {
		ResourceEntry& e = r.entries[i];
		const char* typeString = nullptr, *nameString = nullptr;
		Get_EntryStrings(r, e, typeString, nameString);


		// Set output path based on file type
		std::filesystem::path fileDir;
		if(strcmp(typeString, "rs_streamfile") == 0) { // decls
			
			fileDir = declDir;
		}
		else if (strcmp(typeString, "entityDef") == 0) {
			fileDir = entityDir;
		}
		else {
			continue;
		}

		std::filesystem::path archivePath(nameString);

		if (!archivePath.has_extension()) {
			archivePath.replace_extension(".decl");
		}

		if (archivePath.has_parent_path()) {
			fileDir /= archivePath.parent_path();
		}

		if (std::filesystem::create_directories(fileDir)) {
			// Errors thrown but directories are being created just fine
			// Perhaps the issue is with calling this multiple times on the same directory?
			//std::cout << "Directory Creation Failed " << fileDir << "\n";
		}

		char* dataLocation = r.bufferData + (e.dataOffset - r.header.dataOffset);

		std::ofstream outputFile(fileDir / archivePath.filename(), std::ios_base::binary);
		outputFile.write(dataLocation, e.dataSize);
		outputFile.close();
		fileCount++;
	}

	std::cout << "Dumped " << fileCount << " files from archive\n";
}

/*
* READING FUNCTIONS
*/

enum ResourceFlags {
	RF_ReadEverything = 0,
	RF_SkipData = 1 << 0,
	RF_HeaderOnly = 1 << 1
};

void Read_ResourceArchive(ResourceArchive& r, const std::string pathString, int flags) {

	// Read the Header
	std::ifstream opener(pathString, std::ios_base::binary);
	assert(opener.good());
	opener.read(reinterpret_cast<char*>(&r.header), sizeof(ResourceHeader));
	if(flags & RF_HeaderOnly)
		return;


	// Read the Resource Entries
	r.entries = new ResourceEntry[r.header.numResources];
	opener.seekg(r.header.resourceEntriesOffset);
	opener.read(reinterpret_cast<char*>(r.entries), r.header.numResources * sizeof(ResourceEntry));

	
	// Read the String Chunk
	// TODO: READ OFFSETS TO CONST CHAR** BUFFER, THEN ADD dataBlock address to them
	opener.seekg(r.header.stringTableOffset);
	opener.read(reinterpret_cast<char*>(&r.stringChunk.numStrings), sizeof(uint64_t));
	r.stringChunk.offsets = new uint64_t[r.stringChunk.numStrings];
	r.stringChunk.values = new const char*[r.stringChunk.numStrings];
	r.stringChunk.dataBlock = new char[r.header.stringTableSize - r.stringChunk.numStrings * sizeof(uint64_t) - sizeof(uint64_t)];
	opener.read( reinterpret_cast<char*>(r.stringChunk.offsets), r.stringChunk.numStrings * sizeof(uint64_t));
	opener.read( r.stringChunk.dataBlock, r.header.stringTableSize - r.stringChunk.numStrings * sizeof(uint64_t) - sizeof(uint64_t));
	for(uint64_t i = 0; i < r.stringChunk.numStrings; i++)
		r.stringChunk.values[i] = r.stringChunk.dataBlock + r.stringChunk.offsets[i];


	// Initialize Dependency Data
	r.dependencies = new ResourceDependency[r.header.numDependencies];
	r.dependencyIndex = new uint32_t[r.header.numDepIndices];
	r.stringIndex = new uint64_t[r.header.numStringIndices];

	// Read Dependency Data
	// There can be a varying number of null bytes after the final string (or none at all)
	// Hence we have to jump to the dependency offset and can't just read from where we stopped.
	opener.seekg(r.header.resourceDepsOffset);
	opener.read(reinterpret_cast<char*>(r.dependencies), r.header.numDependencies * sizeof(ResourceDependency));
	opener.read(reinterpret_cast<char*>(r.dependencyIndex), r.header.numDepIndices * sizeof(uint32_t));
	opener.read(reinterpret_cast<char*>(r.stringIndex), r.header.numStringIndices * sizeof(uint64_t));

	// TODO: must take note of IDCL size - develop assert for it
	// TODO: Account for location of data now being = file_offset - data_offset
	if(flags & RF_SkipData)
		return;

	// Determine size of data block
	opener.seekg(0, std::ios_base::beg);
	opener.seekg(0, std::ios_base::end);
	uint64_t fileLength = static_cast<uint64_t>(opener.tellg());
	opener.seekg(r.header.dataOffset);

	// Read the data block
	r.bufferData = new char[fileLength - r.header.dataOffset];
	opener.read(r.bufferData, fileLength - r.header.dataOffset);
}

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
	ts(metaSize);
	#endif

	writeTo.append("}\n");
}

void String_StringChunk(const StringChunk& s, std::string& writeTo) {
	writeTo.append("strings = {\n");

	for (uint64_t i = 0; i < s.numStrings; i++) {
		writeTo.push_back('"');
		writeTo.append(s.values[i]);
		writeTo.append("\"\n");
	}
	writeTo.append("}\n");
}

void String_ResourceArchive(const ResourceArchive& r, std::string& writeTo) {
	String_ResourceHeader(r.header, writeTo);
	String_StringChunk(r.stringChunk, writeTo);

	writeTo.append("files = {\n");

	for (uint64_t i = 0; i < r.header.numResources; i++) {
		ResourceEntry& e = r.entries[i];

		const char *typeString = nullptr, *nameString = nullptr;
		Get_EntryStrings(r, e, typeString, nameString);

		writeTo.push_back('"');
		writeTo.append(typeString);
		writeTo.append("\" \"");
		writeTo.append(nameString);
		writeTo.append("\"\n");
	}


	writeTo.append("}\n");
}

/*
* Testing
*/


void Test_DumpAllHeaders() {

	#ifdef DOOMETERNAL
	const char* dirGame = "D:/Steam/steamapps/common/DOOMEternal/";
	const char* outputPath = "../input/archiveHeaders_Eternal.txt";
	#else
	const char* dirGame = "D:/Steam/steamapps/common/DOOMTheDarkAges/";
	const char* outputPath = "../input/archiveHeaders_DarkAges.txt";
	#endif
	
	std::string text;

	text.append("Headers = {\n");
	for (const auto& entry : std::filesystem::recursive_directory_iterator(dirGame)) {

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

void Test_DumpCommonManifest() {
	const char* path = "D:/Steam/steamapps/common/DOOMTheDarkAges/base/common.resources";

	ResourceArchive archive;
	Read_ResourceArchive(archive, path, RF_SkipData);

	std::string stringForm;
	String_ResourceArchive(archive, stringForm);

	std::ofstream output("../input/common_manifest.txt", std::ios_base::binary);
	output << stringForm;
	output.close();
}



struct PriorityInfo {
	std::string name;
	int priority;
};

std::vector<PriorityInfo> Test_GetArchiveList(std::filesystem::path PathMapSpec)
{
	std::vector<PriorityInfo> packages;
	EntityParser parser(PathMapSpec.string(), ParsingMode::JSON);

	EntNode* root = parser.getRoot()->ChildAt(0);
	EntNode& files = (*root)["\"files\""];

	// Get all resource archive names and their priorities
	for (int i = 0; i < files.getChildCount(); i++) {
		EntNode& name = (*files.ChildAt(i))["\"name\""];

		assert(&name != EntNode::SEARCH_404);

		std::string_view nameString = name.getValueUQ();

		// All of this...because the C++ 17 STL doesn't have EndsWith
		std::string_view extString = ".resources";
		size_t extIndex = nameString.rfind(extString);
		if (extIndex != std::string_view::npos && extIndex + extString.length() == nameString.length()) 
		{
			//printf("%.*s\n", (int)nameString.length(), nameString.data());
			packages.push_back({std::string(nameString), i});
		}
	}

	return packages;
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
	std::vector<PriorityInfo> packages = Test_GetArchiveList("D:/steam/steamapps/common/DOOMTheDarkAges/base/packagemapspec.json");
	std::unordered_map<std::string, std::vector<PackageManifest>> groupedPackages; // First in vector = higher priority
	std::vector<std::string> packageNameList;

	// Group patches by their "true" name
	for (PriorityInfo& p : packages) {
		int lastSlash = (int)p.name.rfind('/'); // Will be index of final slash, or -1
		int period = (int)p.name.rfind('.');
		assert(period > lastSlash);

		int patchIndex = (int)p.name.find("_patch", lastSlash + 1);
		std::string fullName = p.name.substr(lastSlash + 1, period - lastSlash - 1);
		std::string patchlessName;
		if (patchIndex > -1) {
			assert(patchIndex + 7 == period); // Length of _patch#
			patchlessName = p.name.substr(lastSlash + 1, patchIndex - lastSlash - 1);
		}
		else {
			patchlessName = p.name.substr(lastSlash + 1, period - lastSlash - 1);
		}
		groupedPackages[patchlessName].push_back({fullName, p.name});
		packageNameList.push_back(fullName);
	}

	//for (auto& pair : groupedPackages) {
	//	std::cout << "-----\n" <<  pair.first << "\n";

	//	for (PackageManifest& manifest : pair.second) {
	//		std::cout << "   " << manifest.name << "\n";
	//	}
	//}

	const std::filesystem::path baseDir = "D:/steam/steamapps/common/DOOMTheDarkAges/base";
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
				int nameLength = strlen(nameString);
				if(nameLength > maxNameLength){
					if(strcmp(typeString, "rs_streamfile") == 0) {
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
* CONSOLIDATED RESOURCE EXTRACTOR FUNCTION
*/
void Test_DumpConsolidatedFiles(const char* installFolder, const char* outputFolder) {
	std::cout << "Dark Ages consolidated resource extractor by FlavorfulGecko5\n";

	// Setup Paths
	std::filesystem::path PathOutput = outputFolder;
	PathOutput /= "geckoExporter";
	std::filesystem::path PathBase = installFolder;
	PathBase /= "base";
	std::filesystem::path PathMapSpec = PathBase / "packagemapspec.json";

	if (!std::filesystem::exists(PathMapSpec)) {
		std::cout << "FATAL ERROR: Could not find PackageMapSpec.json\n";
		std::cout << "Did you enter the correct path to your Dark Ages folder?";
	}
	else {
		std::cout << "Found DOOM The Dark Ages Folder\n";
		std::cout << "Dumping data to " << std::filesystem::absolute(PathOutput) << "\n";
	}

	std::vector<PriorityInfo> packages = Test_GetArchiveList(PathMapSpec);

	for (int i = (int)packages.size() - 1; i > -1; i--) {

		//if(i <= packages.size() - 2) // Remove me later
		//	return;

		std::filesystem::path resPath = PathBase / packages[i].name;
		std::cout << "Dumping: " << resPath.string() << "\n";

		// Parse the archive
		ResourceArchive archive;
		Read_ResourceArchive(archive, resPath.string(), RF_ReadEverything);

		ExtractFiles(PathOutput, archive);
	}

}


int main(int argc, char* argv[]) {
	//printf("%d", argc);
	Test_DumpAllHeaders();
	//Test_DumpPriorityManifest();
	//Test_DumpConsolidatedFiles("D:/Steam/steamapps/common/DOOMTheDarkAges", "../input");
	//Test_DumpCommonManifest();

	//BinaryWriter writer(1000, 2.0f);

	//int i = 1;
	//char c = 'e';
	//writer.pushSizeStack();
	//writer << i << c;
	//std::string_view s = "asdf";
	//writer.WriteBytes(s.data(), s.length());
	//writer.popSizeStack();
	//writer.SaveTo("../input/BinaryWriterTest.txt");
}