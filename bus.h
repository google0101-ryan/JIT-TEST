#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace Bus
{

uint8_t Read8(uint16_t addr);
uint16_t Read16(uint16_t addr);

void Write8(uint16_t addr, uint8_t data);

void AttachBootROM(uint8_t* rom);

};