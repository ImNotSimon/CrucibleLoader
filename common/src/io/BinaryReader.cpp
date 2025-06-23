#include "BinaryReader.h"
#include <fstream>

void BinaryReader::DebugLogState() const
{
const char* msg = R"(Buffer Addr:	%zu
Buffer Length	%zu
Position	%zu
)";
	printf(msg, buffer, GetLength(), GetPosition());
}

BinaryOpener::BinaryOpener(const std::string& path)
{
	std::ifstream file(path, std::ios_base::binary);
	if (file.fail())
		return;

	// Technically, tellg() is not guaranteed to give the readable length
	// of a file. But in practice, it translates to a readable length
	// when the file is opened in binary mode
	file.seekg(0, std::ios_base::end);
	length = static_cast<size_t>(file.tellg());
	buffer = new char[length];
	file.seekg(0, std::ios_base::beg);
	file.read(buffer, length);
	file.close();
}