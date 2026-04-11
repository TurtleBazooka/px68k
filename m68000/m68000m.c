/******************************************************************************

	m68000m.c

	M68000 CPUインタフェース関数 for MUSASHI and c68k

******************************************************************************/
#include "irqh.h"
#include "prop.h"
#include "m68000.h"
#include "../x68k/x68kmemory.h"

#include "c68k/c68k.h"
#include "Musashi/m68k.h"
#include "Musashi/m68kcpu.h"

int32_t m68000_ICountBk;
int32_t ICount;

/******************************************************************************
	M68000インタフェース関数 for MUSASHI
******************************************************************************/

//==== for Memory Access ====
unsigned int m68k_read_memory_8( unsigned int address){ return cpu_readmem24      (address); }
unsigned int m68k_read_memory_16(unsigned int address){ return cpu_readmem24_word (address); }
unsigned int m68k_read_memory_32(unsigned int address){ return cpu_readmem24_dword(address); }

void m68k_write_memory_8( unsigned int address, unsigned int data){ cpu_writemem24      (address, data); }
void m68k_write_memory_16(unsigned int address, unsigned int data){ cpu_writemem24_word (address, data); }
void m68k_write_memory_32(unsigned int address, unsigned int data){ cpu_writemem24_dword(address, data); }

/*--------------------------------------------------------
	CPU初期化 for c68k
--------------------------------------------------------*/
void c68k_init(void)
{
    C68k_Init(&C68K, my_irqh_callback);

    C68k_Set_ReadB(&C68K, Memory_ReadB);
    C68k_Set_ReadW(&C68K, Memory_ReadW);
    C68k_Set_WriteB(&C68K, Memory_WriteB);
    C68k_Set_WriteW(&C68K, Memory_WriteW);

        C68k_Set_Fetch(&C68K, 0x000000, 0xbfffff, (uintptr_t)MEM);
        C68k_Set_Fetch(&C68K, 0xc00000, 0xc7ffff, (uintptr_t)GVRAM);
        C68k_Set_Fetch(&C68K, 0xe00000, 0xe7ffff, (uintptr_t)TVRAM);
        C68k_Set_Fetch(&C68K, 0xea0000, 0xea1fff, (uintptr_t)SCSIIPL);
        C68k_Set_Fetch(&C68K, 0xed0000, 0xed3fff, (uintptr_t)SRAM);
        C68k_Set_Fetch(&C68K, 0xf00000, 0xfbffff, (uintptr_t)FONT);
        C68k_Set_Fetch(&C68K, 0xfc0000, 0xffffff, (uintptr_t)IPL);
}

/*--------------------------------------------------------
	CPU初期化 for MUSASHI & c68k
--------------------------------------------------------*/
void m68000_init(void)
{
	c68k_init();// c68k Emu

    m68k_set_cpu_type(M68K_CPU_TYPE_68000);// Musashi Emu
    m68k_init();

}


/*--------------------------------------------------------
	CPUリセット for MUSASHI & c68k
--------------------------------------------------------*/

void m68000_reset(void)
{
	C68k_Reset(&C68K);// c68k Emu
	m68k_pulse_reset();// Musashi Emu
}


/*--------------------------------------------------------
	CPU停止 (none)
--------------------------------------------------------*/

void m68000_exit(void)
{
}


/*--------------------------------------------------------
	CPU実行 for MUSASHI or c68k
--------------------------------------------------------*/

int32_t m68000_execute(int32_t cycles)
{
	switch(Config.CPU_Emu)
	{
	case 0:// c68k Emu
	 return C68k_Exec(&C68K, cycles);
	 break;
	case 1:// Musashi Emu
	default:
	 return m68k_execute(cycles);
	 break;
	}
}


/*--------------------------------------------------------
	割り込み処理 for MUSASHI or c68k
--------------------------------------------------------*/

void m68000_set_irq_line(int32_t irqline)
{
	switch(Config.CPU_Emu)
	{
	case 0:// c68k Emu
	 C68k_Set_IRQ(&C68K, irqline);
	 break;
	case 1:
	default:
	 m68k_set_irq(irqline);
	 break;
	}

}


/*--------------------------------------------------------
	割り込みコールバック関数設定
--------------------------------------------------------*/

void m68000_set_irq_callback(int32_t (*callback)(int32_t line))
{
//	C68k_Set_IRQ_Callback(&C68K, callback);
}


/*--------------------------------------------------------
	レジスタ取得 for MUSASHI or c68k
--------------------------------------------------------*/

uint32_t m68000_get_reg(int32_t regnum)
{
	switch(Config.CPU_Emu)
	{
	case 0:// c68k Emu
		switch (regnum)
		{

		case M68K_PC:  return C68k_Get_PC(&C68K);
		case M68K_USP: return C68k_Get_USP(&C68K);
		case M68K_MSP: return C68k_Get_MSP(&C68K);
		case M68K_SR:  return C68k_Get_SR(&C68K);
		case M68K_D0:  return C68k_Get_DReg(&C68K, 0);
		case M68K_D1:  return C68k_Get_DReg(&C68K, 1);
		case M68K_D2:  return C68k_Get_DReg(&C68K, 2);
		case M68K_D3:  return C68k_Get_DReg(&C68K, 3);
		case M68K_D4:  return C68k_Get_DReg(&C68K, 4);
		case M68K_D5:  return C68k_Get_DReg(&C68K, 5);
		case M68K_D6:  return C68k_Get_DReg(&C68K, 6);
		case M68K_D7:  return C68k_Get_DReg(&C68K, 7);
		case M68K_A0:  return C68k_Get_AReg(&C68K, 0);
		case M68K_A1:  return C68k_Get_AReg(&C68K, 1);
		case M68K_A2:  return C68k_Get_AReg(&C68K, 2);
		case M68K_A3:  return C68k_Get_AReg(&C68K, 3);
		case M68K_A4:  return C68k_Get_AReg(&C68K, 4);
		case M68K_A5:  return C68k_Get_AReg(&C68K, 5);
		case M68K_A6:  return C68k_Get_AReg(&C68K, 6);
		case M68K_A7:  return C68k_Get_AReg(&C68K, 7);

		default: return 0;
		}
	break;
	case 1:// Musashi Emu
	default:
		switch (regnum)
		{

		case M68K_PC:  return m68k_get_reg(NULL, M68K_REG_PC);
		case M68K_USP: return m68k_get_reg(NULL, M68K_REG_USP);
		case M68K_MSP: return m68k_get_reg(NULL, M68K_REG_MSP);
		case M68K_SR:  return m68k_get_reg(NULL, M68K_REG_SR);
		case M68K_D0:  return m68k_get_reg(NULL, M68K_REG_D0);
		case M68K_D1:  return m68k_get_reg(NULL, M68K_REG_D1);
		case M68K_D2:  return m68k_get_reg(NULL, M68K_REG_D2);
		case M68K_D3:  return m68k_get_reg(NULL, M68K_REG_D3);
		case M68K_D4:  return m68k_get_reg(NULL, M68K_REG_D4);
		case M68K_D5:  return m68k_get_reg(NULL, M68K_REG_D5);
		case M68K_D6:  return m68k_get_reg(NULL, M68K_REG_D6);
		case M68K_D7:  return m68k_get_reg(NULL, M68K_REG_D7);
		case M68K_A0:  return m68k_get_reg(NULL, M68K_REG_A0);
		case M68K_A1:  return m68k_get_reg(NULL, M68K_REG_A1);
		case M68K_A2:  return m68k_get_reg(NULL, M68K_REG_A2);
		case M68K_A3:  return m68k_get_reg(NULL, M68K_REG_A3);
		case M68K_A4:  return m68k_get_reg(NULL, M68K_REG_A4);
		case M68K_A5:  return m68k_get_reg(NULL, M68K_REG_A5);
		case M68K_A6:  return m68k_get_reg(NULL, M68K_REG_A6);
		case M68K_A7:  return m68k_get_reg(NULL, M68K_REG_A7);

		default: return 0;
		}
	break;
	}

}


/*--------------------------------------------------------
	レジスタ設定 for MUSASHI or c68k
--------------------------------------------------------*/

void m68000_set_reg(int32_t regnum, uint32_t val)
{
	switch(Config.CPU_Emu)
	{
	case 0:// c68k Emu
		switch (regnum)
		{

		case M68K_PC:  C68k_Set_PC(&C68K, val); break;
		case M68K_USP: C68k_Set_USP(&C68K, val); break;
		case M68K_MSP: C68k_Set_MSP(&C68K, val); break;
		case M68K_SR:  C68k_Set_SR(&C68K, val); break;
		case M68K_D0:  C68k_Set_DReg(&C68K, 0, val); break;
		case M68K_D1:  C68k_Set_DReg(&C68K, 1, val); break;
		case M68K_D2:  C68k_Set_DReg(&C68K, 2, val); break;
		case M68K_D3:  C68k_Set_DReg(&C68K, 3, val); break;
		case M68K_D4:  C68k_Set_DReg(&C68K, 4, val); break;
		case M68K_D5:  C68k_Set_DReg(&C68K, 5, val); break;
		case M68K_D6:  C68k_Set_DReg(&C68K, 6, val); break;
		case M68K_D7:  C68k_Set_DReg(&C68K, 7, val); break;
		case M68K_A0:  C68k_Set_AReg(&C68K, 0, val); break;
		case M68K_A1:  C68k_Set_AReg(&C68K, 1, val); break;
		case M68K_A2:  C68k_Set_AReg(&C68K, 2, val); break;
		case M68K_A3:  C68k_Set_AReg(&C68K, 3, val); break;
		case M68K_A4:  C68k_Set_AReg(&C68K, 4, val); break;
		case M68K_A5:  C68k_Set_AReg(&C68K, 5, val); break;
		case M68K_A6:  C68k_Set_AReg(&C68K, 6, val); break;
		case M68K_A7:  C68k_Set_AReg(&C68K, 7, val); break;

		default: break;
		}
	break;
	case 1:// Musashi Emu
	default:
		switch (regnum)
		{

		case M68K_PC:  m68k_set_reg(M68K_REG_PC, val); break;
		case M68K_USP: m68k_set_reg(M68K_REG_USP, val); break;
		case M68K_MSP: m68k_set_reg(M68K_REG_MSP, val); break;
		case M68K_SR:  m68k_set_reg(M68K_REG_SR, val); break;
		case M68K_D0:  m68k_set_reg(M68K_REG_D0, val); break;
		case M68K_D1:  m68k_set_reg(M68K_REG_D1, val); break;
		case M68K_D2:  m68k_set_reg(M68K_REG_D2, val); break;
		case M68K_D3:  m68k_set_reg(M68K_REG_D3, val); break;
		case M68K_D4:  m68k_set_reg(M68K_REG_D4, val); break;
		case M68K_D5:  m68k_set_reg(M68K_REG_D5, val); break;
		case M68K_D6:  m68k_set_reg(M68K_REG_D6, val); break;
		case M68K_D7:  m68k_set_reg(M68K_REG_D7, val); break;
		case M68K_A0:  m68k_set_reg(M68K_REG_A0, val); break;
		case M68K_A1:  m68k_set_reg(M68K_REG_A1, val); break;
		case M68K_A2:  m68k_set_reg(M68K_REG_A2, val); break;
		case M68K_A3:  m68k_set_reg(M68K_REG_A3, val); break;
		case M68K_A4:  m68k_set_reg(M68K_REG_A4, val); break;
		case M68K_A5:  m68k_set_reg(M68K_REG_A5, val); break;
		case M68K_A6:  m68k_set_reg(M68K_REG_A6, val); break;
		case M68K_A7:  m68k_set_reg(M68K_REG_A7, val); break;

		default: break;
		}
	break;
	}

}


/*------------------------------------------------------
	セーブ/ロード ステート
------------------------------------------------------*/

#ifdef SAVE_STATE

STATE_SAVE( m68000 )
{
	int i;
	uint32_t pc = m68000_get_reg(M68K_PC);

	for (i = M68K_D0; i <= M68K_A7; i++)
		state_save_long(m68000_get_reg(i), 1);

	state_save_long(&C68K.flag_C, 1);
	state_save_long(&C68K.flag_V, 1);
	state_save_long(&C68K.flag_Z, 1);
	state_save_long(&C68K.flag_N, 1);
	state_save_long(&C68K.flag_X, 1);
	state_save_long(&C68K.flag_I, 1);
	state_save_long(&C68K.flag_S, 1);
	state_save_long(&C68K.USP, 1);
	state_save_long(&pc, 1);
	state_save_long(&C68K.HaltState, 1);
	state_save_long(&C68K.IRQLine, 1);
	state_save_long(&C68K.IRQState, 1);
}

STATE_LOAD( m68000 )
{
	int i;
	uint32_t dat;

	for (i = M68K_D0; i <= M68K_A7; i++)
	{
		state_load_long(dat, 1);
		m68000_set_reg(i,dat);
	}

	state_load_long(&C68K.flag_C, 1);
	state_load_long(&C68K.flag_V, 1);
	state_load_long(&C68K.flag_Z, 1);
	state_load_long(&C68K.flag_N, 1);
	state_load_long(&C68K.flag_X, 1);
	state_load_long(&C68K.flag_I, 1);
	state_load_long(&C68K.flag_S, 1);
	state_load_long(&C68K.USP, 1);
	state_load_long(&pc, 1);
	state_load_long(&C68K.HaltState, 1);
	state_load_long(&C68K.IRQLine, 1);
	state_load_long(&C68K.IRQState, 1);

	m68000_set_reg(M68K_PC, pc);
}

#endif /* SAVE_STATE */
