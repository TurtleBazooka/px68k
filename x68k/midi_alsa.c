/*
 Support for Linux alsa MIDI output
 (not implemented yet!)
*/
#include "prop.h"
#include "winui.h"
#include "midi.h"

// for  CZ-6BM1 I/F
#include "midi.c"

// for each machine.
// for alsa Sound
uint32_t
midiOutShortMsg(HMIDIOUT hmo, uint32_t dwMsg)
{
	(void)hmo;
	(void)dwMsg;
	return MMSYSERR_NOERROR;
}

uint32_t
midiOutLongMsg(HMIDIOUT hmo, LPMIDIHDR pmh, uint32_t cbmh)
{
	(void)hmo;
	(void)pmh;
	(void)cbmh;
	return MMSYSERR_NOERROR;
}

uint32_t
midiOutUnprepareHeader(HMIDIOUT hmo, LPMIDIHDR pmh, uint32_t cbmh)
{
	(void)hmo;
	(void)pmh;
	(void)cbmh;
	return MMSYSERR_NOERROR;
}

uint32_t
midiOutPrepareHeader(HMIDIOUT hmo, LPMIDIHDR pmh, uint32_t cbmh)
{
	(void)hmo;
	(void)pmh;
	(void)cbmh;
	return MMSYSERR_NOERROR;
}

uint32_t
mid_outDevList(LPHMIDIOUT phmo)
{
 return 0;
}

uint32_t
mid_inDevList(LPHMIDIOUT phmo)
{
 return 0;
}

uint32_t
midiOutClose(HMIDIOUT hmo)
{
	(void)hmo;
	return MMSYSERR_NOERROR;
}

uint32_t
midiOutReset(HMIDIOUT hmo)
{
	(void)hmo;
	return MMSYSERR_NOERROR;
}

void
midOutChg(uint32_t port_no, uint32_t bank)
{
	HMIDIOUT hmo;

	/* All note off */
	for (uint32_t msg=0x7bb0; msg<0x7bc0; msg++) {
		midiOutShortMsg(hmo, msg);
	}

	/* select BANK CC20 LSB */
	//uint32_t msg = ((bank << 16) | (0x20 << 8) | 0xb0);/*Bank select2*/
	//midiOutShortMsg(hmo, msg);

 return;
}

void
midInChg(uint32_t port_no)
{
 return;
}
