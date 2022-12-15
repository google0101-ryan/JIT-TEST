#include <bus.h>
#include <cstring>

namespace Bus
{

uint8_t boot_rom[0x100];
uint8_t* vram;

uint8_t Read8(uint16_t addr)
{
	if (addr < 0x100)
		return boot_rom[addr];
	
	printf("Read from unknown addr 0x%04x\n", addr);
	exit(1);
}

uint16_t Read16(uint16_t addr)
{
	return Read8(addr+1) << 8 | Read8(addr);
}

void Write8(uint16_t addr, uint8_t data)
{
	if (addr > 0x8000 && addr <= 0x9fff)
	{
		vram[addr - 0x8000] = data;
		return;
	}

	printf("Write 0x%02x to unknown address 0x%04x\n", data, addr);
	exit(1);
}

void AttachBootROM(uint8_t* rom)
{
	memcpy(boot_rom, rom, 0x100);

	vram = new uint8_t[0x2000];
}

}