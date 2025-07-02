#include "BinaryWriter.h"
#include <fstream>

bool BinaryWriter::SaveTo(const std::string& path)
{
	std::ofstream output(path, std::ios_base::binary);
	if(!output.good())
		return false;

	output.write(buffer, GetFilledSize()); // Only write the written portion of the buffer
	output.close();
	return true;
}