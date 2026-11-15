#ifndef _winx68k_sasi
#define _winx68k_sasi

#include "common.h"

void    SASI_Init(void);
uint8_t SASI_Read(uint32_t adr);
void    SASI_Write(uint32_t adr, uint8_t data);
int32_t SASI_IsReady(void);


#endif //_winx68k_sasi
