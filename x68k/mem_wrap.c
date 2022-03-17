/*	$Id: mem_wrap.c,v 1.2 2003/12/05 18:07:19 nonaka Exp $	*/
/*	Add SCSI_iocs fuck address 2022/3/21 kameya */

#include "common.h"
//#include <string.h>
#include "../m68000/m68000.h"
#include "winx68k.h"

#include "adpcm.h"
#include "bg.h"
#include "crtc.h"
#include "dmac.h"
#include "fdc.h"
#include "gvram.h"
#include "mercury.h"
#include "mfp.h"
#include "midi.h"
#include "ioc.h"
#include "palette.h"
#include "pia.h"
#include "rtc.h"
#include "sasi.h"
#include "scc.h"
#include "scsi.h"
#include "sram.h"
#include "sysport.h"
#include "tvram.h"
#include "x68kmemory.h"

#include "fmg_wrap.h"

void AdrError(int32_t, int32_t);
void BusError(int32_t, int32_t);

static void wm_main(int32_t addr, uint8_t val);
static void wm_cnt(int32_t addr, uint8_t val);
static void wm_buserr(int32_t addr, uint8_t val);
static void wm_opm(int32_t addr, uint8_t val);
static void wm_e82(int32_t addr, uint8_t val);
static void wm_nop(int32_t addr, uint8_t val);

static uint8_t rm_main(int32_t addr);
static uint8_t rm_font(int32_t addr);
static uint8_t rm_ipl(int32_t addr);
static uint8_t rm_nop(int32_t addr);
static uint8_t rm_opm(int32_t addr);
static uint8_t rm_e82(int32_t addr);
static uint8_t rm_buserr(int32_t addr);

void cpu_setOPbase24(int32_t addr);
void Memory_ErrTrace(void);

uint8_t (*MemReadTable[])(int32_t) = {
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	CRTC_Read, rm_e82, DMA_Read, rm_nop, MFP_Read, RTC_Read, rm_nop, SysPort_Read,
	rm_opm, ADPCM_Read, FDC_Read, SASI_Read, SCC_Read, PIA_Read, IOC_Read, rm_nop,
	SCSI_Read, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, MIDI_Read,
	BG_Read, BG_Read, BG_Read, BG_Read, BG_Read, BG_Read, BG_Read, BG_Read,
#ifndef	NO_MERCURY
	rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, Mcry_Read, rm_buserr,
#else
	rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr,
#endif
	SRAM_Read, SRAM_Read, SRAM_Read, SRAM_Read, SRAM_Read, SRAM_Read, SRAM_Read, SRAM_Read,
	rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr,
	rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
/* SCSI の場合は rm_buserr になる？ */
	rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl,
	rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl,
	rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl,
	rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl,
};

void (*MemWriteTable[])(int32_t, uint8_t) = {
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	CRTC_Write, wm_e82, DMA_Write, wm_nop, MFP_Write, RTC_Write, wm_nop, SysPort_Write,
	wm_opm, ADPCM_Write, FDC_Write, SASI_Write, SCC_Write, PIA_Write, IOC_Write, SCSI_iocs,
	SCSI_Write, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, MIDI_Write,
	BG_Write, BG_Write, BG_Write, BG_Write, BG_Write, BG_Write, BG_Write, BG_Write,
#ifndef	NO_MERCURY
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, Mcry_Write, wm_buserr,
#else
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
#endif
	SRAM_Write, SRAM_Write, SRAM_Write, SRAM_Write, SRAM_Write, SRAM_Write, SRAM_Write, SRAM_Write,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
/* ROMエリアへの書きこみは全てバスエラー */
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
};

uint8_t *IPL;
uint8_t *MEM;
uint8_t *OP_ROM;
uint8_t *FONT;

int32_t BusErrFlag = 0;
int32_t BusErrHandling = 0;
int32_t BusErrAdr;
int32_t MemByteAccess = 0;

/*
 * write function
 */
void 
dma_writemem24(int32_t addr, uint8_t val)
{

	MemByteAccess = 0;

	wm_main(addr, val);
}

void 
dma_writemem24_word(int32_t addr, uint16_t val)
{

	MemByteAccess = 0;

	if (addr & 1) {
		BusErrFlag |= 4;
		return;
	}

	wm_main(addr, (val >> 8) & 0xff);
	wm_main(addr + 1, val & 0xff);
}

void 
dma_writemem24_dword(int32_t addr, uint32_t val)
{

	MemByteAccess = 0;

	if (addr & 1) {
		BusErrFlag |= 4;
		return;
	}

	wm_main(addr, (val >> 24) & 0xff);
	wm_main(addr + 1, (val >> 16) & 0xff);
	wm_main(addr + 2, (val >> 8) & 0xff);
	wm_main(addr + 3, val & 0xff);
}

void 
cpu_writemem24(int32_t addr, uint32_t val)
{

	MemByteAccess = 0;
	BusErrFlag = 0;

	wm_cnt(addr, val & 0xff);
	if (BusErrFlag & 2) {
		Memory_ErrTrace();
		BusError(addr, val);
	}
}

void 
cpu_writemem24_word(int32_t addr, uint32_t val)
{

	MemByteAccess = 0;

	if (addr & 1) {
		AdrError(addr, val);
		return;
	}

	BusErrFlag = 0;

	wm_cnt(addr, (val >> 8) & 0xff);
	wm_main(addr + 1, val & 0xff);

	if (BusErrFlag & 2) {
		Memory_ErrTrace();
		BusError(addr, val);
	}
}

void 
cpu_writemem24_dword(int32_t addr, uint32_t val)
{

	MemByteAccess = 0;

	if (addr & 1) {
		AdrError(addr, val);
		return;
	}

	BusErrFlag = 0;

	wm_cnt(addr, (val >> 24) & 0xff);
	wm_main(addr + 1, (val >> 16) & 0xff);
	wm_main(addr + 2, (val >> 8) & 0xff);
	wm_main(addr + 3, val & 0xff);

	if (BusErrFlag & 2) {
		Memory_ErrTrace();
		BusError(addr, val);
	}
}

static void 
wm_main(int32_t addr, uint8_t val)
{

	if ((BusErrFlag & 7) == 0)
		wm_cnt(addr, val);
}

static void 
wm_cnt(int32_t addr, uint8_t val)
{

	addr &= 0x00ffffff;
	if (addr < 0x00c00000) {	// Use RAM upto 12MB
		MEM[addr ^ 1] = val;
	} else if (addr < 0x00e00000) {
		GVRAM_Write(addr, val);
	} else {
		MemWriteTable[(addr >> 13) & 0xff](addr, val);
	}
}

static void 
wm_buserr(int32_t addr, uint8_t val)
{

	BusErrFlag = 2;
	BusErrAdr = addr;
	(void)val;
}

static void 
wm_opm(int32_t addr, uint8_t val)
{
	uint8_t t;
#ifdef RFMDRV
	char buf[2];
#endif

	t = addr & 3;
	if (t == 1) {
		OPM_Write(0, val);
	} else if (t == 3) {
		OPM_Write(1, val);
	}
#ifdef RFMDRV
	buf[0] = t;
	buf[1] = val;
	send(rfd_sock, buf, sizeof(buf), 0);
#endif
}

static void 
wm_e82(int32_t addr, uint8_t val) /* VIDEO WRITE */
{

	if (addr < 0x00e82400) {
		Pal_Write(addr, val);
	} else if (addr < 0x00e82700) {
		VCtrl_Write(addr, val);
	}
}

static void 
wm_nop(int32_t addr, uint8_t val)
{

	/* Nothing to do */
	(void)addr;
	(void)val;
}

/*
 * read function
 */
uint8_t
dma_readmem24(int32_t addr)
{

	return rm_main(addr);
}

uint16_t
dma_readmem24_word(int32_t addr)
{
	uint16_t v;

	if (addr & 1) {
		BusErrFlag = 3;
		return 0;
	}

	v = rm_main(addr++) << 8;
	v |= rm_main(addr);
	return v;
}

uint32_t
dma_readmem24_dword(int32_t addr)
{
	uint32_t v;

	if (addr & 1) {
		BusErrFlag = 3;
		return 0;
	}

	v = rm_main(addr++) << 24;
	v |= rm_main(addr++) << 16;
	v |= rm_main(addr++) << 8;
	v |= rm_main(addr);
	return v;
}

uint32_t
cpu_readmem24(int32_t addr)
{
	uint8_t v;

	v = rm_main(addr);
	if (BusErrFlag & 1) {
		p6logd("func = %s addr = %x flag = %d\n", __func__, addr, BusErrFlag);
		Memory_ErrTrace();
		BusError(addr, 0);
	}
	return (uint32_t) v;
}

uint32_t
cpu_readmem24_word(int32_t addr)
{
	uint16_t v;

	if (addr & 1) {
		AdrError(addr, 0);
		return 0;
	}

	BusErrFlag = 0;

	v = rm_main(addr++) << 8;
	v |= rm_main(addr);
	if (BusErrFlag & 1) {
		p6logd("func = %s addr = %x flag = %d\n", __func__, addr, BusErrFlag);
		Memory_ErrTrace();
		BusError(addr, 0);
	}
	return (uint32_t) v;
}

uint32_t
cpu_readmem24_dword(int32_t addr)
{
	uint32_t v;

	MemByteAccess = 0;

	if (addr & 1) {
		BusErrFlag = 3;
		p6logd("func = %s addr = %x\n", __func__, addr);
		return 0;
	}

	BusErrFlag = 0;

	v = rm_main(addr++) << 24;
	v |= rm_main(addr++) << 16;
	v |= rm_main(addr++) << 8;
	v |= rm_main(addr);
	return v;
}

static uint8_t
rm_main(int32_t addr)
{
	uint8_t v;

	addr &= 0x00ffffff;
	if (addr < 0x00c00000) {	// Use RAM upto 12MB
		v = MEM[addr ^ 1];
	} else if (addr < 0x00e00000) {
		v = GVRAM_Read(addr);
	} else {
		v = MemReadTable[(addr >> 13) & 0xff](addr);
	}

	return v;
}

static uint8_t
rm_font(int32_t addr)
{

	return FONT[addr & 0xfffff];
}

static uint8_t
rm_ipl(int32_t addr)
{

	return IPL[(addr & 0x3ffff) ^ 1];
}

static uint8_t
rm_nop(int32_t addr)
{

	(void)addr;
	return 0;
}

static uint8_t
rm_opm(int32_t addr)
{

	if ((addr & 3) == 3) {
		return OPM_Read(0);
	}
	return 0;
}

static uint8_t
rm_e82(int32_t addr)
{

	if (addr < 0x00e82400) {
		return Pal_Read(addr);
	} else if (addr < 0x00e83000) {
		return VCtrl_Read(addr);
	}
	return 0;
}

static uint8_t
rm_buserr(int32_t addr)
{
    p6logd("func = %s addr = %x flag = %d\n", __func__, addr, BusErrFlag);

	BusErrFlag = 1;
	BusErrAdr = addr;

	return 0;
}

/*
 * Memory misc
 */
void Memory_Init(void)
{

//        cpu_setOPbase24((DWORD)C68k_Get_Reg(&C68K, C68K_PC));
    cpu_setOPbase24((uint32_t)C68k_Get_PC(&C68K));

}

void 
cpu_setOPbase24(int32_t addr)
{

	switch ((addr >> 20) & 0xf) {
	case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
	case 8: case 9: case 0xa: case 0xb:
		OP_ROM = MEM;
		break;

	case 0xc: case 0xd:
		OP_ROM = GVRAM - 0x00c00000;
		break;

	case 0xe:
		if (addr < 0x00e80000) 
			OP_ROM = TVRAM - 0x00e00000;
		else if ((addr >= 0x00ea0000) && (addr < 0x00ea2000))
			OP_ROM = SCSIIPL - 0x00ea0000;
		else if ((addr >= 0x00ed0000) && (addr < 0x00ed4000))
			OP_ROM = SRAM - 0x00ed0000;
		else {
			BusErrFlag = 3;
			BusErrAdr = addr;
			Memory_ErrTrace();
			BusError(addr, 0);
		}
		break;

	case 0xf:
		if (addr >= 0x00fc0000)
			OP_ROM = IPL - 0x00fc0000;
		else {
			BusErrFlag = 3;
			BusErrAdr = addr;
			Memory_ErrTrace();
			BusError(addr, 0);
		}
		break;
	}
}

void 
Memory_SetSCSIMode(int32_t mode)/*1:Ex-SCSI 2:In-SCSI*/
{
	int_fast16_t i;

/*バスエラーに落とす*/
/*	for (i = 0xe0; i < 0xf0; i++) {
		MemReadTable[i] = rm_buserr;
	}
*/

/*作成中*/
	Memory_WriteB(0xe8e00d, 0x31);	/* Allow SRAM Access(91byte)*/
	switch(mode){/*SRAM SCSI set*/
		case 0:/*No-SCSI*/
			break;
		case 1:/*SCSI EX*/
			if(Memory_ReadB(0xed006f)!='V'){
			Memory_WriteB(0xe8e06f, 'V');	/*Activate SCSI*/
			Memory_WriteB(0xe8e070, 0x0f);	/*ExternalSCSI Set ID=7*/
			}
			break;
		case 2:/*SCSI IN*/
			if(Memory_ReadB(0xed006f)!='V'){
			Memory_WriteB(0xe8e06f, 'V');	/*Activate SCSI*/
			Memory_WriteB(0xe8e070, 0x07);	/*InternalSCSI Set ID=7*/
			}
			break;
		default:
			break;
	}
	Memory_WriteB(0xe8e071, 0x00);/*emurate SASI*/

	return;
}

void Memory_ErrTrace(void)
{
}

void 
Memory_IntErr(int32_t i)
{
}

void
AdrError(int32_t adr, int32_t unknown)
{

	(void)adr;
	(void)unknown;
	p6logd("AdrError: %x\n", adr);
	//	assert(0);
}

void
BusError(int32_t adr, int32_t unknown)
{

	(void)adr;
	(void)unknown;

	p6logd("BusError: %x\n", adr);
	BusErrHandling = 1;
	//assert(0);
}
