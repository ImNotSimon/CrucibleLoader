#pragma once
#include <filesystem>
#include <unordered_map>

struct ModDef;
struct ModFile;

typedef std::filesystem::path fspath;

enum class ModFileType : uint8_t {
	rs_streamfile,
	entityDef,
	mapentities,
	image
};

enum resourcetypeflags_t {
	RTF_None = 0,
	RTF_NoExtension = 1 << 0,
	RTF_Disabled = 1 << 1
};

struct resourcetypeinfo_t {
	std::string_view typestring;
	int typeflags; // resourcetypeflags_t
	ModFileType typeenum;

};

struct ModDef {
	int loadPriority = 0;
	std::string modName;
	std::vector<ModFile> modFiles;
};

struct ModFile {
	const resourcetypeinfo_t* typedata = nullptr;
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
	
	void ReadLooseMod(ModDef& readto, const fspath& modsfolder, const std::vector<fspath>& pathlist, int argflags);
	void ReadZipMod(ModDef& readto, const fspath& zipPath, int argflags);
}