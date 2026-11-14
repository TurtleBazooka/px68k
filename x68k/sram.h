#ifndef _winx68k_sram
#define _winx68k_sram

#include "common.h"

extern	uint8_t	SRAM[0x4000];

void SRAM_Clear();
void SRAM_SetRamSize(uint8_t size);
void SRAM_SetSCSIMode(int32_t mode);
void SRAM_SetSASIDrive(uint8_t drive);

void SRAM_Init(void);
void SRAM_Cleanup(void);
void SRAM_VirusCheck(void);

uint8_t SRAM_Read(uint32_t adr);
void    SRAM_Write(uint32_t adr, uint8_t data);

#endif

