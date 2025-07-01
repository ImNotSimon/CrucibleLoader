#include "io/BinaryReader.h"
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

void ExtractFiles(std::filesystem::path outputDir, const ResourceArchive& r, BinaryReader& reader) {

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

		assert(reader.Goto(e.dataOffset));
		assert(reader.GetRemaining() >= e.dataSize);

		std::ofstream outputFile(fileDir / archivePath.filename(), std::ios_base::binary);
		outputFile.write(reader.GetNext(), e.dataSize);
		outputFile.close();
		fileCount++;
	}

	std::cout << "Dumped " << fileCount << " files from archive\n";
}

/*
* READING FUNCTIONS
*/

void Read_ResourceHeader(ResourceHeader& h, BinaryReader& reader) {
	assert(reader.GetRemaining() >= sizeof(ResourceHeader));
	memcpy(&h, reader.GetNext(), sizeof(ResourceHeader));
	assert(reader.GoRight(sizeof(ResourceHeader)));
}

void Read_ResourceHeader_Eternal(ResourceHeader_Eternal& h, BinaryReader& reader) {
	Read_ResourceHeader(h, reader);
	assert(reader.ReadLE(h.unknown));
	assert(reader.ReadLE(h.metaSize));
}

void Read_StringChunk(StringChunk& s, BinaryReader& reader) {
	assert(reader.ReadLE(s.numStrings));
	s.offsets = new uint64_t[s.numStrings];
	s.values  = new const char* [s.numStrings];

	memcpy(s.offsets, reader.GetNext(), sizeof(uint64_t) * s.numStrings);
	assert(reader.GoRight(s.numStrings * sizeof(uint64_t)));

	BinaryReader stringReader(reader.GetNext(), reader.GetRemaining());
	for (uint64_t i = 0; i < s.numStrings; i++) {
		assert(stringReader.Goto(s.offsets[i]));
		assert(stringReader.ReadCString(s.values[i]));
	}

	uint8_t endByte;
	//assert(stringReader.ReadLE(endByte));
	//assert(endByte == 0);
	// Can be multiple null bytes at the end of the chunk (or even none)
	//assert(stringReader.ReachedEOF());
	//printf("%zu\n", stringReader.GetRemaining());
	//printf("num strings %zu\n", s.numStrings);
}

void Read_ResourceArchive(ResourceArchive& r, BinaryReader& reader) {
	Read_ResourceHeader(r.header, reader);

	// Read the String Chunk
	size_t copySize = 0;
	BinaryReader stringReader = BinaryReader(reader.GetBuffer() + r.header.stringTableOffset, r.header.stringTableSize);
	Read_StringChunk(r.stringChunk, stringReader);

	// Read the Resource Entries
	r.entries = new ResourceEntry[r.header.numResources];
	copySize = sizeof(ResourceEntry) * r.header.numResources;

	assert(reader.Goto(r.header.resourceEntriesOffset));
	memcpy(r.entries, reader.GetNext(), copySize);
	assert(reader.GoRight(copySize));

	// Initialize Dependency Data
	r.dependencies = new ResourceDependency[r.header.numDependencies];
	r.dependencyIndex = new uint32_t[r.header.numDepIndices];
	r.stringIndex = new uint64_t[r.header.numStringIndices];

	// Read Dependency Structs
	copySize = r.header.numDependencies * sizeof(ResourceDependency);
	assert(reader.Goto(r.header.resourceDepsOffset));
	memcpy(r.dependencies, reader.GetNext(), copySize);
	assert(reader.GoRight(copySize));

	// Ready Dependency Indices
	copySize = r.header.numDepIndices * sizeof(uint32_t);
	memcpy(r.dependencyIndex, reader.GetNext(), copySize);
	assert(reader.GoRight(copySize));

	// Read String Indices
	copySize = r.header.numStringIndices * sizeof(uint64_t);
	memcpy(r.stringIndex, reader.GetNext(), copySize);
	assert(reader.GoRight(copySize));
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

	writeTo.append("}\n");
}

void String_ResourceHeader_Eternal(const ResourceHeader_Eternal& h, std::string& writeTo) {
	String_ResourceHeader(h, writeTo);
	writeTo.pop_back();
	writeTo.pop_back();
	writeTo.push_back('\n');

	ts(unknown)
	ts(metaSize)
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
	const char* dirDA = "D:/Steam/steamapps/common/DOOMTheDarkAges/";
	const char* dirEternal = "D:/Steam/steamapps/common/DOOMEternal/";
	std::string text;

	text.append("DarkAges = {\n");
	for (const auto& entry : std::filesystem::recursive_directory_iterator(dirDA)) {

		if (entry.is_directory())
			continue;

		if(entry.path().extension() != ".resources")
			continue;

		std::string filename = entry.path().filename().string();

		printf("%.*s\n", (int)filename.length(), filename.data());
		text.push_back('"');
		text.append(filename);
		text.append("\" = {");


		BinaryOpener opener = BinaryOpener(entry.path().string());
		BinaryReader reader = opener.ToReader();

		ResourceHeader header;
		Read_ResourceHeader(header, reader);
		String_ResourceHeader(header, text);

		text.append("}\n");
	}

	text.append("}\nEternal = {\n");
	for (const auto& entry : std::filesystem::recursive_directory_iterator(dirEternal)) {

		if (entry.is_directory())
			continue;

		if (entry.path().extension() != ".resources")
			continue;

		std::string filename = entry.path().filename().string();

		printf("%.*s\n", (int)filename.length(), filename.data());
		text.push_back('"');
		text.append(filename);
		text.append("\" = {");


		BinaryOpener opener = BinaryOpener(entry.path().string());
		BinaryReader reader = opener.ToReader();

		ResourceHeader_Eternal header;
		Read_ResourceHeader_Eternal(header, reader);
		String_ResourceHeader_Eternal(header, text);

		text.append("}\n");
	}
	text.append("}\n");


	std::ofstream output;
	output.open("../input/archiveHeaders.txt", std::ios_base::binary);
	output << text;
	output.close();
}

void Test_DumpCommonManifest() {
	const char* path = "D:/Steam/steamapps/common/DOOMTheDarkAges/base/common.resources";
	BinaryOpener open(path);
	assert(open.Okay());
	BinaryReader reader = open.ToReader();

	ResourceArchive archive;
	Read_ResourceArchive(archive, reader);

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
	}

	//for (auto& pair : groupedPackages) {
	//	std::cout << "-----\n" <<  pair.first << "\n";

	//	for (PackageManifest& manifest : pair.second) {
	//		std::cout << "   " << manifest.name << "\n";
	//	}
	//}

	const std::filesystem::path baseDir = "D:/steam/steamapps/common/DOOMTheDarkAges/base";
	// Now we build a manifest
	for (auto& pair : groupedPackages) 
	{
		for (int index = 0; index < pair.second.size(); index++) 
		{
			PackageManifest& manifest = pair.second[index];
			int numOverrides = 0;

			BinaryOpener opener((baseDir / manifest.relativePath).string());
			assert(opener.Okay());
			BinaryReader reader = opener.ToReader();

			ResourceArchive archive;
			Read_ResourceArchive(archive, reader);
			

			for (uint64_t entryIndex = 0; entryIndex < archive.header.numResources; entryIndex++)
			{
				ResourceEntry& e = archive.entries[entryIndex];
				const char* typeString, *nameString;
				Get_EntryStrings(archive, e, typeString, nameString);

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

	std::cout << "Putting it all together\n";
	std::unordered_map<std::string, std::vector<std::string>> filesToArchives; // ME DYNAMIC MEMORY MANAGEMENT SO GOOD AAAAAAGGGGGGGHHHHH

	for (auto& pair : groupedPackages) {
		for (PackageManifest& manifest : pair.second) {
			for (const std::string& s : manifest.files) {
				filesToArchives[s].push_back(manifest.name);
			}
		}
	}

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

	const char* outputPath = "D:/Modding/dark ages/EntityAtlan/input/autoMapping.txt";
	std::ofstream open(outputPath, std::ios_base::binary);
	open << textForm;
	open.close();
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
		
		// Read the archive to memory
		BinaryOpener resourceBytes(resPath.string());
		assert(resourceBytes.Okay());
		BinaryReader resourceReader = resourceBytes.ToReader();

		// Parse the archive
		ResourceArchive archive;
		Read_ResourceArchive(archive, resourceReader);

		//
		ExtractFiles(PathOutput, archive, resourceReader);
	}

	// Group the resource archives by their patches
	//std::vector<std::vector<Package>> groupedPackages;

}


int main(int argc, char* argv[]) {
	//printf("%d", argc);
	Test_DumpPriorityManifest();
	//Test_DumpConsolidatedFiles("D:/Steam/steamapps/common/DOOMTheDarkAges", "../input");
	//Test_DumpCommonManifest();
}