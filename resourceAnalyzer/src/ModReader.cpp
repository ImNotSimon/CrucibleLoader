#include "ModReader.h"
#include "miniz/miniz.h"
#include "entityslayer\EntityParser.h"
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <iostream>

#define CFG_PATH "darkagesmod.txt"
#define CFG_REQUIREDVERSION "requiredVersion"
#define CFG_LOADPRIORITY "loadPriority"
#define CFG_ALIASES "aliasing"

typedef std::filesystem::path fspath;

struct configData_t {
	int requiredversion = 1;
	int loadpriority = 0;
	std::unordered_map<std::string, std::string> alias;
};

void ReadConfigData(configData_t& cfg, mz_zip_archive* zptr)
{
	char* dataBuffer = nullptr;
	size_t dataLength = 0;
	EntityParser configParser(ParsingMode::PERMISSIVE);

	/* 
	* Extract file from zip
	*/
	dataBuffer = static_cast<char*>(mz_zip_reader_extract_file_to_heap(zptr, CFG_PATH, &dataLength, 0));
	if (!dataBuffer) {
		std::cout << "\nWARNING: Could not find " << CFG_PATH;
		return;
	}

	/*
	* Parse config into the node tree
	*/
	ParseResult presult = configParser.EditTree(std::string(dataBuffer, dataLength), configParser.getRoot(), 0, 0, 0, 0);
	delete[] dataBuffer;
	if (!presult.success) {
		std::cout << "\nWARNING: " << CFG_PATH << " cannot be read due to syntax errors.";
		return;
	}

	std::cout << "\nFound " << CFG_PATH;

	/*
	* Read config properties
	*/
	EntNode& root = *configParser.getRoot();
	EntNode& modInfo = root["modinfo"];
	
	bool foundReqVersion = modInfo[CFG_REQUIREDVERSION].ValueInt(cfg.requiredversion, INT_MIN, INT_MAX);
	bool foundLoadPriority = modInfo[CFG_LOADPRIORITY].ValueInt(cfg.loadpriority, INT_MIN, INT_MAX);

	if (!foundReqVersion) 
	{
		std::cout << "\nWARNING: " << CFG_REQUIREDVERSION << " not found. Using default value";
	}
	if (!foundLoadPriority) 
	{
		std::cout << "\nWARNING: " << CFG_LOADPRIORITY << " not found. Using default value";
	}

	/*
	* Read optional filepath aliases
	*/
	EntNode& aliasNode = root[CFG_ALIASES];
	for (int i = 0, max = aliasNode.getChildCount(); i < max; i++) {
		EntNode& currentAlias = *aliasNode.ChildAt(i);
		cfg.alias.emplace(currentAlias.getNameUQ(), currentAlias.getValueUQ());
	}

	//for (auto& pair : cfg.alias) {
	//	std::cout << "\n" << pair.first << "-" << pair.second;
	//}
}

/*
* The strategy: zip loose mod files into a zip file, and then re-read that zip.
* 
* Why do things in such an inefficient manner? Because the alternative is to maintain two large
* codepaths that must behave exactly the same while using two completely different IO interfaces.
* It's far less of a headache to merge the rarely-used codepath into the commonly used one.
*/
void ModReader::ReadLooseMod(ModDef& readto, const fspath& tempzippath, const std::vector<fspath>& pathlist, const int loaderVersion)
{
	if(pathlist.empty())
		return;
	std::cout << "Zipping Loose Mod Files\n";

	size_t substringIndex = tempzippath.parent_path().string().size() + 1; // + 1 Accounts for the backslash

	// Initialize the zip data
	mz_zip_archive zipfile;
	mz_zip_archive* zptr = &zipfile;
	mz_zip_zero_struct(zptr);
	mz_zip_writer_init_heap(zptr, 4096, 4096);

	// Read files from disk into the zip data
	for (const fspath& fp : pathlist) {
		std::string zippedName = fp.string().substr(substringIndex);

		bool result = mz_zip_writer_add_file(zptr, zippedName.c_str(), fp.string().c_str(), "", 0, MZ_DEFAULT_COMPRESSION);
		if (!result)
			std::cout << "ERROR: Failed to add file to zip\n";
	}

	// Get the finished zip data
	size_t bufferused = 0;
	void* buffer = nullptr;
	bool finalize = mz_zip_writer_finalize_heap_archive(zptr, &buffer, &bufferused);
	if (!finalize) {
		std::cout << "ERROR: Failed to finalize zip archive\n";
	}

	// Write the zip data
	std::ofstream outzip(tempzippath, std::ios_base::binary);
	outzip.write((char*)buffer, bufferused);
	outzip.close();

	// Clean up heap data
	mz_zip_writer_end(zptr);
	delete[] buffer;
	std::cout << "Finished zipping loose mod files\n";
	
	// Read the zip to a mod struct
	ReadZipMod(readto, tempzippath, loaderVersion);
	// todo: mz_zip_reader_init_mem

	// Delete the temporary mod zip
	std::filesystem::remove(tempzippath);
}

void ModReader::ReadZipMod(ModDef& mod, const fspath& zipPath, const int loaderVersion)
{
	std::cout << "\nReading " << zipPath.filename();

	// Open the zip file
	mz_zip_archive zipfile;
	mz_zip_archive* zptr = &zipfile;
	mz_zip_zero_struct(zptr);
	if (!mz_zip_reader_init_file(zptr, zipPath.string().c_str(), 0))
	{
		std::cout << "Failed to open zip file\n";
		return;
	}
	
	/*
	* Read config files if they exist
	*/
	configData_t cfg;
	ReadConfigData(cfg, zptr);
	mod.loadPriority = cfg.loadpriority;
	mod.modName = zipPath.stem().string();

	/*
	* Check if version requirement is met. If it isn't, skip reading the mod files
	*/
	if (loaderVersion < cfg.requiredversion) {
		std::cout << "\nERROR: Mod requires Mod Loader Version " << cfg.requiredversion << " or greater. (Your version is " << loaderVersion << ")\n";
		mz_zip_reader_end(zptr);
		return;
	}


	/*
	* Read all mod files
	*/
	uint32_t fileCount = mz_zip_reader_get_num_files(zptr);
	size_t nameBufferMax = 512;
	char* nameBuffer = new char[nameBufferMax];
	for (uint32_t i = 0; i < fileCount; i++) {
		if(mz_zip_reader_is_file_a_directory(zptr, i))
			continue;

		ModFile modfile;

		/*
		* Get the file name
		*/
		uint32_t nameLength = mz_zip_reader_get_filename(zptr, i, nullptr, 0);
		if (nameLength > nameBufferMax) {
			delete[] nameBuffer;
			nameBuffer = new char[nameLength];
			nameBufferMax = nameLength;
		}
		mz_zip_reader_get_filename(zptr, i, nameBuffer, nameLength);
		nameLength--; // The null char is included in the name length...(but data length is correct and without a nullchar)
		modfile.realPath = std::string(nameBuffer, nameLength);

		/*
		* If this is a pre-parsed config file, skip it
		*/
		if(modfile.realPath == CFG_PATH)
			continue;
		std::cout << "\n   \"" << modfile.realPath << "\" --> ";


		/*
		* If an alias exists for this file name, load it into the name buffer
		*/
		{
			auto iter = cfg.alias.find(modfile.realPath);
			if(iter != cfg.alias.end()) {
				const std::string& alias = iter->second;

				nameLength = static_cast<int>(alias.length());
				if (nameLength > nameBufferMax) {
					delete[] nameBuffer;
					nameBuffer = new char[nameLength];
					nameBufferMax = nameLength;
				}
				memcpy(nameBuffer, alias.data(), alias.length());
			}
		}

		/*
		* Identify the asset type
		*/
		uint32_t delimiter = 0;
		for (delimiter = 0; delimiter < nameLength; delimiter++) {
			char c = nameBuffer[delimiter];
			if(c == '/' || c == '\\' || c == '@')
				break;
		}
		if (delimiter >= nameLength) {
			std::cout << "ERROR: Missing resource type string";
			continue;
		}
		std::string typeString = std::string(nameBuffer, delimiter);


		/*
		* Map the typeString to a resource type
		*/
		if (typeString == "rs_streamfile") {
			modfile.assetType = ModFileType::rs_streamfile;
		}
		else {
			std::cout << "ERROR: Unknown/Unsupported resource type \"" << typeString << "\"";
			continue;
		}


		/*
		* Create the resource string - Beginning after the resource type
		*/
		char* nameEnd = nameBuffer + delimiter + 1;
		while (nameEnd < nameBuffer + nameLength) {
			if (*nameEnd == '@' || *nameEnd == '\\') {
				*nameEnd = '/';
			}
			else if (*nameEnd == '.') {
				if (modfile.assetType == ModFileType::entityDef) {
					break;
				}
			}
			nameEnd++;
		}
		modfile.assetPath = std::string(nameBuffer + delimiter + 1, nameEnd);

		// TODO: Should probably do a thorough error checking of the asset path.
		// Ensure things like there's no whitespace, etc.
		std::cout << modfile.assetPath;

		/*
		* Read the mod data
		*/

		void* dataBuffer = nullptr;
		size_t dataLength = 0;
		dataBuffer = mz_zip_reader_extract_to_heap(zptr, i, &dataLength, 0);
		if (!dataBuffer) {
			std::cout << " ERROR: Failed to extract file\n";
			continue;
		}
		modfile.dataBuffer = dataBuffer;
		modfile.dataLength = dataLength;


		/*
		* Finish processing the file
		*/
		modfile.parentMod = &mod;
		mod.modFiles.push_back(modfile);
	}

	mz_zip_reader_end(zptr);
	delete[] nameBuffer;
}