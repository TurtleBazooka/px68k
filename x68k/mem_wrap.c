/*	$Id: mem_wrap.c,v 1.2 2003/12/05 18:07:19 nonaka Exp $	*/
/*	Add Support 16bit(WORD)Access 2022/10/4 kameya */
/*	Add SCSI_iocs fuck address 2022/3/21*/

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
#include "ppi.h"
#include "rtc.h"
#include "sasi.h"
#include "scc.h"
#include "scsi.h"
#include "sram.h"
#include "sysport.h"
#include "tvram.h"

#ifdef	YMFM
#include "ymfm_wrap.h"
#else
#include "fmg_wrap.h"
#endif

void AdrError(uint32_t, int32_t);
void BusError(uint32_t, int32_t);

static void wm_main(uint32_t addr, uint8_t val);
static void wm16_main(uint32_t addr, uint16_t val);
static void wm_buserr(uint32_t addr, uint8_t val);
static void wm_opm(uint32_t addr, uint8_t val);
static void wm_e82(uint32_t addr, uint8_t val);
static void wm_nop(uint32_t addr, uint8_t val);

static uint8_t rm_main(uint32_t addr);
static uint16_t rm16_main(uint32_t addr);
static uint8_t rm_font(uint32_t addr);
static uint8_t rm_ipl(uint32_t addr);
static uint8_t rm_nop(uint32_t addr);
static uint8_t rm_opm(uint32_t addr);
static uint8_t rm_e82(uint32_t addr);
static uint8_t rm_buserr(uint32_t addr);

void cpu_setOPbase24(uint32_t addr);
void Memory_ErrTrace(void);

uint8_t (*MemReadTable[])(uint32_t) = {
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	CRTC_Read, rm_e82, DMA_Read, rm_nop, MFP_Read, RTC_Read, rm_nop, SysPort_Read,
	rm_opm, ADPCM_Read, FDC_Read, SASI_Read, SCC_Read, PPI_Read, IOC_Read, rm_nop,
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

void (*MemWriteTable[])(uint32_t, uint8_t) = {
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	CRTC_Write, wm_e82, DMA_Write, wm_nop, MFP_Write, RTC_Write, PRN_Write, SysPort_Write,
	wm_opm, ADPCM_Write, FDC_Write, SASI_Write, SCC_Write, PPI_Write, IOC_Write, wm_buserr,
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
dma_writemem24(uint32_t addr, uint8_t val)
{

	MemByteAccess = 0;

	if ((BusErrFlag & 7) == 0){
	  wm_main(addr, val);
	}

}

void 
dma_writemem24_word(uint32_t addr, uint16_t val)
{

	MemByteAccess = 0;

	if (addr & 1) {
		BusErrFlag |= 4;
		return;
	}

	if ((BusErrFlag & 7) == 0){
	  wm16_main(addr  ,val);
	}

}

void 
dma_writemem24_dword(uint32_t addr, uint32_t val)
{

	MemByteAccess = 0;

	if (addr & 1) {
		BusErrFlag |= 4;
		return;
	}

	if ((BusErrFlag & 7) == 0){
	  wm16_main(addr  ,(val >> 16) & 0xffff);
	  wm16_main(addr+2, val & 0xffff);
	}

}

void 
cpu_writemem24(uint32_t addr, uint32_t val)
{

	MemByteAccess = 0;
	BusErrFlag = 0;

	wm_main(addr, val & 0xff);

	if (BusErrFlag & 2) {
		Memory_ErrTrace();
		BusError(addr, val);
	}
}

void 
cpu_writemem24_word(uint32_t addr, uint32_t val)
{

  MemByteAccess = 0;

  if (addr & 1) {
  	AdrError(addr, val);
  	return;
  }

  BusErrFlag = 0;

  wm16_main(addr  ,val & 0xffff);

  if (BusErrFlag & 2) {
  	Memory_ErrTrace();
  	BusError(addr, val);
  }

  return;
}

void 
cpu_writemem24_dword(uint32_t addr, uint32_t val)
{

	MemByteAccess = 0;

	if (addr & 1) {
		AdrError(addr, val);
		return;
	}

	BusErrFlag = 0;

	wm16_main(addr  ,(val >> 16) & 0xffff);
	if ((BusErrFlag & 7) == 0){
	wm16_main(addr+2, val & 0xffff);
	}

	if (BusErrFlag & 2) {
		Memory_ErrTrace();
		BusError(addr, val);
	}
}


static void 
wm_main(uint32_t addr, uint8_t val)
{
  addr &= 0x00ffffff;

  switch(addr){
  case 0x000000 ... 0xbfffff: /* RAM */
#ifndef __BIG_ENDIAN__
    MEM[addr ^ 1] = val;
#else
    MEM[addr    ] = val;
#endif
    break;
  case 0xc00000 ... 0xdfffff: /*GVRAM*/
    GVRAM_Write(addr, val);
    break;
  default:
    MemWriteTable[(addr >> 13) & 0xff](addr, val);
    break;
  }

  return;
}

static void 
wm16_main(uint32_t addr, uint16_t val)
{
  addr &= 0x00fffffe;

  switch(addr){
  case 0x000000 ... 0xbfffff: /* RAM */
    *(uint16_t *)&MEM[addr] = (uint16_t)(val & 0xffff);
    return;
    break;
  case 0xc00000 ... 0xdfffff: /*GVRAM*/
    GVRAM_Write(addr, (val>>8) & 0xff);
    GVRAM_Write(addr+1, val & 0xff);
    break;
  //case 0xe00000 ... 0xe7ffff: /*TVRAM*/
    
  case 0xe80000 ... 0xe81fff: /*CRTC*/
	CRTC_Write16(addr,val,0x03);
	break;
  case 0xe82000 ... 0xe823ff: /*Pallette*/
	Pal_Write16(addr,val);
	break;
  case 0xe82400 ... 0xe826ff: /*VCtrl*/
	VCtrl_Write16(addr,val);
	break;
  case 0xe92000 ... 0xe93fff: /*ADPCM*/
	ADPCM_Write(addr|1,(uint8_t)(val & 0xff));
    break;
  //case 0xe9e000 ... 0xe9e07f: /*68881*/
  case 0xed0000 ... 0xed3fff: /*SRAM*/
	/* through write control */
  default:
	MemWriteTable[(addr >> 13) & 0xff](addr, (val>>8) & 0xff);
	addr++;
	MemWriteTable[(addr >> 13) & 0xff](addr, val & 0xff);
    break;
  }

  return;
}


static void 
wm_buserr(uint32_t addr, uint8_t val)
{

	BusErrFlag = 2;
	BusErrAdr = addr;
	(void)val;
}

static void 
wm_opm(uint32_t addr, uint8_t val)
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
wm_e82(uint32_t addr, uint8_t val) /* VIDEO WRITE */
{
  switch(addr){
  case 0xe82000 ... 0xe823ff: /* Pallette */
   Pal_Write(addr, val);
   break;
  case 0xe82400 ... 0xe826ff: /* Video Control */
   VCtrl_Write(addr, val);
   break;
  default:
   break;
  }

  return;
}

static void 
wm_nop(uint32_t addr, uint8_t val)
{

	/* Nothing to do */
	(void)addr;
	(void)val;
}

/*
 * read function
 */
uint8_t
dma_readmem24(uint32_t addr)
{

	return rm_main(addr);
}

uint16_t
dma_readmem24_word(uint32_t addr)
{
	uint16_t v;

	if (addr & 1) {
		BusErrFlag = 3;
		return 0;
	}

	v = rm16_main(addr);

	return v;
}

uint32_t
dma_readmem24_dword(uint32_t addr)
{
	uint32_t v;

	if (addr & 1) {
		BusErrFlag = 3;
		return 0;
	}

	v  = rm16_main(addr) << 16;
	v |= rm16_main(addr+2);

	return v;
}

uint32_t
cpu_readmem24(uint32_t addr)
{
	uint8_t v;
	BusErrFlag = 0;

	v = rm_main(addr);

	if (BusErrFlag & 1) {
		p6logd("func = %s addr = %x flag = %d\n", __func__, addr, BusErrFlag);
		Memory_ErrTrace();
		BusError(addr, 0);
	}
	return (uint32_t) v;
}

uint32_t
cpu_readmem24_word(uint32_t addr)
{
  uint16_t v;

  if (addr & 1) {
  	AdrError(addr, 0);
  	return 0;
  }

  BusErrFlag = 0;

  v = rm16_main(addr);

  if (BusErrFlag & 1) {
  	p6logd("func = %s addr = %x flag = %d\n", __func__, addr, BusErrFlag);
  	Memory_ErrTrace();
  	BusError(addr, 0);
  }

  return (uint32_t) v;
}

uint32_t
cpu_readmem24_dword(uint32_t addr)
{
	uint32_t v;

	MemByteAccess = 0;

	if (addr & 1) {
		BusErrFlag = 3;
		p6logd("func = %s addr = %x\n", __func__, addr);
		return 0;
	}

	BusErrFlag = 0;

	v  = rm16_main(addr) << 16;
	v |= rm16_main(addr+2);

	return v;
}

static uint16_t
rm16_main(uint32_t addr)
{
  uint16_t v;

  addr &= 0x00fffffe;

  switch(addr){
  case 0x000000 ... 0xbfffff: /* RAM */
    return((uint16_t)*(uint16_t *)&MEM[addr]);
    break;
  case 0xc00000 ... 0xdfffff: /*GVRAM*/
    v  = GVRAM_Read(addr) << 8;
    v |= GVRAM_Read(addr +1);
    break;
  //case 0xe00000 ... 0xe7ffff: /*TVRAM*/

  case 0xe80000 ... 0xe81fff: /*CRTC*/
	return CRTC_Read16(addr);
	break;
  case 0xe82000 ... 0xe823ff: /*Pallette*/
	return Pal_Read16(addr);
	break;
  case 0xed0000 ... 0xed3fff: /*SRAM*/
    return((uint16_t)*(uint16_t *)&SRAM[(addr & 0x003fff)]);
    break;
  case 0xf00000 ... 0xfbffff: /*FONT-ROM*/
    return((uint16_t)*(uint16_t *)&FONT[(addr & 0xfffff)]);
    break;
  case 0xfc0000 ... 0xffffff: /*IPL-ROM*/
    return((uint16_t)*(uint16_t *)&IPL[(addr & 0x3ffff)]);
    break;
  default:
	v  = MemReadTable[(addr >> 13) & 0xff](addr) << 8;
	addr++;
	v |= MemReadTable[(addr >> 13) & 0xff](addr);
    break;
  }
 return v;
}


static uint8_t
rm_main(uint32_t addr)
{

  addr &= 0x00ffffff;

  switch(addr){
  case 0x000000 ... 0xbfffff: /* RAM */
    if(addr & 1){
    return(*(uint16_t *)&MEM[(addr & 0xfffffe)] & 0xff);   /*奇数Byte*/
    }
    return((*(uint16_t *)&MEM[(addr & 0xfffffe)] >> 8) & 0xff);/*偶数Byte*/
    break;
  case 0xc00000 ... 0xdfffff: /*GVRAM*/
    return(GVRAM_Read(addr));
    break;
  default:
    return(MemReadTable[(addr >> 13) & 0xff](addr));
    break;
  }

}

static uint8_t
rm_font(uint32_t addr)
{
if(addr & 1){
return(*(uint16_t *)&FONT[(addr & 0xffffe)] & 0xff);   /*奇数Byte*/
}
return((*(uint16_t *)&FONT[(addr & 0xffffe)] >> 8) & 0xff);/*偶数Byte*/
}

static uint8_t
rm_ipl(uint32_t addr)
{
if(addr & 1){
return(*(uint16_t *)&IPL[(addr & 0x3fffe)] & 0xff);   /*奇数Byte*/
}
return((*(uint16_t *)&IPL[(addr & 0x3fffe)] >> 8) & 0xff);/*偶数Byte*/
}

static uint8_t
rm_nop(uint32_t addr)
{

	(void)addr;
	return 0;
}

static uint8_t
rm_opm(uint32_t addr)
{

	if ((addr & 3) == 3) {
		return OPM_Read(0);
	}
	return 0;
}

static uint8_t
rm_e82(uint32_t addr)
{
  switch(addr){
  case 0xe82000 ... 0xe823ff: /* Pallette */
   return Pal_Read(addr);
   break;
  case 0xe82400 ... 0xe826ff: /* Video Control */
   return VCtrl_Read(addr);
   break;
  default:
   break;
  }

  return 0;
}

static uint8_t
rm_buserr(uint32_t addr)
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

  cpu_setOPbase24((uint32_t)m68000_get_reg(M68K_PC));
}

void 
cpu_setOPbase24(uint32_t addr)
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
Memory_SetSCSIMode() //SCSIアクセスはBusErrorにSet
{
	int_fast16_t i;

/*バスエラーに落とす*/
  for (i = 0xe0; i < 0xf0; i++) {
   MemReadTable[i] = rm_buserr;
  }
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
AdrError(uint32_t adr, int32_t unknown)
{

	(void)adr;
	(void)unknown;
	p6logd("AdrError: %x\n", adr);
	//	assert(0);
}

void
BusError(uint32_t adr, int32_t unknown)
{

	(void)adr;
	(void)unknown;

	p6logd("BusError: %x\n", adr);
	BusErrHandling = 1;
	//assert(0);
}
