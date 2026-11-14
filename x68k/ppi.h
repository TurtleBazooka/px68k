#ifndef _winx68k_ppi
#define _winx68k_ppi

#include "common.h"

void    PPI_Init(void);
uint8_t PPI_Read(uint32_t adr);
void    PPI_Write(uint32_t adr, uint8_t data);

#endif
