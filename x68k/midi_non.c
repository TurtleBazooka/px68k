/*
 No Support for MIDI
 (Not installed  CZ-6BM1 I/F)
*/
#include "winui.h"
#include "irqh.h"
#include "x68kmemory.h"


// No-interrupt
int32_t MIDI_Int(uint8_t irq)
{
	return -1;
}

// No-timer
void MIDI_Timer(int32_t clk)
{
	return;
}


// No-Change Port(Output)
void
midOutChg(uint32_t port_no, uint32_t bank)
{
}

// No-Change Port(input)
void
midInChg(uint32_t port_no)
{
}

// No-Output
void MIDI_DelayOut(uint32_t delay)
{
}

//   initialize(No Device)
void MIDI_Init(void)
{
		 strcpy(menu_items[MIDI_OUT_menu_item][0],"No Device Found.");
		 strcpy(menu_items[MIDI_OUT_menu_item][1],"\0"); /* Menu END*/

		 strcpy(menu_items[MIDI_IN_menu_item][0],"No Device Found.");
		 strcpy(menu_items[MIDI_IN_menu_item][1],"\0"); /* Menu END*/
}

//   Quit
void MIDI_Cleanup(void)
{
}

//  NO-I/O Read
uint8_t MIDI_Read(uint32_t adr)
{
	BusErrFlag = 1;
	BusErrAdr = adr;

	return 0x00;
}

//   No-I/O Write
void MIDI_Write(uint32_t adr, uint8_t data)
{
	BusErrFlag = 2;
	BusErrAdr = adr;
}


