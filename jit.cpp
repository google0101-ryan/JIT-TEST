#include "jit.h"
#include "instructions.h"
#include <bus.h>
#include <sys/mman.h>

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <errno.h>
#include <fstream>
#include <libexplain/mmap.h>

State g_state;

JIT::JIT()
{
	base = (uint8_t*)mmap(nullptr, 0xffffffff, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (base == MAP_FAILED)
	{
		printf("Error allocating address space for JIT (%s)\n", explain_errno_mmap(errno, nullptr, 0xffffffff, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
		exit(1);
	}

	free_base = base;
	code_used = 0;
}

JIT::~JIT()
{
	if (base)
		munmap(base, 0xffffffff);
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

void JIT::EmitCallRax()
{
	WriteU8(0xFF);
	WriteU8(0xD0);
}

void JIT::EmitIncAtAddr(int reg)
{
	WriteU8(0x66);
	WriteU8(0xFF);
	WriteU8(0 | reg);
}

void JIT::EmitAddAtAddr(int reg, uint16_t imm)
{
	WriteU8(0x66);
	WriteU8(0x83);
	WriteU8(0 | reg);
	WriteU8(imm);
}

void JIT::EmitDec(int reg)
{
	WriteU8(0x48);
	WriteU8(0xFF);
	uint8_t modrm = 0xC8;
	modrm |= reg;

	WriteU8(modrm);
}

void JIT::EmitMovPtrReg8(int ptr_reg, int reg)
{
	WriteU8(0x88);
	uint8_t modrm = 0;
	modrm |= (reg << 3);
	modrm |= (ptr_reg);
	WriteU8(modrm);
}

void JIT::EmitJNZ(size_t off)
{
	WriteU8(0x75);
	WriteU8(off);
}

void JIT::EmitLdHlU16(uint16_t u16)
{
	//printf("ld hl, 0x%04x\n", u16);

	EmitLea(HostRegisters::RAX, offsetof(State, h));
	EmitMovRegAddress8(HostRegisters::RAX, (u16 >> 8));
	EmitLea(HostRegisters::RAX, offsetof(State, l));
	EmitMovRegAddress8(HostRegisters::RAX, (u16 & 0xff));
}

void JIT::EmitLdSpU16(uint16_t u16)
{
	//printf("ld sp, 0x%04x\n", u16);

	EmitLea(HostRegisters::RAX, offsetof(State, sp));
	EmitMovRegAddress16(HostRegisters::RAX, u16);
}

void JIT::EmitLdHlA()
{
	//printf("ld (hl-), a\n");

	WriteU8(0x90);

	EmitLea(HostRegisters::RAX, offsetof(State, a));
	EmitMovR8RegPointer(HostRegisters8::BL, HostRegisters::RAX);

	EmitLea(HostRegisters::RAX, offsetof(State, h));
	EmitMovR8RegPointer(HostRegisters8::BH, HostRegisters::RAX);
	
	EmitLea(HostRegisters::RAX, offsetof(State, l));
	EmitMovR8RegPointer(HostRegisters8::BL, HostRegisters::RAX);

	EmitMovRegReg(HostRegisters::RDI, HostRegisters::RBX);
	EmitMovRegImm(HostRegisters::RBX, 0);

	EmitMovRegReg(HostRegisters::RSI, HostRegisters::RBX);

	EmitLoadAddress(reinterpret_cast<const void*>(Bus::Write8));
	EmitCallRax();

	EmitDec(HostRegisters::RDI);
	EmitMovRegReg(HostRegisters::RBX, HostRegisters::RDI);

	EmitLea(HostRegisters::RAX, offsetof(State, h));
	EmitMovPtrReg8(HostRegisters::RAX, HostRegisters8::BH);

	EmitLea(HostRegisters::RAX, offsetof(State, l));
	EmitMovPtrReg8(HostRegisters::RAX, HostRegisters8::BL);
}

void JIT::EmitXorA()
{
	EmitLea(HostRegisters::RAX, offsetof(State, a));
	EmitMovRegAddress8(HostRegisters::RAX, 0);

	EmitLea(HostRegisters::RAX, offsetof(State, f));
	EmitMovRegAddress8(HostRegisters::RAX, 0x40);
}

void JIT::EmitIncPC()
{
	EmitLea(HostRegisters::RAX, offsetof(State, pc));
	EmitIncAtAddr(HostRegisters::RAX);
}

enum class InstrFlagType : uint32_t
{
	BIT = 0,
};

enum Flags
{
	CF = (1 << 4),
	HC = (1 << 5),
	SF = (1 << 6),
	ZF = (1 << 7)
};

void SetFlag(Flags f, bool t)
{
	if (t)
		g_state.f |= f;
	else
		g_state.f &= ~f;
}

static void CalculateFlags(uint8_t result, InstrFlagType flag_type)
{
	switch (flag_type)
	{
	case InstrFlagType::BIT:
		SetFlag(ZF, result == 0);
		SetFlag(SF, false);
		SetFlag(HC, true);
		break;
	default:
		printf("Unknown flag type %d\n", (int)flag_type);
		exit(1);
	}
}

void JIT::EmitBit7H()
{
	//printf("bit 7, h\n");

	EmitLea(HostRegisters::RAX, offsetof(State, h));
	EmitMovRegImm(HostRegisters::RBX, 0);
	EmitMovR8RegPointer(HostRegisters8::BL, HostRegisters::RAX);

	EmitMovRegReg(HostRegisters::RAX, HostRegisters::RBX);
	EmitAndAlImm((1 << 7));
	
	EmitMovRegReg(HostRegisters::RDI, HostRegisters::RAX);
	EmitMovRegImm(HostRegisters::RSI, static_cast<uint32_t>(InstrFlagType::BIT));

	EmitLoadAddress(reinterpret_cast<void*>(CalculateFlags));
	EmitCallRax();
}

void JIT::EmitJrNzi8(int8_t imm)
{
	//printf("jr nz, %d\n", imm);

	EmitLea(HostRegisters::RAX, offsetof(State, f));
	EmitMovRegImm(HostRegisters::RBX, 0);
	EmitMovR8RegPointer(HostRegisters8::BL, HostRegisters::RAX);

	EmitMovRegReg(HostRegisters::RAX, HostRegisters::RBX);
	EmitAndAlImm(ZF);
	EmitJNZ(8);

	EmitLea(HostRegisters::RAX, offsetof(State, pc));
	EmitAddAtAddr(HostRegisters::RAX, (int16_t)imm);
}

void JIT::CompileBlock(HostFunc& func)
{
	for (auto b : blocks)
		if (b->guest_addr == g_state.pc)
		{
			func = (HostFunc)b->code;
			return;
		}

	JitBlock* block = new JitBlock();
	block->code = free_base;
	block->guest_addr = g_state.pc;
	blocks.push_back(block);

	instrs_compiled = 0;

	uint32_t cur = g_state.pc;
	uint8_t op;
	uint32_t free_off = 0;
	uint8_t* b = free_base;

	for (int i = 0; i < HostRegisters::HostRegCount; i++)
		if (i != HostRegisters::RSP)
			EmitPushReg(i);

	EmitLoadAddress(reinterpret_cast<const void*>(&g_state));
	EmitMovRegReg(HostRegisters::RBP, HostRegisters::RAX);

	do
	{
	 	op = Bus::Read8(cur++);
		EmitIncPC();

	 	switch (static_cast<Instruction>(op))
	 	{
		case Instruction::jr_nz_i8:
			EmitIncPC();
			EmitJrNzi8(Bus::Read8(cur++));
			break;
	 	case Instruction::ld_hl_u16:
			EmitIncPC();
			EmitIncPC();
	 	 	EmitLdHlU16(Bus::Read16(cur));
	 	 	cur += 2;
	 	 	break;
	 	case Instruction::ld_sp_u16:
			EmitIncPC();
			EmitIncPC();
	 	  	EmitLdSpU16(Bus::Read16(cur));
	 	  	cur += 2;
	 	  	break;
	 	case Instruction::ld_hl_minus_a:
	 	 	EmitLdHlA();
	 	 	break;
	 	case Instruction::xor_a:
	 	 	EmitXorA();
	 	 	break;
		case Instruction::prefix_cb:
		{
			op = Bus::Read8(cur++);
			EmitIncPC();
			switch (static_cast<CBInstructions>(op))
			{
			case CBInstructions::bit_7_h:
				EmitBit7H();
				break;
			default:
				printf("Unknown opcode 0xcb 0x%02x (0x%04x)\n", op, cur - 1);
				exit(1);
			}
			break;
		}
	 	default:
	 		printf("Unknown opcode 0x%02x (0x%04x)\n", op, cur - 1);
	 		exit(1);
	 	}
	} while (!DoesOpcodeModifyPC(op) && instrs_compiled < 50);

	for (int i = HostRegisters::RDI; i >= 0; i--)
		if (i != HostRegisters::RSP)
			EmitPopReg(i);

	EmitMovRegImm(HostRegisters::RAX, 0);

	WriteU8(0xC3);

	std::ofstream file("out.bin");

	for (uint32_t i = 0; i < (free_base - block->code); i++)
	{
		file << b[i];
	}

	file.close();

	func = (HostFunc)block->code;
}