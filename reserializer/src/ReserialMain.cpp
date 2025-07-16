#include <iostream>
#include <charconv>
#include <iostream>
#include <iomanip>
#include "io/BinaryReader.h"
#include "io/BinaryWriter.h"

// Confirmed - atof seems to match for going from string --> float
// 0x3F333333 = 0.699999988
// 0x3C88889A = 0.0166666992

// -31.5 = 0xC1FC0000
// 44.0000114 = 0x42300003


// But what about from float --> string?


int main() {

	printf("reserial main");

	char buffer[128];
	float f;
	snprintf(buffer, 128, "%1.10f", f);
	
	return 0;
}