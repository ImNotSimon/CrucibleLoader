#include "ModReader.h"
#include "miniz/miniz.h"
#include <filesystem>
#include <iostream>

typedef std::filesystem::path fspath;

void ModReader::ReadZipMod(ModDef& mod, const std::filesystem::path& zipPath)
{
	std::cout << "Reading " << zipPath.filename() << "\n";

	// Open the zip file
	mz_zip_archive zipfile;
	mz_zip_archive* zptr = &zipfile;
	mz_zip_zero_struct(zptr);
	if (!mz_zip_reader_init_file(zptr, zipPath.string().c_str(), 0))
	{
		std::cout << "Failed to open zip file\n";
		return;
	}


	uint32_t fileCount = mz_zip_reader_get_num_files(zptr);
	size_t nameBufferMax = 512;
	char* nameBuffer = new char[nameBufferMax];

	for (uint32_t i = 0; i < fileCount; i++) {
		if(mz_zip_reader_is_file_a_directory(zptr, i))
			continue;

		ModFile modfile;

		// Get the file name
		uint32_t nameLength = mz_zip_reader_get_filename(zptr, i, nullptr, 0);
		if (nameLength > nameBufferMax) {
			delete[] nameBuffer;
			nameBuffer = new char[nameLength];
			nameBufferMax = nameLength;
		}
		mz_zip_reader_get_filename(zptr, i, nameBuffer, nameLength);
		modfile.realPath = std::string(nameBuffer, nameLength);

		// Identify the asset type
		

		std::cout << std::string_view(nameBuffer, nameLength) << " " << mz_zip_reader_is_file_a_directory(zptr, i) << "\n";
	}


	delete[] nameBuffer;
	//mz_zip_get_archive_size
}