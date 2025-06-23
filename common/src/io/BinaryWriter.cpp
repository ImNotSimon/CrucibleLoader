#include "BinaryWriter.h"

BinaryWriter::BinaryWriter(const std::string& filepath) {
	file.open(filepath, std::ios_base::binary);
}

BinaryWriter::~BinaryWriter()
{
	file.close();
}

bool BinaryWriter::InitSuccessful() {
	return file.good();
}

size_t BinaryWriter::Position() {
	return file.tellp();
}

void BinaryWriter::Goto(const size_t newPos) {
	file.seekp(newPos);
}

void BinaryWriter::pushSizeStack() {
	std::streampos pos = file.tellp();
	sizeStack.push_back(pos);
	file.seekp((size_t)pos + sizeof(uint32_t));
}

void BinaryWriter::popSizeStack() {
	if(sizeStack.empty())
		return;

	std::streampos sizePos = sizeStack.back();
	std::streampos currentPos = file.tellp();
	uint32_t blockSize = static_cast<uint32_t>(currentPos - sizePos - sizeof(uint32_t));

	file.seekp(sizePos);
	WriteLE(blockSize);
	file.seekp(currentPos);
	sizeStack.pop_back();
}

void BinaryWriter::WriteBytes(const char* bytes, const size_t numBytes)
{
	file.write(bytes, numBytes);
}

void BinaryWriter::WriteFiller(const char value, const size_t numBytes)
{
	char* buff = new char[numBytes];
	for(size_t i = 0; i < numBytes; i++)
		buff[i] = value;
	file.write(buff, numBytes);
	delete[] buff;
}

//bool BinaryWriter::SaveTo(const std::string& path)
//{
//	std::ofstream output(path, std::ios_base::binary);
//	if(!output.good())
//		return false;
//
//	output.write(buffer, pos); // Only write the written portion of the buffer
//	output.close();
//	return true;
//}

//void BinaryWriter::AddCapacity(const size_t numBytes)
//{
//	char* newBuffer = new char[capacity + numBytes];
//
//	if (buffer != nullptr) {
//		memcpy(newBuffer, buffer, capacity);
//		delete[] buffer;
//	}
//	buffer = newBuffer;
//	capacity += numBytes;
//}