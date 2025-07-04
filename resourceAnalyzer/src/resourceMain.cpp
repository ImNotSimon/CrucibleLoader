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
	ts(metaOffset);
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

		writeTo.append("}\n");
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
		//std::string manifestString;
		//String_ResourceArchive(archive, manifestString);
		//std::ofstream output(manifestPath, std::ios_base::binary);
		//output << manifestString;
		//output.close();
	}
}

void ModLoader(fspath gamedir, fspath outputdir) {

}


int main(int argc, char* argv[]) {
	#ifdef DOOMETERNAL
	fspath gamedir = "D:/Steam/steamapps/common/DOOMEternal";
	fspath outputdir = "../input/eternal"
	#else
	fspath gamedir = "D:/Steam/steamapps/common/DOOMTheDarkAges";
	fspath outputdir = "../input/darkages";
	#endif

	ModDef mod;
	ModReader::ReadZipMod(mod, "../input/testzip.zip");
	//Test_DumpManifests(gamedir, outputdir);
	//Test_DumpAllHeaders();
	//Test_DumpPriorityManifest();
	//Test_DumpConsolidatedFiles(gamedir, outputdir);
	//Test_DumpCommonManifest();
}