#include "ModReader.h"
#include "miniz/miniz.h"
#include "entityslayer\EntityParser.h"
#include <unordered_map>
#include <filesystem>
#include <iostream>

#define CFG_PATH "darkagemod.txt"
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

void ModReader::ReadZipMod(ModDef& mod, const std::filesystem::path& zipPath)
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


	delete[] nameBuffer;
}