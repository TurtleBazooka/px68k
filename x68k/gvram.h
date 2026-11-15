#ifndef _winx68k_gvram
#define _winx68k_gvram

#include "common.h"

extern	uint8_t		GVRAM[0x80000];

extern	uint32_t	Grp_LineBuf32[1024];
extern	uint32_t	Grp_LineBuf32SP[1024];
extern	uint32_t	Grp_LineBuf32SP2[1024];

void GVRAM_Init(void);

void GVRAM_FastClear(void);

uint8_t GVRAM_Read(uint32_t adr);
void    GVRAM_Write(uint32_t adr, uint8_t data);

void Grp_DrawLine16(void);
void Grp_DrawLine8(int32_t page, int32_t opaq);
void Grp_DrawLine4(uint32_t page, int32_t opaq);
void Grp_DrawLine4h(void);
void Grp_DrawLine16SP(void);
void Grp_DrawLine8SP(int32_t page/*, int32_t opaq*/);
void Grp_DrawLine4SP(uint32_t page/*, int32_t opaq*/);
void Grp_DrawLine4hSP(void);
void Grp_DrawLine8TR(int32_t page, int32_t opaq);
void Grp_DrawLine8TR_GT(int32_t page, int32_t opaq);
void Grp_DrawLine4TR(uint32_t page, int32_t opaq);
#endif

