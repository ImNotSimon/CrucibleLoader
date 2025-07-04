#pragma once
#include <filesystem>
#include "entityslayer\GenericBlockAllocator.h"

struct ModDef;
struct ModFile;

struct ModDef {
	int loadPriority = 0;
	std::string modName;
	std::vector<ModFile> modFiles;

	/* Holds all of the mod's dynamically allocated file data */
	BlockAllocator<char> fileData = BlockAllocator<char>(50000);
};

struct ModFile {
	ModDef* parentMod = nullptr;
	std::string realPath;   // The verbatim path from the zip file or mods folder
	std::string assetPath;  // Path that will be used as the resource name
	std::string typeString; // Type string parsed from the resource path
	std::string_view data;       // File data
};

namespace ModReader {
	
	void ReadZipMod(ModDef& readto, const std::filesystem::path& zipPath);
}