#pragma once

#include <cstdint>

enum class Instruction : uint8_t
{
	jr_i8 = 0x18,
	jr_nz_i8 = 0x20,
	ld_hl_u16 = 0x21,
	jr_z_i8 = 0x28,
	jr_nc_i8 = 0x30,
	ld_sp_u16 = 0x31,
	ld_hl_minus_a = 0x32,
	jr_c_i8 = 0x38,
	xor_a = 0xaf,
	ret_nz = 0xC0,
	jp_nz_u16 = 0xC2,
	jp_u16 = 0xC3,
	call_nz_u16 = 0xC4,
	rst_00 = 0xC7,
	ret_z = 0xC8,
	ret = 0xC9,
	jp_z_u16 = 0xCA,
	prefix_cb = 0xCB,
	call_z_u16 = 0xCC,
	call_u16 = 0xCD,
	rst_08 = 0xCF,
	ret_nc = 0xD0,
	jp_nc_u16 = 0xD2,
	call_nc_u16 = 0xD4,
	rst_10 = 0xD6,
	ret_c = 0xD7,
	reti = 0xD8,
	jp_c_u16 = 0xDA,
	call_c_u16 = 0xDC,
	rst_18 = 0xDF,
	rst_20 = 0xE7,
	jp_hl = 0xE9,
	rst_28 = 0xEF,
	rst_30 = 0xF7,
	rst_38 = 0xFF
};

enum HostRegisters
{
	RAX = 0,
	RCX = 1,
	RDX = 2,
	RBX = 3,
	RSP = 4,
	RBP = 5,
	RSI = 6,
	RDI = 7,
	HostRegCount
};

enum HostRegisters8
{
	BH = 7,
	BL = 3
};