#include "jit.h"
#include "instructions.h"
#include <sys/mman.h>

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <errno.h>
#include <fstream>
#include <libexplain/mmap.h>

State g_state;

JIT::JIT(uint8_t* code_buf)
{
	base = (uint8_t*)mmap(nullptr, 0xffffffff, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (base == MAP_FAILED)
	{
		printf("Error allocating address space for JIT (%s)\n", explain_errno_mmap(errno, nullptr, 0xffffffff, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
		exit(1);
	}

	free_base = base;
	code_used = 0;

	guest_code = code_buf;
}

bool JIT::DoesOpcodeModifyPC(uint8_t op)
{
	switch (static_cast<Instruction>(op))
	{
	case Instruction::jr_i8:
	case Instruction::jr_nz_i8:
	case Instruction::jr_z_i8:
	case Instruction::jr_nc_i8:
	case Instruction::jr_c_i8:
	case Instruction::ret_nz:
	case Instruction::jp_nz_u16:
	case Instruction::jp_u16:
	case Instruction::call_nz_u16:
	case Instruction::rst_00:
	case Instruction::ret_z:
	case Instruction::ret:
	case Instruction::jp_z_u16:
	case Instruction::call_z_u16:
	case Instruction::call_u16:
	case Instruction::rst_08:
	case Instruction::ret_nc:
	case Instruction::jp_nc_u16:
	case Instruction::call_nc_u16:
	case Instruction::rst_10:
	case Instruction::ret_c:
	case Instruction::reti:
	case Instruction::jp_c_u16:
	case Instruction::call_c_u16:
	case Instruction::rst_18:
	case Instruction::rst_20:
	case Instruction::jp_hl:
	case Instruction::rst_28:
	case Instruction::rst_30:
	case Instruction::rst_38:
		return true;
	default:
		return false;
	}
}

void JIT::EmitLea(int reg, uint8_t targetDisplacement)
{
	WriteU8(0x48);
	WriteU8(0x8D);

	uint8_t modrm = (0b01 << 6);
	modrm |= (reg & 0b111) << 3;
	modrm |= 0b101;

	WriteU8(modrm);
	WriteU8(targetDisplacement);

	instrs_compiled++;
}

void JIT::EmitMovRegAddress16(int reg, uint16_t imm)
{
	WriteU8(0x66);
	WriteU8(0xC7);

	uint8_t modrm = (reg & 0b111) << 3;

	WriteU8(reg);
	WriteU16(imm);

	instrs_compiled++;
}

void JIT::EmitMovRegAddress8(int reg, uint8_t imm)
{
	WriteU8(0xC6);

	uint8_t modrm = (reg & 0b111) << 3;

	WriteU8(reg);
	WriteU8(imm);

	instrs_compiled++;
}

void JIT::EmitPushf()
{
	WriteU8(0x9C);

	instrs_compiled++;
}

void JIT::EmitPopReg(int reg)
{
	WriteU8(0x58 + reg);

	instrs_compiled++;
}

void JIT::EmitPushReg(int reg)
{
	WriteU8(0x50 + reg);

	instrs_compiled++;
}

void JIT::EmitAndAlImm(uint8_t imm)
{
	WriteU8(0x24);
	WriteU8(imm);

	instrs_compiled++;
}

void JIT::EmitMovR8RegPointer(int reg, int reg_pointer)
{
	WriteU8(0x8A);

	uint8_t modrm = 0;
	modrm |= (reg & 0b111) << 3;
	modrm |= (reg_pointer & 0b111);

	WriteU8(modrm);

	instrs_compiled++;
}

void JIT::EmitMovRegReg(int reg_dest, int reg_src)
{
	WriteU8(0x48);
	WriteU8(0x89);

	uint8_t modrm = (0b11 << 6);
	modrm |= (reg_src << 3);
	modrm |= reg_dest;

	WriteU8(modrm);

	instrs_compiled++;
}

void JIT::EmitMovRegImm(int reg, uint32_t imm)
{
	WriteU8(0x48);
	WriteU8(0xC7);

	uint8_t modrm = (0b11 << 6);
	modrm |= reg;
	
	WriteU8(modrm);
	WriteU32(imm);

	instrs_compiled++;
}

void JIT::EmitLoadAddress(const void* ptr)
{
	WriteU8(0x48);
	WriteU8(0xB8);

	WriteU64(reinterpret_cast<size_t>(ptr));

	instrs_compiled++;
}

void JIT::EmitLdHlU16(uint16_t u16)
{
	printf("ld hl, 0x%04x\n", u16);

	EmitLea(HostRegisters::RAX, offsetof(State, h));
	EmitMovRegAddress8(HostRegisters::RAX, (u16 >> 8));
	EmitLea(HostRegisters::RAX, offsetof(State, l));
	EmitMovRegAddress8(HostRegisters::RAX, (u16 & 0xff));
}

void JIT::EmitLdSpU16(uint16_t u16)
{
	printf("ld sp, 0x%04x\n", u16);

	EmitLea(HostRegisters::RAX, offsetof(State, sp));
	EmitMovRegAddress16(HostRegisters::RAX, u16);
}

void JIT::EmitLdHlA()
{
	EmitLea(HostRegisters::RAX, offsetof(State, h));
	EmitMovR8RegPointer(HostRegisters8::BH, HostRegisters::RAX);
	
	EmitLea(HostRegisters::RAX, offsetof(State, l));
	EmitMovR8RegPointer(HostRegisters8::BL, HostRegisters::RAX);

	EmitMovRegReg(HostRegisters::RDI, HostRegisters::RBX);
	EmitMovRegImm(HostRegisters::RBX, 0);

	EmitLea(HostRegisters::RAX, offsetof(State, a));
	EmitMovR8RegPointer(HostRegisters8::BL, HostRegisters::RAX);

	EmitMovRegReg(HostRegisters::RSI, HostRegisters::RBX);
}

void JIT::EmitXorA()
{
	EmitLea(HostRegisters::RAX, offsetof(State, a));
	EmitMovRegAddress8(HostRegisters::RAX, 0);

	EmitLea(HostRegisters::RAX, offsetof(State, f));
	EmitMovRegAddress8(HostRegisters::RAX, 0x40);
}

bool JIT::CompileBlock(uint32_t start)
{
	JitBlock* block = new JitBlock();
	block->code = free_base;
	block->guest_addr = start;

	instrs_compiled = 0;

	uint32_t cur = start;
	uint8_t op = guest_code[cur++];
	uint32_t free_off = 0;

	EmitPushReg(HostRegisters::RSP);

	for (int i = 0; i < HostRegisters::HostRegCount; i++)
		if (i != HostRegisters::RSP)
			EmitPushReg(i);

	EmitLoadAddress(reinterpret_cast<const void*>(&g_state));

	while (!DoesOpcodeModifyPC(op) && instrs_compiled < 50)
	{
	 	switch (static_cast<Instruction>(op))
	 	{
	 	// case Instruction::ld_hl_u16:
	 	// 	EmitLdHlU16(*(uint16_t*)&guest_code[cur]);
	 	// 	cur += 2;
	 	// 	break;
	 	// case Instruction::ld_sp_u16:
	 	//  	EmitLdSpU16(*(uint16_t*)&guest_code[cur]);
	 	//  	cur += 2;
	 	//  	break;
	 	// case Instruction::ld_hl_minus_a:
	 	// 	EmitLdHlA();
	 	// 	break;
	 	// case Instruction::xor_a:
	 	// 	EmitXorA();
	 	// 	break;
	 	default:
	 		printf("Unknown opcode 0x%02x\n", op);
	 		goto cleanup;
	 	}

	 	op = guest_code[cur++];
	}

cleanup:
	for (int i = HostRegisters::RDI; i >= 0; i--)
		if (i != HostRegisters::RSP)
			EmitPopReg(i);
	
	EmitPopReg(HostRegisters::RSP);

	EmitMovRegImm(HostRegisters::RAX, 0x12345678);

	WriteU8(0xC3);

	HostFunc entry = (HostFunc)block->code;
	int exit_code = entry();

	std::ofstream file("out.bin");

	for (int i = 0; i < (free_base - block->code); i++)
	{
		file << block->code[i];
	}

	file.close();

	printf("Compiled %d blocks of code (result = 0x%x)\n", cur - start, exit_code);
	printf("State: 0x%04x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", g_state.sp,  g_state.a,  g_state.f,  g_state.b,  g_state.c,  g_state.d,  g_state.e,  g_state.h,  g_state.l);

	return true;
}