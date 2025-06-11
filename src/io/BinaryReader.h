#include <string>
#include <fstream>

class BinaryReader
{
	private:
	bool ownsBuffer = false;
	char* buffer = nullptr;
	size_t length = 0;
	size_t pos = 0;

	public:
	void ClearState() {
		if (ownsBuffer)
			delete[] buffer;
		ownsBuffer = false;
		buffer = nullptr;
		length = 0;
		pos = 0;
	}

	~BinaryReader() {
		if (ownsBuffer)
			delete[] buffer;
	}

	BinaryReader() {
	}
	BinaryReader(const BinaryReader& b);
	BinaryReader(const std::string& path);
	BinaryReader(char* p_buffer, size_t p_length);
	bool SetBuffer(const std::string& path);
	bool SetBuffer(std::ifstream& stream, size_t startAt, size_t expectedLength);
	void SetBuffer(char* p_buffer, size_t p_length);
	bool InitSuccessful();
	char* GetBuffer();
	size_t GetLength();

	void DebugLogState();

	// =====
	// Reading
	// =====
	size_t Position() const;
	bool ReachedEOF() const;
	bool Goto(const size_t newPos);
	bool GoRight(const size_t shiftAmount);


	/*
	* Reading Functions
	*/

	char* ReadCString();
	wchar_t* ReadWCStringLE();

	bool ReadBytes(char* writeTo, const size_t numBytes)
	{
		if (pos + numBytes > length)
			return false;

		memcpy(writeTo, buffer + pos, numBytes);
		pos += numBytes;
		return true;
	}

	bool ReadLE(int8_t& readTo)
	{
		if(pos + 1 > length)
			return false;
		readTo = buffer[pos++];
		return true;
	}

	bool ReadLE(uint8_t& readTo)
	{
		if(pos + 1 > length)
			return false;
		readTo = *reinterpret_cast<unsigned char*>(buffer + pos++);
		return true;
	}

	bool ReadLE(uint16_t& readTo) {
		if(pos + 2 > length)
			return false;
		
		readTo = _byteswap_ushort( *reinterpret_cast<uint16_t*>(buffer + pos));
		pos += 2;
		return true;
	}

	bool ReadLE(int16_t& readTo) {
		uint16_t u;
		if (ReadLE(u)) {
			readTo = *reinterpret_cast<int16_t*>(&u);
			return true;
		}
		return false;
	}

	bool ReadLE(uint32_t& readTo)
	{
		if (pos + 4 > length)
			return false;

		readTo = _byteswap_ulong(*reinterpret_cast<uint32_t*>(buffer + pos));
		pos += 4;
		return true;
	}

	bool ReadLE(int32_t& readTo) {
		uint32_t u;
		if (ReadLE(u)) {
			readTo = *reinterpret_cast<int32_t*>(&u);
			return true;
		}
		return false;
		
	}

	bool ReadLE(uint64_t& readTo) {
		if (pos + 8 > length)
			return false;

		readTo = _byteswap_uint64(*reinterpret_cast<uint64_t*>(buffer + pos));
		pos += 8;
		return true;
	}

	bool ReadLE(int64_t& readTo) {
		uint64_t u;
		if (ReadLE(u)) {
			readTo = *reinterpret_cast<int64_t*>(&u);
			return true;
		}
		return false;
	}

	bool ReadLE(float& readTo) {
		uint32_t u;
		if (ReadLE(u)) {
			readTo = *reinterpret_cast<float*>(&u);
			return true;
		}
		return false;
	}

	bool ReadLE(double& readTo) {
		uint64_t u;
		if (ReadLE(u)) {
			readTo = *reinterpret_cast<double*>(&u);
			return true;
		}
		return false;
	}
};