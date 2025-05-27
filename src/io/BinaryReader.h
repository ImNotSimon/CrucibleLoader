#include <string>
#include <fstream>

class IndexOOBException : public std::exception {};

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
	void Goto(const size_t newPos);
	void GoRight(const size_t shiftAmount);
	void ReadBytes(char* writeTo, const size_t numBytes);

	char* ReadCString();
	wchar_t* ReadWCStringLE();

	template<typename T>
	void ReadLE(T& readTo)
	{
		// Determine number of bytes to read
		constexpr size_t numBytes = sizeof(T);
		if (pos + numBytes > length)
			throw IndexOOBException();

		// Read the bytes into the value
		T value = 0;
		for (size_t i = 0, max = numBytes * 8; i < max; i += 8) {
			T temp = static_cast<unsigned char>(buffer[pos++]);

			// Literal expression overflows for > 32 bits if we don't shift on a temp variable
			// This ensures it works for 64 bits. TODO: research and find a better solution
			temp = temp << i; 
			value += temp;
		}

		readTo = value;
	}

	template<>
	void ReadLE(float& readTo)
	{
		union {
			uint32_t i = 0;
			float f;
			static_assert(sizeof(float) == sizeof(uint32_t));
		} binaryCast;

		ReadLE(binaryCast.i);
		readTo = binaryCast.f;
	}

	template<>
	void ReadLE(double& readTo)
	{
		union {
			uint64_t i = 0;
			double d;
			static_assert(sizeof(double) == sizeof(uint64_t));
		} binaryCast;

		ReadLE(binaryCast.i);
		readTo = binaryCast.d;
	}
};