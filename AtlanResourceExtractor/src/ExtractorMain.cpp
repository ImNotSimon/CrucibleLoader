#include "entityslayer/Oodle.h"
#include "archives/ResourceStructs.h"
#include "archives/PackageMapSpec.h"
#include <iostream>
#include <fstream>
#include <cassert>
#include <set>

#ifndef _DEBUG
#undef assert
#define assert(OP) (OP)
#endif

void ExtractFiles(std::filesystem::path outputDir, const ResourceArchive& r) {
	if (!Oodle::IsInitialized()) {
		std::cout << "Oodle is not initialized! Aborting file extraction\n";
		return;
	}
	int fileCount = 0;

	char* decompBlob = nullptr;
	size_t decompSize = 0;

	const std::set<std::string> validTypes = {"rs_streamfile", "entityDef", "file", "mapentities", "binarymd6def", "logicEntity" };

	for (uint64_t i = 0; i < r.header.numResources; i++) {
		ResourceEntry& e = r.entries[i];
		const char* typeString = nullptr, *nameString = nullptr;
		Get_EntryStrings(r, e, typeString, nameString);

		// Set output path based on file type
		if(validTypes.count(typeString) == 0)
			continue;
		fspath fileDir = outputDir / typeString;
		fspath assetpath(nameString);

		if (!assetpath.has_extension()) {
			if(strcmp(typeString, "mapentities") == 0)
				assetpath.replace_extension(".entities");
			else assetpath.replace_extension(".decl");
		}

		if (assetpath.has_parent_path()) {
			fileDir /= assetpath.parent_path();
		}

		// Normal for this to return false on success, for whatever reason
		std::filesystem::create_directories(fileDir);

		char* dataLocation = r.bufferData + (e.dataOffset - r.header.dataOffset);
		std::ofstream outputFile(fileDir / assetpath.filename(), std::ios_base::binary);

		switch(e.compMode) {
			case 0:
			outputFile.write(dataLocation, e.dataSize);
			break;

			case 4:
			dataLocation += 12;
			case 2:
			{
				if(decompSize < e.uncompressedSize) {
					delete[] decompBlob;
					decompBlob = new char[e.uncompressedSize];
					decompSize = e.uncompressedSize;
				}
				bool success = Oodle::DecompressBuffer(dataLocation, e.dataSize - (e.compMode == 4 ? 12 : 0), decompBlob, e.uncompressedSize);
				if(!success)
					std::cout << "Oodle Decompression Failed for " << (fileDir / assetpath.filename()) << "\n";
				outputFile.write(decompBlob, e.uncompressedSize);
			}
			break;

			default:
			assert(0);
			break;
		}


		outputFile.close();
		fileCount++;
	}

	delete[] decompBlob;
	std::cout << "Dumped " << fileCount << " files from archive\n";
}

/*
* CONSOLIDATED RESOURCE EXTRACTOR FUNCTION
*/
void ExtractorMain(int argc, char* argv[]) {
	/*
	* REMEMBER TO UPDATE VERSION NUMBER
	*/
	std::cout << "Atlan Consolidated Resource Extractor v1.1 by FlavorfulGecko5\n";

	/*
	* Get and verify command line arguments
	*/
	if (argc != 3) {
		std::cout << "AtlanResourceExtractor.exe < Game Installation Folder > < Output Folder >\n";
		return;
	}
	fspath installFolder = argv[1], outputFolder = argv[2];
	if (!std::filesystem::is_directory(installFolder)) {
		std::cout << "FATAL ERROR: " << installFolder << " is not a valid directory";
		return;
	}
	if(!std::filesystem::is_directory(outputFolder)) {
		std::cout << "FATAL ERROR: " << outputFolder << " is not a valid directory";
		return;
	}

	/*
	* Read and verify PackageMapSpec data
	*/
	outputFolder /= "atlanextractor";
	fspath PathBase = installFolder / "base";
	std::vector<std::string> packages = PackageMapSpec::GetPrioritizedArchiveList(installFolder);

	if (packages.empty()) {
		std::cout << "FATAL ERROR: Could not find PackageMapSpec.json\n";
		std::cout << "Did you enter the correct path to your Dark Ages folder?";
		return;
	}
	else {
		std::cout << "Found DOOM The Dark Ages Folder\n";
		std::cout << "Dumping data to " << std::filesystem::absolute(outputFolder) << "\n";
	}
	std::filesystem::create_directories(outputFolder);

	/*
	* Download and load Oodle
	* (Do it here so it doesn't get downloaded to the wrong folder if install folder is input wrong)
	*/
	const fspath oo2corepath = installFolder / "oo2core_9_win64.dll";
	if(!std::filesystem::exists(oo2corepath)) {
		// Because nobody ever needed a simple STL function to convert a string to a wide string....
		#define OODLE_URL    "https://github.com/WorkingRobot/OodleUE/raw/refs/heads/main/Engine/Source/Programs/Shared/EpicGames.Oodle/Sdk/2.9.10/win/redist/oo2core_9_win64.dll"
		#define OODLE_URL_W L"https://github.com/WorkingRobot/OodleUE/raw/refs/heads/main/Engine/Source/Programs/Shared/EpicGames.Oodle/Sdk/2.9.10/win/redist/oo2core_9_win64.dll"
		std::cout << "Downloading " << oo2corepath << " from " << OODLE_URL << "\n";

		bool success = Oodle::Download(OODLE_URL_W, oo2corepath.wstring().c_str());
		if (!success) {
			std::cout << "FATAL ERROR: Failed to download " << oo2corepath << "\n";
			return;
		}
		std::cout << "Download Complete (Oodle is a file decompression library)\n";
	}
	if (!Oodle::init(oo2corepath.string().c_str())) {
		std::cout << "FATAL ERROR: Failed to initialize " << oo2corepath << "\n";
		return;
	}

	for (int i = (int)packages.size() - 1; i > -1; i--) {
		
		std::filesystem::path resPath = PathBase / packages[i];
		std::cout << "Dumping: " << resPath.string() << "\n";

		// Parse the archive
		ResourceArchive archive;
		Read_ResourceArchive(archive, resPath.string(), RF_ReadEverything);

		ExtractFiles(outputFolder, archive);
	}

}

int main(int argc, char* argv[]) {
	ExtractorMain(argc, argv);
}