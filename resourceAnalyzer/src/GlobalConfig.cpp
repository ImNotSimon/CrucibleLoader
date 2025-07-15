#include "GlobalConfig.h"
#include <iostream>
#include <fstream>
#include <filesystem>

AtlanLogger atlog;
std::ofstream logfile;

void AtlanLogger::init(const char* configpath) {
	bool replaceExisting = true;
	if (std::filesystem::exists(configpath)) {
		replaceExisting = std::filesystem::file_size(configpath) > 100000;
	}

	logfile.open(configpath, std::ios_base::binary | (replaceExisting ? 0 : std::ios_base::app));
	if(replaceExisting)
		atlog.logfileonly("Log file size threshold exceeded. Starting new log file.\n");

	char buffer[256];
	time_t timestamp;
	time(&timestamp);
	ctime_s(buffer, 256, &timestamp);
	atlog.logfileonly("\n\n----------\n").logfileonly(buffer);
}

void AtlanLogger::exit() {
	logfile.close();
}

AtlanLogger& AtlanLogger::operator<<(const char* data)
{
	std::cout << data;
	logfile << data;
	return *this;
}

AtlanLogger& AtlanLogger::operator<<(const std::string& data)
{
	std::cout << data;
	logfile << data;
	return *this;
}

AtlanLogger& AtlanLogger::operator<<(const std::filesystem::path& data)
{
	std::cout << data;
	logfile << data;
	return *this;
}

//AtlanLogger& AtlanLogger::operator<<(const size_t data)
//{
//	std::string s = std::to_string(data);
//	std::cout << s;
//	logfile << s;
//	return *this;
//}

AtlanLogger& AtlanLogger::operator<<(const int64_t data)
{
	std::string s = std::to_string(data);
	std::cout << s;
	logfile << s;
	return *this;
}

AtlanLogger& AtlanLogger::logfileonly(const char* data)
{
	logfile << data;
	return *this;
}

AtlanLogger& AtlanLogger::logfileonly(const std::string& data)
{
	logfile << data;
	return *this;
}
