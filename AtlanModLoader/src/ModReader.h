#pragma once
#include <filesystem>

struct ModDef;
struct ModFile;

typedef std::filesystem::path fspath;

enum class ModFileType : uint8_t {
	rs_streamfile = 0,
	entityDef = 1
};

struct ModDef {
	int loadPriority = 0;
	std::string modName;
	std::vector<ModFile> modFiles;
};

struct ModFile {
	ModFileType assetType;
	ModDef* parentMod = nullptr;
	void* dataBuffer = nullptr;
	size_t dataLength = 0;
	std::string realPath;   // The verbatim path from the zip file or mods folder
	std::string assetPath;  // Path that will be used as the resource name
};

inline void ModFile_Free(ModFile& mfile) {
	delete[] mfile.dataBuffer;
}

inline void ModDef_Free(ModDef& mod) {
	for (ModFile& f : mod.modFiles) {
		ModFile_Free(f);
	}
	mod.modFiles.clear();
}

namespace ModReader {
	
	void ReadLooseMod(ModDef& readto, const fspath& tempzippath, const std::vector<fspath>& pathlist, int argflags);
	void ReadZipMod(ModDef& readto, const fspath& zipPath, int argflags);
}