#ifndef _winx68k_scsi
#define _winx68k_scsi

#include "common.h"

extern	uint8_t	SCSIIPL[0x2000];

void SCSI_Init(void);
void SCSI_Cleanup(void);

uint8_t SCSI_Read(uint32_t adr);
void    SCSI_Write(uint32_t adr, uint8_t data);

#endif

