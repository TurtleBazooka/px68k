#ifndef _winx68k_crtc
#define _winx68k_crtc

#include "common.h"

#define	VSYNC_HIGH	180310L
#define	VSYNC_NORM	162707L

extern	uint8_t		CRTC_Regs[48];
extern	uint8_t		CRTC_Mode;
extern	uint16_t	CRTC_VSTART, CRTC_VEND;
extern	uint16_t	CRTC_HSTART, CRTC_HEND;
extern	int32_t		TextDotX, TextDotY;
extern	int32_t		TextScrollX, TextScrollY;
extern	uint8_t		VCReg0[2];
extern	uint8_t		VCReg1[2];
extern	uint8_t		VCReg2[2];
extern	uint16_t	CRTC_IntLine;
extern	uint8_t		CRTC_FastClr;
extern	uint8_t		CRTC_DispScan;
extern	uint32_t	CRTC_FastClrLine;
extern	uint16_t	CRTC_FastClrMask;
extern	uint8_t		CRTC_VStep;
extern  int32_t		HSYNC_CLK;

extern	uint32_t	GrphScrollX[];
extern	uint32_t	GrphScrollY[];

void CRTC_Init(void);

void CRTC_RasterCopy(void);

uint8_t  CRTC_Read(uint32_t adr);
uint16_t CRTC_Read16(uint32_t adr);
void     CRTC_Write(uint32_t adr, uint8_t data);
void     CRTC_Write16(uint32_t adr, uint16_t data, uint8_t ulds);

uint8_t  VCtrl_Read(uint32_t adr);
void  VCtrl_Write(uint32_t adr, uint8_t data);
void  VCtrl_Write16(uint32_t adr, uint16_t data);

#endif
