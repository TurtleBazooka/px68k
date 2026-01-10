//==========================
//  ppi.c - i8255A-PPI
//==========================

#include "common.h"
#include "ppi.h"
#include "adpcm.h"
#include "m68000.h"
#include "GamePad.h"

typedef struct {
	uint8_t PortA;
	uint8_t PortB;
	uint8_t PortC;
	uint8_t Ctrl;
} i8255;

static i8255 ppi;

//==========================
//   初期化
//==========================
void PPI_Init(void)
{
	ppi.PortA = 0xff;
	ppi.PortB = 0xff;
	ppi.PortC = 0x0b;
	ppi.Ctrl = 0;
}


//==========================
//   I/O Write (8255A)
//==========================
void FASTCALL PPI_Write(uint32_t adr, uint8_t data)
{
	uint8_t mask, bit, portc;

	/*0xe9a000 ~ 0xe9bfff*/
	switch(adr & 0x07){
	case 0x01://PortA
		ppi.PortA = data;
		break;
	case 0x03://PortB
		ppi.PortB = data;
		break;
	case 0x05://PortC
		portc = ppi.PortC;//保存
		ppi.PortC = data;
		if ( (portc&0x0f)!=(ppi.PortC&0x0f) ) ADPCM_SetPan(ppi.PortC&0x0f);
		if ( (portc&0x10)!=(ppi.PortC&0x10) ) GamePad_Write(0, (uint8_t)((data&0x10)?0xff:0x00));
		if ( (portc&0x20)!=(ppi.PortC&0x20) ) GamePad_Write(1, (uint8_t)((data&0x20)?0xff:0x00));
		break;
	case 0x07://Control
		if ( !(data&0x80) ) {
			portc = ppi.PortC;//保存
			bit   = (data>>1)&7;// bit NO.
			mask  = 1<<bit;
			if ( data&1 )    //Set/Reset
				ppi.PortC |= mask;
			else
				ppi.PortC &= ~mask;
			if ( (portc&0x0f)!=(ppi.PortC&0x0f) ) ADPCM_SetPan(ppi.PortC&0x0f);
			if ( (portc&0x10)!=(ppi.PortC&0x10) ) GamePad_Write(0, (uint8_t)((data&1)?0xff:0x00));
			if ( (portc&0x20)!=(ppi.PortC&0x20) ) GamePad_Write(1, (uint8_t)((data&1)?0xff:0x00));
		}
		else{
			ppi.Ctrl = data;
		}
		break;
	default:
		break;
	}

}


//==========================
//   I/O Read (8255A)
//==========================
uint8_t FASTCALL PPI_Read(uint32_t adr)
{
	uint8_t ret=0xff;

	/*0xe9a000 ~ 0xe9bfff*/
	switch(adr & 0x07){
	case 0x01://PortA
		if(ppi.Ctrl & 0x10) ret = GamePad_Read(0);
		else ret = ppi.PortA;
		break;
	case 0x03://PortB
		if(ppi.Ctrl & 0x02) ret = GamePad_Read(1);
		else ret = ppi.PortB;
		break;
	case 0x05://PortC
		ret = ppi.PortC;
		break;
	case 0x07://Control
		ret = ppi.Ctrl;
		break;
	default:
		break;
	}

	return ret;
}
