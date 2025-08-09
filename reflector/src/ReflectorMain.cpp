#include "reflector.h"
#include "cleaner.h"

void ReflectIdlib() {
	idlibCleaning::JsonToHeader();
	idlibCleaning::Pass1();
	idlibCleaning::Pass2();
	idlibReflection::Generate();
}

int main() {
	ReflectIdlib();
	return 0;
}