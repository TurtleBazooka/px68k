#ifndef _winx68k_ppi
#define _winx68k_ppi

#include "common.h"

void PPI_Init(void);
uint8_t FASTCALL PPI_Read(uint32_t adr);
void FASTCALL PPI_Write(uint32_t adr, uint8_t data);

#endif
