#include <jit.h>
#include <bus.h>
#include <fstream>

int main()
{
	std::ifstream file("bios.bin", std::ios::ate);

	size_t size = file.tellg();
	uint8_t* buf = new uint8_t[size];

	file.seekg(0, std::ios::beg);

	file.read((char*)buf, size);

	Bus::AttachBootROM(buf);

	JIT* jit = new JIT();

	jit->CompileBlock(0);

	printf("Done. Returning\n");

	file.close();

	return 0;
}