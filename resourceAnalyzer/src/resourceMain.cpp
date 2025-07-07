#include "io/BinaryReader.h"
#include "io/BinaryWriter.h"
#include "hash/HashLib.h"
#include "entityslayer/EntityParser.h"
#include "ResourceStructs.h"
#include "ModReader.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <set>

#ifndef _DEBUG
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

void Get_EntryStrings(const ResourceArchive& r, const ResourceEntry& e, const char*& typeString, const char*& nameString) {
	uint64_t* stringBuffer = r.stringIndex + e.strings;

	assert(e.numStrings == 2);
	assert(e.resourceTypeString == 0);
	assert(e.nameString == 1);
	assert(e.descString == -1);

	uint64_t typeOffset = r.stringChunk.offsets[*stringBuffer];
	uint64_t nameOffset = r.stringChunk.offsets[*(stringBuffer + 1)];
	typeString = r.stringChunk.dataBlock + typeOffset;
	nameString = r.stringChunk.dataBlock + nameOffset;

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
* DIAGNOSTICS
*/

void Audit_ResourceHeader(const ResourceHeader& h)
{
	assert(h.magic[0] == 'I' && h.magic[1] == 'D' && h.magic[2] == 'C' && h.magic[3] == 'L');
	
	#ifdef DOOMETERNAL
	assert(h.version == 12);
	assert(h.unknown == 0);
	assert(h.metaOffset == h.resourceDepsOffset + h.numDependencies * sizeof(ResourceDependency) + h.numDepIndices * sizeof(uint32_t) + h.numStringIndices * sizeof(uint64_t));
	#else
	assert(h.version == 13);
	#endif

	// Constants
	assert(h.flags == 0);
	assert(h.numSegments == 1);
	assert(h.segmentSize == 1099511627775UL);
	assert(h.metadataHash == 0);
	assert(h.numSpecialHashes == 0);
	assert(h.numMetaEntries == 0);
	assert(h.metaEntriesSize == 0);
	
	// Size Arithmetic
	assert(h.resourceEntriesOffset == sizeof(ResourceHeader));
	assert(h.resourceEntriesOffset + h.numResources * sizeof(ResourceEntry) == h.stringTableOffset);
	assert(h.stringTableSize % 8 == 0);
	assert(h.stringTableOffset + h.stringTableSize == h.resourceDepsOffset);
	assert(h.stringTableOffset + h.stringTableSize == h.metaEntriesOffset);
	assert(h.resourceDepsOffset + h.numDependencies * sizeof(ResourceDependency) == h.resourceSpecialHashOffset);
}

struct TypeAuditData {
	std::set<std::string> fileExtensions;
};

struct AuditData {
	std::unordered_map<std::string, TypeAuditData> typeData;
};

void Audit_ResourceArchive(const ResourceArchive& r, AuditData& log) {
	Audit_ResourceHeader(r.header);

	// Audit Entries
	for (uint64_t i = 0; i < r.header.numResources; i++) {

		const ResourceEntry& e = r.entries[i];
		const char *typeString = nullptr, *nameString = nullptr;
		Get_EntryStrings(r, e, typeString, nameString);

		assert(e.resourceTypeString == 0);
		assert(e.nameString == 1);
		assert(e.descString == -1);
		assert(e.strings == i * 2);
		assert(e.specialHashes == 0);
		assert(e.metaEntries == 0);

		if(e.compMode == 0) {
			assert(e.dataSize == e.uncompressedSize);

			// Not Guaranteed
			//assert(e.dataCheckSum == e.defaultHash);
		}
		// e.dataCheckSum, e.generationTimeStamp, e.defaultHash, e.version, e.flags, e.compMode
		assert(e.reserved0 == 0);
		// e.variation
		assert(e.reserved2 == 0);
		assert(e.reservedForVariations == 0);
		assert(e.numStrings == 2);
		assert(e.numSources == 0);
		// e.numDependencies
		assert(e.numSpecialHashes == 0);
		assert(e.numMetaEntries == 0);

		assert(e.dataOffset % 8 == 0);

		/*
		* The following may be tested by individual file type:
		* - depIndices          - IGNORE - Can be non-zero even if numDependencies == 0
		* - dataOffset          - IGNORE
		* - dataSize
		* - uncompressedSize    - CHECK if == uncompressedSize by file type
		* - dataCheckSum
		* - generationTimeStamp - IGNORE
		* - defaultHash         - CHECK if == dataCheckSum by file type
		* - version             - CHECK BY FILE TYPE
		* - flags               - CHECK BY FILE TYPE
		* - compMode            - CHECK BY FILE TYPE
		* - variation           - CHECK BY FILE TYPE
		* - numDependencies     - CHECK BY FILE TYPE
		*/

		if(strcmp(typeString, "rs_streamfile") == 0) {
			assert(e.dataSize == e.uncompressedSize);
			assert(e.dataCheckSum == e.defaultHash);
			assert(e.version == 0);
			assert(e.flags == 0);
			assert(e.compMode == 0);
			assert(e.variation == 0);
			assert(e.numDependencies == 0);
		}
		else if(strcmp(typeString, "entityDef") == 0) {
			//assert(e.dataSize != e.uncompressedSize);
			//assert(e.dataCheckSum != e.defaultHash);
			assert(e.version == 21);
			assert(e.flags == 2);
			//assert(e.compMode == 2 || e.compMode == 0);
			assert(e.variation == 70);
			//assert(e.numDependencies == 0);
		}

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

enum ResourceFlags {
	RF_ReadEverything = 0,
	RF_SkipData = 1 << 0,
	RF_HeaderOnly = 1 << 1
};

void Read_ResourceArchive(ResourceArchive& r, const fspath pathString, int flags) {

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
	opener.seekg(r.header.stringTableOffset);
	opener.read(reinterpret_cast<char*>(&r.stringChunk.numStrings), sizeof(uint64_t));
	r.stringChunk.offsets = new uint64_t[r.stringChunk.numStrings];
	r.stringChunk.dataBlock = new char[r.header.stringTableSize - r.stringChunk.numStrings * sizeof(uint64_t) - sizeof(uint64_t)];
	opener.read( reinterpret_cast<char*>(r.stringChunk.offsets), r.stringChunk.numStrings * sizeof(uint64_t));

	size_t stringBlockSize = r.header.stringTableSize - r.stringChunk.numStrings * sizeof(uint64_t) - sizeof(uint64_t);
	opener.read( r.stringChunk.dataBlock, stringBlockSize);

	// Count the number of padding bytes
	while(r.stringChunk.dataBlock[--stringBlockSize] == '\0') {
		r.stringChunk.paddingCount++;
	}
	r.stringChunk.paddingCount--; // We overcount by 1 due to the end string


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
			writeTo.append("\ntimestampOrHash = "); writeTo.append(std::to_string(d.hashOrTimestamp));

			writeTo.append("\n}\n");
		}

		writeTo.append("}\n}\n");
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

struct PriorityInfo {
	std::string name;
	int priority;
};

std::vector<PriorityInfo> Test_GetArchiveList(const fspath& installFolder)
{
	fspath pathMapSpec = installFolder / "base/packagemapspec.json";
	if(!std::filesystem::exists(pathMapSpec))
		return {};

	std::vector<PriorityInfo> packages;
	try {
		EntityParser parser(pathMapSpec.string(), ParsingMode::JSON);

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
				packages.push_back({ std::string(nameString), i });
			}
		}
	}
	catch (std::exception e) {
		return {};
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
	std::vector<PriorityInfo> packages = Test_GetArchiveList("D:/steam/steamapps/common/DOOMTheDarkAges");
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
* CONSOLIDATED RESOURCE EXTRACTOR FUNCTION
*/
void Test_DumpConsolidatedFiles(fspath installFolder, fspath outputFolder) {
	std::cout << "Dark Ages consolidated resource extractor by FlavorfulGecko5\n";

	// Setup Paths
	outputFolder /= "geckoExporter";
	fspath PathBase = installFolder / "base";
	std::vector<PriorityInfo> packages = Test_GetArchiveList(installFolder);

	if (packages.empty()) {
		std::cout << "FATAL ERROR: Could not find PackageMapSpec.json\n";
		std::cout << "Did you enter the correct path to your Dark Ages folder?";
	}
	else {
		std::cout << "Found DOOM The Dark Ages Folder\n";
		std::cout << "Dumping data to " << std::filesystem::absolute(outputFolder) << "\n";
	}
	std::filesystem::create_directories(outputFolder);

	for (int i = (int)packages.size() - 1; i > -1; i--) {

		//if(i <= packages.size() - 2) // Remove me later
		//	return;

		std::filesystem::path resPath = PathBase / packages[i].name;
		std::cout << "Dumping: " << resPath.string() << "\n";

		// Parse the archive
		ResourceArchive archive;
		Read_ResourceArchive(archive, resPath.string(), RF_ReadEverything);

		ExtractFiles(outputFolder, archive);
	}

}

/*
* DUMPS MANIFEST FILES FOR ALL RESOURCE ARCHIVES IN THE GAME
*/
void Test_DumpManifests(fspath installDir, fspath outputDir) {
	std::vector<PriorityInfo> packages = Test_GetArchiveList(installDir);
	AuditData audit;

	for (int i = 0; i < packages.size(); i++) {
		std::cout << packages[i].name << "\n";
		fspath resourcePath = installDir / "base" / packages[i].name;
		fspath manifestPath = outputDir / "manifests" / resourcePath.stem();
		manifestPath.concat(".txt");

		ResourceArchive archive;
		Read_ResourceArchive(archive, resourcePath, RF_SkipData);

		// AUDIT ARCHIVES
		Audit_ResourceArchive(archive, audit);
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

/*
* Resource Type Strings that will be placed at the start of the string table
*/
const char* StringTableStart[] = {"rs_streamfile", "entityDef"};


void BuildArchive(const std::vector<const ModFile*>& modfiles, fspath outarchivepath) {
	ResourceArchive archive;
	ResourceHeader& h = archive.header;
	
	/*
	* Header Constants
	*/
	h.magic[0] = 'I'; h.magic[1] = 'D'; h.magic[2] = 'C'; h.magic[3] = 'L';
	h.version = 13;
	h.flags = 0;
	h.numSegments = 1;
	h.segmentSize = 1099511627775UL;
	h.metadataHash = 0;
	h.numSpecialHashes = 0;
	h.numMetaEntries = 0;
	h.metaEntriesSize = 0;
	h.resourceEntriesOffset = sizeof(ResourceHeader);
	h.numResources = static_cast<uint32_t>(modfiles.size());
	

	/*
	* Build String Chunk
	*/
	{
		h.stringTableOffset = h.resourceEntriesOffset + h.numResources * sizeof(ResourceEntry);
		archive.stringChunk.numStrings = sizeof(StringTableStart) / sizeof(StringTableStart[0]) + modfiles.size();
		archive.stringChunk.offsets = new uint64_t[archive.stringChunk.numStrings];
		h.stringTableSize = sizeof(uint64_t) // String Count Variable
			+ archive.stringChunk.numStrings * sizeof(uint64_t); // Offsets Section (8 bytes per string)

		/*
		* For the sake of simplicity, we write the string blob to a resizeable array, then copy it over
		* to the final data buffer when finished
		*/
		std::string stringblob;
		stringblob.reserve(modfiles.size() * 75);
		uint64_t runningIndex = 0, runningOffset = 0;

		for(int i = 0; i < sizeof(StringTableStart) / sizeof(StringTableStart[0]); i++) {
			const char* current = StringTableStart[i];
			size_t len = strlen(current);

			archive.stringChunk.offsets[runningIndex++] = runningOffset;
			runningOffset+= len + 1; // Null char

			stringblob.append(current, len);
			stringblob.push_back('\0');
		}
		for(const ModFile* f : modfiles) {
			archive.stringChunk.offsets[runningIndex++] = runningOffset;
			runningOffset += f->assetPath.length() + 1;

			stringblob.append(f->assetPath);
			stringblob.push_back('\0');
		}
		assert(runningOffset == stringblob.length());
		assert(runningIndex == archive.stringChunk.numStrings);
		h.stringTableSize += runningOffset;
		archive.stringChunk.paddingCount = h.stringTableSize % 8;
		h.stringTableSize += 8 - archive.stringChunk.paddingCount;

		/*
		* Copy over blob
		*/
		archive.stringChunk.dataBlock = new char[stringblob.length()];
		memcpy(archive.stringChunk.dataBlock, stringblob.data(), stringblob.length());
	}


	/*
	* Build String Indices
	*/
	{
		h.numStringIndices = modfiles.size() * 2;
		archive.stringIndex = new uint64_t[h.numStringIndices];

		uint64_t* ptr = archive.stringIndex;
		for (uint64_t i = 0; i < modfiles.size(); i++) {
			const ModFile& f = *modfiles[i];
			*ptr = static_cast<uint64_t>(f.assetType);
			*(ptr + 1) = i + sizeof(StringTableStart) / sizeof(StringTableStart[0]);
			ptr += 2;
		}
	}

	/*
	* Build Dependencies
	* TODO: This section (and likely the string table) will need to be heavily revised.
	* For now it's fine, since rs_streamfiles have no dependencies. But entitydefs do
	*/

	h.resourceDepsOffset = h.stringTableOffset + h.stringTableSize;
	h.numDependencies = 0;
	h.numDepIndices = 0;
	h.metaEntriesOffset = h.resourceDepsOffset;
	h.resourceSpecialHashOffset = h.resourceDepsOffset + h.numDependencies * sizeof(ResourceDependency);

	
	/*
	* Configure IDCL Size and Data Offset
	*/
	archive.idclOffset = h.resourceDepsOffset + h.numDependencies * sizeof(ResourceDependency) 
		+ h.numDepIndices * sizeof(uint32_t) + h.numStringIndices * sizeof(uint64_t);
	archive.idclSize = 4;
	if(archive.idclOffset % 8 == 0) // Ensure the data offset has an 8 byte alignment
		archive.idclSize += 4;
	h.dataOffset = archive.idclOffset + archive.idclSize;
	assert(h.dataOffset % 8 == 0);


	/*
	* Build the resource entries
	*/
	archive.entries = new ResourceEntry[modfiles.size()];
	uint64_t runningDataOffset = h.dataOffset;
	for(size_t i = 0; i < modfiles.size(); i++) {
		ResourceEntry& e = archive.entries[i];
		const ModFile& f = *modfiles[i];

		/*
		* Set universal values
		*/
		e.resourceTypeString = 0;
		e.nameString = 1;
		e.descString = -1;
		e.strings = i * 2;
		e.specialHashes = 0;
		e.metaEntries = 0;
		e.reserved0 = 0;
		e.reserved2 = 0;
		e.reservedForVariations = 0;
		e.numStrings = 2;
		e.numSources = 0;
		e.numSpecialHashes = 0;
		e.numMetaEntries = 0;

		// TODO: Does this need to be non-zero? Probably not, but monitor
		// TODO: Padding? Probably not necessary?
		e.generationTimeStamp = 0; 

		/*
		* Set values that vary based on resource type
		*/
		if (f.assetType == ModFileType::rs_streamfile) {
			e.depIndices = 0;
			e.version = 0;
			e.flags = 0;
			e.compMode = 0; 
			e.variation = 0;
			e.numDependencies = 0;
			e.dataSize = f.dataLength;
			e.uncompressedSize = f.dataLength;
			e.dataCheckSum = HashLib::ResourceMurmurHash(std::string_view(static_cast<char*>(f.dataBuffer), f.dataLength));
			e.defaultHash = e.dataCheckSum;

			e.dataOffset = runningDataOffset;
			runningDataOffset += e.dataSize;

			// TODO: There's a fair bit of padding between each resource data block.
			// At a minimum, a data block has 8-byte alignment. It's unknown what the implications of ignoring
			// these practices are
			runningDataOffset += 8 - runningDataOffset % 8;
		}
		else {
			std::cout << "\nERROR: Unsupported resource type made it into build";
			return;
		}
	}

	AuditData dummy;
	Audit_ResourceArchive(archive, dummy);

	/*
	* Write the archive
	*/
	#define rc(PARM) reinterpret_cast<char*>(PARM)

	std::ofstream writer(outarchivepath, std::ios_base::binary);
	writer.write(rc(&archive.header), sizeof(ResourceHeader));
	writer.write(rc(archive.entries), sizeof(ResourceEntry) * h.numResources);

	// String Chunk
	uint64_t blobSize = h.stringTableSize - sizeof(uint64_t) - sizeof(uint64_t) * archive.stringChunk.numStrings - archive.stringChunk.paddingCount;
	writer.write(rc(&archive.stringChunk.numStrings), sizeof(uint64_t));
	writer.write(rc(archive.stringChunk.offsets), archive.stringChunk.numStrings * sizeof(uint64_t));
	writer.write(archive.stringChunk.dataBlock, blobSize);
	for(uint64_t i = 0; i < archive.stringChunk.paddingCount; i++)
		writer.put('\0');

	// Dependencies
	writer.write(rc(archive.dependencies), h.numDependencies * sizeof(ResourceDependency));
	writer.write(rc(archive.dependencyIndex), h.numDepIndices * sizeof(uint32_t));
	writer.write(rc(archive.stringIndex), h.numStringIndices * sizeof(uint64_t));

	// IDCL
	writer.write("IDCL", 4);
	for(int i = 0; i < archive.idclSize - 4; i++);
		writer.put('\0');

	// Write data chunks
	for (size_t i = 0; i < modfiles.size(); i++) {
		ResourceEntry& e = archive.entries[i];
		const ModFile& f = *modfiles[i];

		writer.seekp(e.dataOffset, std::ios_base::beg);
		writer.write(rc(f.dataBuffer), f.dataLength);
	}
	writer.close();

}

void StringPackageMapSpec(const fspath pmspath) {
	EntityParser entparser(pmspath.string(), ParsingMode::JSON);
	EntNode& jsonroot = *entparser.getRoot()->ChildAt(0);
	
	std::vector<std::string_view> filenames;
	EntNode& filelist = jsonroot["\"files\""];
	for(int i = 0; i < filelist.getChildCount(); i++) {
		filenames.push_back(filelist.ChildAt(i)->ChildAt(0)->getValueUQ());
		//std::cout << filenames.back() << "\n";
	}

	std::vector<std::string_view> mapnames;
	EntNode& maplist = jsonroot["\"maps\""];
	for(int i = 0; i < maplist.getChildCount(); i++) {
		mapnames.push_back(maplist.ChildAt(i)->ChildAt(0)->getValueUQ());
	}

	std::vector<std::vector<int>> filemapping;
	filemapping.resize(mapnames.size());

	EntNode& mapfilerefs = jsonroot["\"mapFileRefs\""];
	for(int i = 0; i < mapfilerefs.getChildCount(); i++) {
		int fileindex = -1, mapindex = -1;
		mapfilerefs.ChildAt(i)->ChildAt(0)->ValueInt(fileindex, -9999, 9999);
		mapfilerefs.ChildAt(i)->ChildAt(1)->ValueInt(mapindex, -9999, 9999);

		assert(fileindex != -1);
		assert(mapindex != -1);

		filemapping[mapindex].push_back(fileindex);
	}


	for(size_t i = 0; i < filemapping.size(); i++) {
		std::cout << "\n" << mapnames[i] << "\n";

		for(int fileindex : filemapping[i]) {
			std::cout << "-" << filenames[fileindex] << "\n";
		}

	}
}

// TODO: Will need to revisit this upon adding more advanced features
// and allow for packagemapspec manipulation
void ModPackageMapSpec(const fspath pmspath, const fspath newarchivepath)
{
	EntityParser entparser(pmspath.string(), ParsingMode::JSON);
	EntNode& jsonroot = *entparser.getRoot()->ChildAt(0);

	// Get the relative path appropriate for the packagemapspec
	size_t substringIndex = pmspath.parent_path().string().size() + 1;
	std::string archrelativepath = newarchivepath.string().substr(substringIndex);
	for(char& c : archrelativepath) {
		if(c == '\\') c = '/';
	}

	//std::cout << archrelativepath;

	// Get relevant nodes
	EntNode& filelist = jsonroot["\"files\""];
	EntNode& mapfilerefs = jsonroot["\"mapFileRefs\""];

	// Insert the file name into the packagemapspec
	char buffer[512];
	snprintf(buffer, 512, R"({ "name": "%s" })", archrelativepath.c_str());
	entparser.EditTree(buffer, &filelist, filelist.getChildCount(), 0, 0, 0);

	snprintf(buffer, 512, R"({ "file": %s, "map": 0 })", std::to_string(filelist.getChildCount() - 1).c_str());
	entparser.EditTree(buffer, &mapfilerefs, 0, 0, 0, 0);

	entparser.WriteToFile(pmspath.string(), 0);
}

void Test_Injector(const fspath gamedir) {
	#define INJECTOR_VERSION 1

	fspath modsdir = gamedir / "mods";
	fspath loosezippath = modsdir / "TEMPORARY_unzipped_modfiles.zip";
	fspath basedir = gamedir / "base";
	fspath outdir = basedir / "modarchives";
	fspath outarchivepath = outdir / "common_mod.resources";
	fspath pmspath = basedir / "packagemapspec.json";
	fspath pmsbackup = pmspath;
	pmsbackup += ".backup";

	std::vector<fspath> loosemodpaths;
	std::vector<fspath> zipmodpaths;
	loosemodpaths.reserve(20);
	zipmodpaths.reserve(20);


	/*
	* CREATE / RESTORE BACKUPS; CLEANUP PREVIOUS INJECTION FILES
	*/
	{
		using namespace std::filesystem;

		std::error_code lastCode;
		std::cout << "Managing backups and cleaning up previous injection files.\n";

		// Ensure packagemapspec.json exists
		if (!exists(pmspath)) {
			std::cout << "ERROR: Could not find " << absolute(pmspath) << "\n";
			return;
		}

		// Backup packagemapspec.json - or restore it if backup already exists
		if(!exists(pmsbackup)) {
			copy_file(pmspath, pmsbackup, copy_options::none, lastCode);
		}
		else {
			copy_file(pmsbackup, pmspath, copy_options::overwrite_existing, lastCode);
		}

		// Create input/output directories if they don't exist yet
		if (!exists(modsdir))
			create_directory(modsdir, lastCode);
		if(!exists(outdir))
			create_directory(outdir, lastCode);

		// Delete archives created from previous injections
		std::vector<fspath> filesToDelete;
		filesToDelete.reserve(10);
		for (const directory_entry& dirEntry : directory_iterator(outdir)) {
			if(dirEntry.is_directory())
				continue;

			if(dirEntry.path().extension() == ".resources") {
				filesToDelete.push_back(dirEntry.path());
			}
		}
		for (const fspath& fp : filesToDelete) {
			remove(fp, lastCode);
		}

		// Ensure the temporary loose mod zip doesn't exist
		if (exists(loosezippath)) {
			remove(loosezippath, lastCode);
		}
	}

	/*
	* GATHER MOD FILE PATHS
	*/
	{
		using namespace std::filesystem;

		// Build list of zip mod files
		for (const directory_entry& dirEntry : directory_iterator(modsdir)) {
			if(dirEntry.is_directory())
				continue;

			if(dirEntry.path().extension() == ".zip")
				zipmodpaths.push_back(dirEntry.path());
		}

		// Build list of loose mod files
		for (const directory_entry& dirEntry : recursive_directory_iterator(modsdir)) {
			if (dirEntry.is_directory())
				continue;

			if (dirEntry.path().extension() != ".zip")
				loosemodpaths.push_back(dirEntry.path());
		}

		std::cout << "\nZipped Paths\n";
		for(const fspath& f : zipmodpaths)
			std::cout << f << "\n";
		std::cout << "\nLoose Paths\n";
		for(const fspath& f : loosemodpaths)
			std::cout << f << "\n";
	}
	

	/*
	* READ MOD DATA
	*/

	std::cout << "\n\nReading Mods:\n";

	int totalmods = zipmodpaths.size() + 1; // + 1 for the loose mod
	ModDef* realmods = new ModDef[totalmods];

	realmods[0].loadPriority = -999;
	ModReader::ReadLooseMod(realmods[0], loosezippath, loosemodpaths, INJECTOR_VERSION);
	for (int i = 1; i < totalmods; i++) {
		ModReader::ReadZipMod(realmods[i], zipmodpaths[i - 1], INJECTOR_VERSION);
	}

	/*
	* BUILD THE SUPER MOD - This is a consolidated "mod" containing the highest
	* priority version of every mod file. All mod file conflicts will be resolved here
	* (conflicts from different mods, or from the same mods - in the case of bad aliasing setups)
	*/
	std::cout << "\n\nChecking for Conflicts:\n";

	std::vector<const ModFile*> supermod;
	{
		// TODO: May need to make this map<ResourceType, map<string, ModFile*>> when
		// we support more than just rs_streamfiles
		std::unordered_map<std::string, const ModFile*> priorityAssets;

		for(int i = 0; i < totalmods; i++) {
			const ModDef& current = realmods[i];

			for(const ModFile& file : current.modFiles) {

				auto iter = priorityAssets.find(file.assetPath);
				if(iter == priorityAssets.end()) {
					priorityAssets.emplace(file.assetPath, &file);
				} 
				else {
					bool replaceMapping = current.loadPriority < iter->second->parentMod->loadPriority;

					std::cout << "CONFLICT FOUND: " << file.assetPath
						<< "\n(A):" << current.modName << " - " << file.realPath
						<< "\n(B):" << iter->second->parentMod->modName << " - " << iter->second->realPath
						<< "\nWinner: " << (replaceMapping ? "(A)\n" : "(B)\n");

					if(replaceMapping) {
						iter->second = &file;
					}
				}
			}
		}

		supermod.reserve(priorityAssets.size());
		for (const auto& pair : priorityAssets) {
			supermod.push_back(pair.second);
		}
	}

	/*
	* BUILD THE RESOURCE ARCHIVES
	*/

	if (supermod.size() > 0) {
		BuildArchive(supermod, outarchivepath);
		ModPackageMapSpec(pmspath, outarchivepath);
	}
	else {
		std::cout << "\n\nNo mods will be loaded. All previously loaded mods are removed.\n";
	}

	/*
	* DEALLOCATE EVERYTHING
	*/
	for(int i = 0; i < totalmods; i++) {
		ModDef_Free(realmods[i]);
	}
	delete[] realmods;
}


int main(int argc, char* argv[]) {
	#ifdef DOOMETERNAL
	fspath gamedir = "D:/Steam/steamapps/common/DOOMEternal";
	fspath outputdir = "../input/eternal";
	#else
	fspath gamedir = "D:/Steam/steamapps/common/DOOMTheDarkAges";
	fspath outputdir = "../input/darkages";
	#endif

	fspath modspath = gamedir / "mods";
	fspath basepath = gamedir / "base";

	//ModDef mod;
	//ModReader::ReadZipMod(mod, "../input/ziptest/testzip.zip");
	//BuildArchive(mod, gamedir, outputdir, "testarchive.resources");
	//ModDef_Free(mod);


	Test_Injector("../input/darkages/injectortest");
	//Test_DumpManifests(gamedir, outputdir);
	//Test_DumpAllHeaders();
	//Test_DumpPriorityManifest();
	//Test_DumpConsolidatedFiles(gamedir, outputdir);
	//Test_DumpCommonManifest();
}