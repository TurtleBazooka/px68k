#ifndef _winx68k_fdc
#define _winx68k_fdc

#include "common.h"

void    FDC_Init(void);
uint8_t FDC_Read(uint32_t adr);
void    FDC_Write(uint32_t adr, uint8_t data);
int16_t FDC_Flush(void);
void    FDC_EPhaseEnd(void);
void    FDC_SetForceReady(int32_t n);
int32_t FDC_IsDataReady(void);

#endif //_winx68k_fdc

