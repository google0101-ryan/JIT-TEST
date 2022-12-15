#pragma once

#include <cstdint>
#include <vector>

struct JitBlock
{
	uint8_t* code;
	uint32_t size;
	uint32_t guest_addr;
};

struct State
{
	uint16_t sp;
	uint8_t a, b, c, d, e, h, l;
	uint8_t f;
};

class JIT
{
private:
	std::vector<JitBlock*> blocks;
	uint8_t* base;
	uint8_t* free_base;
	uint8_t* guest_code;
	uint32_t code_used;

	uint32_t instrs_compiled = 0;

	bool DoesOpcodeModifyPC(uint8_t op);

	// High level emitter stuff
	void EmitLdHlU16(uint16_t u16); // 0x21
	void EmitLdSpU16(uint16_t u16); // 0x31
	void EmitLdHlA(); // 0x32
	void EmitXorA(); // 0xAF

	// Low level emitter stuff

	// Load a register with an address, offset from ebp
	// Used for accessing the state structure
	void EmitLea(int reg, uint8_t targetDisplacement);
	// Set the 16-bit value pointed to by reg to imm
	void EmitMovRegAddress16(int reg, uint16_t imm);
	// Set the 8-bit value pointed to by reg to imm
	void EmitMovRegAddress8(int reg, uint8_t imm);
	// Useful for transferring flags to and from the emulator
	void EmitPushf();
	// Pop a register off the stack
	void EmitPopReg(int reg);
	// Push a register onto the stack
	void EmitPushReg(int reg);
	// And AL with imm
	void EmitAndAlImm(uint8_t imm);
	// Set reg8 to memory address
	void EmitMovR8RegPointer(int reg, int reg_pointer);
	// Copy register contents to register
	void EmitMovRegReg(int reg_dest, int reg_src); 
	// Set register to value
	void EmitMovRegImm(int reg, uint32_t imm);
	// Set register to address of struct
	void EmitLoadAddress(const void* ptr);

	void WriteU8(uint8_t d) {*free_base++ = d;}
	void WriteU16(uint16_t d) {WriteU8(d & 0xff); WriteU8(d >> 8);}
	void WriteU32(uint32_t d) {WriteU16(d & 0xffff); WriteU16(d >> 16);}
	void WriteU64(uint64_t d) {WriteU32(d & 0xffffffff); WriteU32(d >> 32);}

	using HostFunc = int (*)();
public:
	JIT(uint8_t* code_buf);

	bool CompileBlock(uint32_t start);
};