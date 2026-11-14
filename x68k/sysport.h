#ifndef _winx68k_sysport
#define _winx68k_sysport

#include "common.h"

extern	uint8_t	SysPort[7];

void    SysPort_Init(void);
uint8_t SysPort_Read(uint32_t adr);
void    SysPort_Write(uint32_t adr, uint8_t data);

#endif
