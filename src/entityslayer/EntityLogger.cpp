#include "EntityLogger.h"

void EntityLogger::log(const std::string& data)
{
	printf("%s", data.c_str());
}

void EntityLogger::logWarning(const std::string& data)
{
	printf("Parser Warning: %s", data.c_str());
}

void EntityLogger::logTimeStamps(const std::string& msg,
	const std::chrono::steady_clock::time_point startTime)
{
	auto stopTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - startTime);

	printf("%s %zu", msg.c_str(), duration.count());
}