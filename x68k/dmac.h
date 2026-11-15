#ifndef _winx68k_dmac
#define _winx68k_dmac

#include "common.h"

typedef struct
{
	uint8_t CSR;		// 00
	uint8_t CER;
	uint8_t dmy0[2];
	uint8_t DCR;		// 04
	uint8_t OCR;
	uint8_t SCR;
	uint8_t CCR;
	uint8_t dmy1[2];	// 08
	uint16_t MTC;
	uint32_t MAR;		// 0C
	uint8_t dmy2[4];	// 10
	uint32_t DAR;		// 14
	uint8_t dmy3[2];	// 18
	uint16_t BTC;
	uint32_t BAR;		// 1C
	uint8_t dmy4[5];	// 20
	uint8_t NIV;
	uint8_t dmy5;
	uint8_t EIV;
	uint8_t dmy6;		// 28
	uint8_t MFC;
	uint8_t dmy7[3];
	uint8_t CPR;
	uint8_t dmy8[3];
	uint8_t DFC;
	uint8_t dmy9[7];
	uint8_t BFC;
	uint8_t dmya[5];
	uint8_t GCR;

} dmac_ch;

extern dmac_ch	DMA[4];

int32_t DMA_Int(uint8_t irq);
uint8_t DMA_Read(uint32_t adr);
void    DMA_Write(uint32_t adr, uint8_t data);

int32_t DMA_Exec(int32_t ch);
void DMA_Init(void);
void DMA_SetReadyCB(int32_t ch, int32_t (*func)(void));

#endif //_winx68k_dmac
