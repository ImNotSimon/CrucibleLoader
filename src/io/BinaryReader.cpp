#include "BinaryReader.h"
#include <fstream>
#include <string>
//#include <fstream>

BinaryReader::BinaryReader(const BinaryReader& b) {
	SetBuffer(b.buffer, b.length);
}

BinaryReader::BinaryReader(const std::string& path)
{
	SetBuffer(path);
}

BinaryReader::BinaryReader(char* p_buffer, size_t p_length)
{
	SetBuffer(p_buffer, p_length);
}

bool BinaryReader::SetBuffer(const std::string& path) {
	ClearState();

	std::ifstream file(path, std::ios_base::binary);
	if (file.fail())
		return false;

	// Technically, tellg() is not guaranteed to give the readable length
	// of a file. But in practice, it translates to a readable length
	// when the file is opened in binary mode
	file.seekg(0, std::ios_base::end);
	length = static_cast<size_t>(file.tellg());
	buffer = new char[length];
	file.seekg(0, std::ios_base::beg);
	file.read(buffer, length);
	file.close();

	ownsBuffer = true;
	return true;
}

bool BinaryReader::SetBuffer(std::ifstream& stream, size_t startAt, size_t expectedLength)
{
	ClearState();
	length = expectedLength;
	buffer = new char[length];
	ownsBuffer = true;

	stream.seekg(startAt, std::ios_base::beg);
	stream.read(buffer, length);
	if (length != static_cast<size_t>(stream.gcount())) {
		ClearState();
		return false;
	}

	return true;
}

void BinaryReader::SetBuffer(char* p_buffer, size_t p_length) {
	ClearState();

	buffer = p_buffer;
	length = p_length;
	ownsBuffer = false;
}

bool BinaryReader::InitSuccessful()
{
	return buffer != nullptr;
}

char* BinaryReader::GetBuffer()
{
	return buffer;
}

size_t BinaryReader::GetLength()
{
	return length;
}

size_t BinaryReader::Position() const
{
	return pos;
}

bool BinaryReader::ReachedEOF() const
{
	return pos == length;
}

bool BinaryReader::Goto(const size_t newPos)
{
	if(newPos > length)
		return false;
	pos = newPos;
	return true;
}

bool BinaryReader::GoRight(const size_t shiftAmount)
{
	if(pos + shiftAmount > length)
		return false;
	pos += shiftAmount;
	return true;
}

char* BinaryReader::ReadCString()
{
	size_t inc = pos;
	bool foundNull = false;
	
	// Determine if there's a valid c string we can read
	while (inc < length)
	{
		if (buffer[inc++] == '\0') {
			foundNull = true;
			break;
		}
	}
	if(!foundNull)
		return nullptr;

	// Copy the string into a new buffer
	size_t length = inc - pos;
	char* ptr = new char[length];
	memcpy(ptr, buffer + pos, length);
	pos = inc;

	return ptr;
}

wchar_t* BinaryReader::ReadWCStringLE()
{
	size_t inc = pos;
	bool foundNull = false;

	// Determine if there's a valid wide C string we can read
	while (inc < length)
	{
		if (buffer[inc++] == '\0') {
			if (inc < length && buffer[inc++] == '\0') {
				foundNull = true;
				break;
			}
		} else inc++;
	}
	if(!foundNull)
		return nullptr;

	// Copy the string into a new buffer
	size_t length = (inc - pos) / 2;
	wchar_t* ptr = new wchar_t[length];
	for (size_t i = 0; i < length; i++) {
		ptr[i] = buffer[pos++];
		ptr[i] += static_cast<unsigned char>(buffer[pos++]) << 8;
	}

	return ptr;
}

void BinaryReader::DebugLogState()
{
const char* msg = R"(Buffer Addr:	%zu
Buffer Length	%zu
Position	%zu
Owns Buffer	%i
)";
	printf(msg, buffer, length, pos, ownsBuffer);
}