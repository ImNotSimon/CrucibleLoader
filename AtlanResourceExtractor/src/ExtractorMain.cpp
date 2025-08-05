#include "entityslayer/Oodle.h"
#include "entityslayer/EntityParser.h"
#include "archives/ResourceStructs.h"
#include "archives/PackageMapSpec.h"
#include "atlan/AtlanLogger.h"
#include <iostream>
#include <fstream>
#include <set>
#include <thread>

typedef std::set<std::string> restypeset_t;

struct configdata_t {
	fspath inputdir = "";
	fspath outputdir = "";
	bool run_extractor = true;
	bool run_deserializer = true;

	restypeset_t restypes;

};

void ExtractFiles(std::filesystem::path outputDir, const ResourceArchive& r, const restypeset_t& validTypes) {
	int fileCount = 0;

	char* decompBlob = nullptr;
	size_t decompSize = 0;

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

		const fspath output_path = fileDir / assetpath.filename();
		if (output_path.string().length() > 250)
			atlog << "WARNING: Filepath " << output_path << " exceeding safe limit. Unexpected behavior may occur\n";

		// Normal for this to return false on success, for whatever reason
		std::filesystem::create_directories(fileDir);

		char* dataLocation = r.bufferData + (e.dataOffset - r.header.dataOffset);
		std::ofstream outputFile(output_path, std::ios_base::binary);

		switch(e.compMode) {
			
			default:
			atlog << "ERROR: Unknown compression format " << e.compMode << " on file " << output_path;

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
					atlog << "Oodle Decompression Failed for " << (fileDir / assetpath.filename()) << "\n";
				outputFile.write(decompBlob, e.uncompressedSize);
			}
			break;
		}


		outputFile.close();
		fileCount++;
	}

	delete[] decompBlob;
	atlog << "Dumped " << fileCount << " files from archive\n";
}

/*
* CONSOLIDATED RESOURCE EXTRACTOR FUNCTION
*/
void ExtractorMain() {
	/*
	* REMEMBER TO UPDATE VERSION NUMBER
	*/
	atlog << "Atlan Consolidated Resource Extractor v1.2 by FlavorfulGecko5\n";

	/*
	* Parse and validate config file
	*/
	configdata_t config;
	try
	{
		#define configpath "extractor_config.txt"
		EntityParser parser(configpath, ParsingMode::PERMISSIVE);

		EntNode& root = *parser.getRoot();
		EntNode& core = root["core"];

		config.inputdir = core["input_folder"].getValueUQ();
		config.outputdir = core["output_folder"].getValueUQ();

		if (!std::filesystem::is_directory(config.inputdir)) {
			atlog << "FATAL ERROR: " << config.inputdir << " is not a valid directory\n"
				<< "Did you remember to set your input/output folders in " << configpath << "?";
			return;
		}
		if (!std::filesystem::is_directory(config.outputdir)) {
			atlog << "FATAL ERROR: " << config.outputdir << " is not a valid directory"
				<< "Did you remember to set your input/output folders in " << configpath << "?";
			return;
		}

		config.inputdir = std::filesystem::absolute(config.inputdir);
		config.outputdir = std::filesystem::absolute(config.outputdir);

		if (config.outputdir.string().size() >= 16) {
			atlog << "FATAL ERROR: Output directory must be less than 16 characters.\n"
				<< "This is to prevent export errors due to long filepaths.\n"
				<< "Your output directory " << config.outputdir << " is " << config.outputdir.string().size() << " characters";
			return;
		}

		if (!core["run_extractor"].ValueBool(config.run_extractor)) {
			atlog << "WARNING: Failed to read config bool core/run_extractor: assuming default\n";
		}
		if(!core["run_deserializer"].ValueBool(config.run_deserializer)) {
			atlog << "WARNING: Failed to read config bool core/run_deserializer: assuming default\n";
		}


		EntNode& restypes = root["extractor"]["resource_types"];
		for (int i = 0; i < restypes.getChildCount(); i++) {
			EntNode& rt = *restypes.ChildAt(i);

			if(rt.IsComment())
				continue;

			config.restypes.insert(std::string(rt.getNameUQ()));
		}
		atlog << "Found " << config.restypes.size() << " resource types\n";
	}
	catch(...) {
		atlog << "FATAL ERROR: failed to parse " << configpath << "\n";
		return;
	}

	/*
	* Read and verify PackageMapSpec data
	*/
	config.outputdir /= "atlan";
	std::vector<std::string> packages = PackageMapSpec::GetPrioritizedArchiveList(config.inputdir, false);

	if (packages.empty()) {
		atlog << "FATAL ERROR: Could not find PackageMapSpec.json\n"
			<< "Did you enter the correct path to your Dark Ages folder?";
		return;
	}
	else {
		atlog << "Found DOOM The Dark Ages Folder\n"
			<< "Dumping data to " << config.outputdir << "\n";
	}
	std::filesystem::create_directories(config.outputdir);

	/*
	* Download and load Oodle
	* (Do it here so it doesn't get downloaded to the wrong folder if install folder is input wrong)
	*/
	const fspath oo2corepath = config.inputdir / "oo2core_9_win64.dll";
	if(!std::filesystem::exists(oo2corepath)) {
		// Because nobody ever needed a simple STL function to convert a string to a wide string....
		#define OODLE_URL    "https://github.com/WorkingRobot/OodleUE/raw/refs/heads/main/Engine/Source/Programs/Shared/EpicGames.Oodle/Sdk/2.9.10/win/redist/oo2core_9_win64.dll"
		#define OODLE_URL_W L"https://github.com/WorkingRobot/OodleUE/raw/refs/heads/main/Engine/Source/Programs/Shared/EpicGames.Oodle/Sdk/2.9.10/win/redist/oo2core_9_win64.dll"
		atlog << "Downloading " << oo2corepath << " from " << OODLE_URL << "\n";

		bool success = Oodle::Download(OODLE_URL_W, oo2corepath.wstring().c_str());
		if (!success) {
			atlog << "FATAL ERROR: Failed to download " << oo2corepath << "\n";
			return;
		}
		atlog << "Download Complete (Oodle is a file decompression library)\n";
	}
	if (!Oodle::init(oo2corepath.string().c_str())) {
		atlog << "FATAL ERROR: Failed to initialize " << oo2corepath << "\n";
		return;
	}


	if (config.run_extractor) {
		atlog << "Performing resource extraction\n";

		const fspath basepath = config.inputdir / "base";

		for (int i = (int)packages.size() - 1; i > -1; i--) {

			std::filesystem::path resPath = basepath / packages[i];
			atlog << "Dumping: " << resPath.string() << "\n";

			// Parse the archive
			ResourceArchive archive;
			Read_ResourceArchive(archive, resPath.string(), RF_ReadEverything);

			ExtractFiles(config.outputdir, archive, config.restypes);
		}
	}
	else {
		atlog << "Skipping resource extraction\n";
	}
}

int main(int argc, char* argv[]) {
	#define logpath "extractor_log.txt"

	try {
		AtlanLogger::init(logpath);
		ExtractorMain();
	}
	catch (std::exception e) {
		atlog << "\n\nFATAL ERROR: An unexpected crash has occurred\n"
			<< "This may have left your extracted files incomplete or corrupted.\n"
			<< "Error Message: " << e.what();
	}

	atlog << "\n\nThis window will close in 10 seconds\n";
	atlog << "Output written to " << logpath << "\n";
	AtlanLogger::exit();
	

	#ifndef _DEBUG
	std::this_thread::sleep_for(std::chrono::seconds(10));
	#endif
}