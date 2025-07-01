#include <iostream>
#include <charconv>
#include <iostream>
#include <iomanip>
#include "io/BinaryReader.h"

// Confirmed - atof seems to match for going from string --> float
// 0x3F333333 = 0.699999988
// 0x3C88889A = 0.0166666992

// -31.5 = 0xC1FC0000
// 44.0000114 = 0x42300003


// But what about from float --> string?



int main() {
	//for (int i = 0; i < 1000000; i++) {
	//	float f = atof("44.0000114441");
	//}
	//printf("done");

	//printf("reserial main");
	//float f = static_cast<float>(atof("44.0000114441"));
	//std::cout << std::hex << std::setfill('0') << std::setw(16) << *reinterpret_cast<uint32_t*>(&f) << std::endl;


	char buffer[128];
	uint32_t num = 0x42300003;
	float fuck = *reinterpret_cast<float*>(&num);
	snprintf(buffer, 128, "%1.10f", fuck);
	printf("%s\n", buffer);

	printf("%1.10f", 0.0000545345345);

	//std::string test = "1.0000$Bitch";
	//std::from_chars

	//char* end = reinterpret_cast<char*>(test.data() + 8);
	//float ass = std::strtof(test.c_str(), &end);

	//std::cout << ass;
	//printf("%f", ass);
	
	return 0;
}