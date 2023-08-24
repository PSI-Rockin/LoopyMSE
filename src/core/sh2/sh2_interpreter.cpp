#include <cassert>
#include <cstdio>
#include "core/sh2/sh2_interpreter.h"
#include "core/sh2/sh2_local.h"

namespace SH2::Interpreter
{

#define GET_T() (sh2.sr & 0x1)

#define SET_T(x) do {    \
	sh2.sr &= ~0x1;      \
	sh2.sr |= (x) & 0x1; \
} while (0);

static void handle_jump(uint32_t dst, bool delay_slot)
{
	//TODO: raise an exception if this function is called within a delay slot
	
	//Small hack: if in a delay slot, immediately execute the next instruction
	if (delay_slot)
	{
		sh2.pc += 2;

		uint16_t instr = read16(sh2.pc - 4);
		run(instr);
	}
	
	sh2.pc = dst + 2;
}

static uint32_t get_control_reg(int index)
{
	switch (index)
	{
	case 0:
		return sh2.sr;
	case 1:
		return sh2.gbr;
	case 2:
		return sh2.vbr;
	default:
		assert(0);
		return 0;
	}
}

static void set_control_reg(int index, uint32_t value)
{
	switch (index)
	{
	case 0:
		sh2.sr = value;
		break;
	case 1:
		sh2.gbr = value;
		break;
	case 2:
		sh2.vbr = value;
		break;
	default:
		assert(0);
	}
}

static uint32_t get_system_reg(int index)
{
	switch (index)
	{
	case 0:
		return sh2.mach;
	case 1:
		return sh2.macl;
	case 2:
		return sh2.pr;
	default:
		assert(0);
		return 0;
	}
}

static void set_system_reg(int index, uint32_t value)
{
	switch (index)
	{
	case 0:
		sh2.mach = value;
		break;
	case 1:
		sh2.macl = value;
		break;
	case 2:
		sh2.pr = value;
		break;
	default:
		assert(0);
	}
}

//Data transfer instructions

static void mov_imm(uint16_t instr)
{
	int32_t imm = (int32_t)(int8_t)(instr & 0xFF);
	uint32_t reg = (instr >> 8) & 0xF;

	sh2.gpr[reg] = imm;
}

static void movw_pcrel_reg(uint16_t instr)
{
	uint32_t offs = (instr & 0xFF) << 1;
	uint32_t reg = (instr >> 8) & 0xF;

	sh2.gpr[reg] = (int32_t)(int16_t)read16(sh2.pc + offs);
}

static void movl_pcrel_reg(uint16_t instr)
{
	uint32_t offs = (instr & 0xFF) << 2;
	uint32_t reg = (instr >> 8) & 0xF;

	sh2.gpr[reg] = read32((sh2.pc & ~0x3) + offs);
}

static void mov_reg_reg(uint16_t instr)
{
	uint32_t src = (instr >> 4) & 0xF;
	uint32_t dst = (instr >> 8) & 0xF;

	sh2.gpr[dst] = sh2.gpr[src];
}

static void movb_reg_mem(uint16_t instr)
{
	uint32_t reg = (instr >> 4) & 0xF;
	uint32_t mem = (instr >> 8) & 0xF;

	write8(sh2.gpr[mem], sh2.gpr[reg]);
}

static void movw_reg_mem(uint16_t instr)
{
	uint32_t reg = (instr >> 4) & 0xF;
	uint32_t mem = (instr >> 8) & 0xF;

	write16(sh2.gpr[mem], sh2.gpr[reg]);
}

static void movl_reg_mem(uint16_t instr)
{
	uint32_t reg = (instr >> 4) & 0xF;
	uint32_t mem = (instr >> 8) & 0xF;

	write32(sh2.gpr[mem], sh2.gpr[reg]);
}

static void movw_mem_reg(uint16_t instr)
{
	uint32_t mem = (instr >> 4) & 0xF;
	uint32_t reg = (instr >> 8) & 0xF;

	sh2.gpr[reg] = (int32_t)(int16_t)read16(sh2.gpr[mem]);
}

static void movl_mem_reg(uint16_t instr)
{
	uint32_t mem = (instr >> 4) & 0xF;
	uint32_t reg = (instr >> 8) & 0xF;

	sh2.gpr[reg] = read32(sh2.gpr[mem]);
}

static void movl_reg_mem_dec(uint16_t instr)
{
	uint32_t reg = (instr >> 4) & 0xF;
	uint32_t mem = (instr >> 8) & 0xF;

	sh2.gpr[mem] -= 4;
	write32(sh2.gpr[mem], sh2.gpr[reg]);
}

static void movw_mem_reg_inc(uint16_t instr)
{
	uint32_t mem = (instr >> 4) & 0xF;
	uint32_t reg = (instr >> 8) & 0xF;

	sh2.gpr[reg] = (int32_t)(int16_t)read16(sh2.gpr[mem]);
	sh2.gpr[mem] += 2;
}

static void movl_mem_reg_inc(uint16_t instr)
{
	uint32_t mem = (instr >> 4) & 0xF;
	uint32_t reg = (instr >> 8) & 0xF;

	sh2.gpr[reg] = read32(sh2.gpr[mem]);
	sh2.gpr[mem] += 4;
}

static void movw_reg_memrel(uint16_t instr)
{
	uint32_t offs = (instr & 0xF) << 1;
	uint32_t mem = (instr >> 4) & 0xF;

	write16(sh2.gpr[mem] + offs, sh2.gpr[0]);
}

static void movl_reg_memrel(uint16_t instr)
{
	uint32_t offs = (instr & 0xF) << 2;
	uint32_t reg = (instr >> 4) & 0xF;
	uint32_t mem = (instr >> 8) & 0xF;

	write32(sh2.gpr[mem] + offs, sh2.gpr[reg]);
}

static void movw_memrel_reg(uint16_t instr)
{
	uint32_t offs = (instr & 0xF) << 1;
	uint32_t mem = (instr >> 4) & 0xF;

	sh2.gpr[0] = (int32_t)(int16_t)read16(sh2.gpr[mem] + offs);
}

static void movl_memrel_reg(uint16_t instr)
{
	uint32_t offs = (instr & 0xF) << 2;
	uint32_t mem = (instr >> 4) & 0xF;
	uint32_t reg = (instr >> 8) & 0xF;

	sh2.gpr[reg] = read32(sh2.gpr[mem] + offs);
}

static void movb_reg_gbrrel(uint16_t instr)
{
	uint32_t offs = instr & 0xFF;
	write8(sh2.gbr + offs, sh2.gpr[0]);
}

static void movw_reg_gbrrel(uint16_t instr)
{
	uint32_t offs = (instr & 0xFF) << 1;
	write16(sh2.gbr + offs, sh2.gpr[0]);
}

static void movb_gbrrel_reg(uint16_t instr)
{
	uint32_t offs = instr & 0xFF;
	sh2.gpr[0] = (int32_t)(int8_t)read8(sh2.gbr + offs);
}

static void movw_gbrrel_reg(uint16_t instr)
{
	uint32_t offs = (instr & 0xFF) << 1;
	sh2.gpr[0] = (int32_t)(int16_t)read16(sh2.gbr + offs);
}

static void mova(uint16_t instr)
{
	uint32_t offs = (instr & 0xFF) << 2;
	sh2.gpr[0] = offs + (sh2.pc & ~0x3);
}

//Arithmetic instructions

static void add_reg(uint16_t instr)
{
	uint32_t src = (instr >> 4) & 0xF;
	uint32_t dst = (instr >> 8) & 0xF;

	sh2.gpr[dst] += sh2.gpr[src];
}

static void add_imm(uint16_t instr)
{
	int32_t imm = (int32_t)(int8_t)(instr & 0xFF);
	uint32_t reg = (instr >> 8) & 0xF;

	sh2.gpr[reg] += imm;
}

static void cmpeq_imm(uint16_t instr)
{
	uint32_t imm = instr & 0xFF;

	bool result = sh2.gpr[0] == imm;
	SET_T(result);
}

static void cmpeq_reg(uint16_t instr)
{
	uint32_t reg1 = (instr >> 4) & 0xF;
	uint32_t reg2 = (instr >> 8) & 0xF;

	bool result = sh2.gpr[reg2] == sh2.gpr[reg1];
	SET_T(result);
}

static void cmphs(uint16_t instr)
{
	uint32_t reg1 = (instr >> 4) & 0xF;
	uint32_t reg2 = (instr >> 8) & 0xF;

	bool result = sh2.gpr[reg2] >= sh2.gpr[reg1];
	SET_T(result);
}

static void cmpge(uint16_t instr)
{
	uint32_t reg1 = (instr >> 4) & 0xF;
	uint32_t reg2 = (instr >> 8) & 0xF;

	bool result = (int32_t)sh2.gpr[reg2] >= (int32_t)sh2.gpr[reg1];
	SET_T(result);
}

static void cmppl(uint16_t instr)
{
	uint32_t reg = (instr >> 8) & 0xF;

	bool result = (int32_t)sh2.gpr[reg] > 0;
	SET_T(result);
}

static void extub(uint16_t instr)
{
	uint32_t src = (instr >> 4) & 0xF;
	uint32_t dst = (instr >> 8) & 0xF;

	sh2.gpr[dst] = sh2.gpr[src] & 0xFF;
}

static void extuw(uint16_t instr)
{
	uint32_t src = (instr >> 4) & 0xF;
	uint32_t dst = (instr >> 8) & 0xF;

	sh2.gpr[dst] = sh2.gpr[src] & 0xFFFF;
}

//Logic instructions

static void and_reg(uint16_t instr)
{
	uint32_t src = (instr >> 4) & 0xF;
	uint32_t dst = (instr >> 8) & 0xF;

	sh2.gpr[dst] &= sh2.gpr[src];
}

static void and_imm(uint16_t instr)
{
	uint32_t imm = instr & 0xFF;

	sh2.gpr[0] &= imm;
}

static void or_reg(uint16_t instr)
{
	uint32_t src = (instr >> 4) & 0xF;
	uint32_t dst = (instr >> 8) & 0xF;

	sh2.gpr[dst] |= sh2.gpr[src];
}

static void or_imm(uint16_t instr)
{
	uint32_t imm = instr & 0xFF;

	sh2.gpr[0] |= imm;
}

static void tst_reg(uint16_t instr)
{
	uint32_t reg1 = (instr >> 4) & 0xF;
	uint32_t reg2 = (instr >> 8) & 0xF;

	bool result = (sh2.gpr[reg1] & sh2.gpr[reg2]) == 0;
	SET_T(result);
}

static void tst_imm(uint16_t instr)
{
	uint32_t imm = instr & 0xFF;

	bool result = (sh2.gpr[0] & imm) == 0;
	SET_T(result);
}

//Shift instructions

static void shll(uint16_t instr)
{
	uint32_t reg = (instr >> 8) & 0xF;

	SET_T(sh2.gpr[reg] >> 31);
	sh2.gpr[reg] <<= 1;
}

static void shlr(uint16_t instr)
{
	uint32_t reg = (instr >> 8) & 0xF;

	SET_T(sh2.gpr[reg] & 0x1);
	sh2.gpr[reg] >>= 1;
}

static void shll2(uint16_t instr)
{
	uint32_t reg = (instr >> 8) & 0xF;
	sh2.gpr[reg] <<= 2;
}

static void shlr2(uint16_t instr)
{
	uint32_t reg = (instr >> 8) & 0xF;
	sh2.gpr[reg] >>= 2;
}

static void shll8(uint16_t instr)
{
	uint32_t reg = (instr >> 8) & 0xF;
	sh2.gpr[reg] <<= 8;
}

//Control flow instructions

static void bf(uint16_t instr)
{
	int32_t offs = (int32_t)(int8_t)(instr & 0xFF);
	offs <<= 1;

	uint32_t dst = sh2.pc + offs;
	if (!GET_T())
	{
		handle_jump(dst, false);
	}
}

static void bt(uint16_t instr)
{
	int32_t offs = (int32_t)(int8_t)(instr & 0xFF);
	offs <<= 1;

	uint32_t dst = sh2.pc + offs;
	if (GET_T())
	{
		handle_jump(dst, false);
	}
}

static void bra(uint16_t instr)
{
	int32_t offs = (instr & 0x7FF) | ((instr & 0x800) ? 0xFFFFF800 : 0);
	offs <<= 1;

	uint32_t dst = sh2.pc + offs;
	handle_jump(dst, true);
}

static void bsr(uint16_t instr)
{
	int32_t offs = (instr & 0x7FF) | ((instr & 0x800) ? 0xFFFFF800 : 0);
	offs <<= 1;

	sh2.pr = sh2.pc;
	uint32_t dst = sh2.pc + offs;
	handle_jump(dst, true);
}

static void jmp(uint16_t instr)
{
	uint32_t reg = (instr >> 8) & 0xF;
	handle_jump(sh2.gpr[reg], true);
}

static void jsr(uint16_t instr)
{
	uint32_t reg = (instr >> 8) & 0xF;
	sh2.pr = sh2.pc;
	handle_jump(sh2.gpr[reg], true);
}

static void rts(uint16_t instr)
{
	handle_jump(sh2.pr, true);
}

//System control instructions

static void ldc_reg(uint16_t instr)
{
	uint32_t index = (instr >> 4) & 0xF;
	uint32_t reg = (instr >> 8) & 0xF;

	set_control_reg(index, sh2.gpr[reg]);
}

static void ldcl_mem_inc(uint16_t instr)
{
	uint32_t reg = (instr >> 4) & 0xF;
	uint32_t mem = (instr >> 8) & 0xF;

	uint32_t value = read32(sh2.gpr[mem]);
	set_control_reg(reg, value);
	sh2.gpr[mem] += 4;
}

static void ldsl_mem_inc(uint16_t instr)
{
	uint32_t reg = (instr >> 4) & 0xF;
	uint32_t mem = (instr >> 8) & 0xF;

	uint32_t value = read32(sh2.gpr[mem]);
	set_system_reg(reg, value);
	sh2.gpr[mem] += 4;
}

static void stc_reg(uint16_t instr)
{
	uint32_t index = (instr >> 4) & 0xF;
	uint32_t reg = (instr >> 8) & 0xF;

	sh2.gpr[reg] = get_control_reg(index);
}

static void stsl_mem_dec(uint16_t instr)
{
	uint32_t reg = (instr >> 4) & 0xF;
	uint32_t mem = (instr >> 8) & 0xF;

	sh2.gpr[mem] -= 4;
	write32(sh2.gpr[mem], get_system_reg(reg));
}

void run(uint16_t instr)
{
	//TODO: convert the decoding into a compile-time generated LUT
	if ((instr & 0xF000) == 0xE000)
	{
		mov_imm(instr);
	}
	else if ((instr & 0xF000) == 0x9000)
	{
		movw_pcrel_reg(instr);
	}
	else if ((instr & 0xF000) == 0xD000)
	{
		movl_pcrel_reg(instr);
	}
	else if ((instr & 0xF00F) == 0x6003)
	{
		mov_reg_reg(instr);
	}
	else if ((instr & 0xF00F) == 0x2000)
	{
		movb_reg_mem(instr);
	}
	else if ((instr & 0xF00F) == 0x2001)
	{
		movw_reg_mem(instr);
	}
	else if ((instr & 0xF00F) == 0x2002)
	{
		movl_reg_mem(instr);
	}
	else if ((instr & 0xF00F) == 0x6001)
	{
		movw_mem_reg(instr);
	}
	else if ((instr & 0xF00F) == 0x6002)
	{
		movl_mem_reg(instr);
	}
	else if ((instr & 0xF00F) == 0x2006)
	{
		movl_reg_mem_dec(instr);
	}
	else if ((instr & 0xF00F) == 0x6005)
	{
		movw_mem_reg_inc(instr);
	}
	else if ((instr & 0xF00F) == 0x6006)
	{
		movl_mem_reg_inc(instr);
	}
	else if ((instr & 0xFF00) == 0x8100)
	{
		movw_reg_memrel(instr);
	}
	else if ((instr & 0xF000) == 0x1000)
	{
		movl_reg_memrel(instr);
	}
	else if ((instr & 0xFF00) == 0x8500)
	{
		movw_memrel_reg(instr);
	}
	else if ((instr & 0xF000) == 0x5000)
	{
		movl_memrel_reg(instr);
	}
	else if ((instr & 0xFF00) == 0xC000)
	{
		movb_reg_gbrrel(instr);
	}
	else if ((instr & 0xFF00) == 0xC100)
	{
		movw_reg_gbrrel(instr);
	}
	else if ((instr & 0xFF00) == 0xC400)
	{
		movb_gbrrel_reg(instr);
	}
	else if ((instr & 0xFF00) == 0xC500)
	{
		movw_gbrrel_reg(instr);
	}
	else if ((instr & 0xFF00) == 0xC700)
	{
		mova(instr);
	}
	else if ((instr & 0xF00F) == 0x300C)
	{
		add_reg(instr);
	}
	else if ((instr & 0xF000) == 0x7000)
	{
		add_imm(instr);
	}
	else if ((instr & 0xFF00) == 0x8800)
	{
		cmpeq_imm(instr);
	}
	else if ((instr & 0xF00F) == 0x3000)
	{
		cmpeq_reg(instr);
	}
	else if ((instr & 0xF00F) == 0x3002)
	{
		cmphs(instr);
	}
	else if ((instr & 0xF00F) == 0x3003)
	{
		cmpge(instr);
	}
	else if ((instr & 0xF0FF) == 0x4015)
	{
		cmppl(instr);
	}
	else if ((instr & 0xF00F) == 0x600C)
	{
		extub(instr);
	}
	else if ((instr & 0xF00F) == 0x600D)
	{
		extuw(instr);
	}
	else if ((instr & 0xF00F) == 0x2009)
	{
		and_reg(instr);
	}
	else if ((instr & 0xFF00) == 0xC900)
	{
		and_imm(instr);
	}
	else if ((instr & 0xF00F) == 0x200B)
	{
		or_reg(instr);
	}
	else if ((instr & 0xFF00) == 0xCB00)
	{
		or_imm(instr);
	}
	else if ((instr & 0xF00F) == 0x2008)
	{
		tst_reg(instr);
	}
	else if ((instr & 0xFF00) == 0xC800)
	{
		tst_imm(instr);
	}
	else if ((instr & 0xF0FF) == 0x4000)
	{
		shll(instr);
	}
	else if ((instr & 0xF0FF) == 0x4001)
	{
		shlr(instr);
	}
	else if ((instr & 0xF0FF) == 0x4008)
	{
		shll2(instr);
	}
	else if ((instr & 0xF0FF) == 0x4009)
	{
		shlr2(instr);
	}
	else if ((instr & 0xF0FF) == 0x4018)
	{
		shll8(instr);
	}
	else if ((instr & 0xFF00) == 0x8B00)
	{
		bf(instr);
	}
	else if ((instr & 0xFF00) == 0x8900)
	{
		bt(instr);
	}
	else if ((instr & 0xF000) == 0xA000)
	{
		bra(instr);
	}
	else if ((instr & 0xF000) == 0xB000)
	{
		bsr(instr);
	}
	else if ((instr & 0xF0FF) == 0x402B)
	{
		jmp(instr);
	}
	else if ((instr & 0xF0FF) == 0x400B)
	{
		jsr(instr);
	}
	else if (instr == 0x000B)
	{
		rts(instr);
	}
	else if ((instr & 0xF00F) == 0x400E)
	{
		ldc_reg(instr);
	}
	else if ((instr & 0xF00F) == 0x4007)
	{
		ldcl_mem_inc(instr);
	}
	else if ((instr & 0xF00F) == 0x4006)
	{
		ldsl_mem_inc(instr);
	}
	else if (instr == 0x0009)
	{
		//nop
	}
	else if ((instr & 0xF00F) == 0x0002)
	{
		stc_reg(instr);
	}
	else if ((instr & 0xF00F) == 0x4002)
	{
		stsl_mem_dec(instr);
	}
	else
	{
		printf("[SH2] unrecognized instr %04X at %08X\n", instr, sh2.pc - 4);
		assert(0);
	}
}

}