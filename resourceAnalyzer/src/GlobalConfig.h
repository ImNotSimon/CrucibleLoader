#pragma once
#include <string>
#include <filesystem>

#define MOD_LOADER_VERSION 1

enum ArgFlags {
	argflag_resetvanilla = 1 << 0,
	argflag_gameupdated = 1 << 1,
	argflag_verbose = 1 << 2,
	argflag_nolaunch = 1 << 3,
	argflag_forceload = 1 << 4
};

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