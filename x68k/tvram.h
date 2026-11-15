#ifndef _winx68k_tvram
#define _winx68k_tvram

#include "common.h"

extern	uint8_t	TVRAM[0x80000];
extern	uint8_t	TextDrawWork[1024*1024 + 100];
extern	uint8_t	TextDirtyLine[1024 + 100];
//extern	uint16_t	Text_LineBuf[1024];
extern	uint8_t	Text_TrFlag[1024 + 100];

void TVRAM_SetAllDirty(void);

void TVRAM_Init(void);
void TVRAM_Cleanup(void);

uint8_t  TVRAM_Read(uint32_t adr);
uint16_t TVRAM_Read16(uint32_t adr);
void TVRAM_Write(uint32_t adr, uint8_t data);
void TVRAM_RCUpdate(void);
void Text_DrawLine(int32_t opaq);

#endif
