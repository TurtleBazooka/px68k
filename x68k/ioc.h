#ifndef _winx68k_ioc
#define _winx68k_ioc

#include "common.h"

extern	uint8_t	IOC_IntStat;
extern	uint8_t	IOC_IntVect;

void IOC_Init(void);
uint8_t IOC_Read(uint32_t adr);
void IOC_Write(uint32_t adr, uint8_t data);
void PRN_Write(uint32_t adr, uint8_t data);

#endif
