#pragma once
#include <filesystem>

class AtlanLogger {
	public:
	static void init(const char* configpath);
	static void exit();

	AtlanLogger& operator<<(const char* data);
	AtlanLogger& operator<<(const std::string& data);
	AtlanLogger& operator<<(const std::filesystem::path& data);
	//AtlanLogger& operator<<(const size_t data);
	AtlanLogger& operator<<(const int64_t data);
	AtlanLogger& logfileonly(const char* data);
	AtlanLogger& logfileonly(const std::string& data);
};

extern AtlanLogger atlog;