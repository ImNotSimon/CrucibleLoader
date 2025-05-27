#pragma once
#include <string>
#include <vector>
#include <fstream>

class BinaryWriter
{
	private:
	std::ofstream file;
	char num[16]; // For converting numerical types to binary data
	std::vector<std::streampos> sizeStack;

	public:
	BinaryWriter(const std::string& filepath);
	~BinaryWriter();
	bool InitSuccessful();
	
	size_t Position();
	void Goto(const size_t newPos);

	void pushSizeStack();
	void popSizeStack();

	void WriteBytes(const char* bytes, const size_t numBytes);

	void WriteFiller(const char value, const size_t numBytes);

	#pragma warning(disable: 4333)
	template<typename T>
	void WriteLE(T value)
	{
		static_assert(sizeof(T) <= 16);

		for (size_t i = 0; i < sizeof(T); i++)
		{
			num[i] = value & 0xFF;
			value = value >> 8;
		}
		file.write(num, sizeof(T));
	}
	#pragma warning(default: 4333)

	//bool SaveTo(const std::string& path);

	//void AddCapacity(const size_t numBytes);
};