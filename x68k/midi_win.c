/* for WIN32-API */
#include <windows.h>
#include <mmsystem.h>
#include <tchar.h>
#include "prop.h"
#include "winui.h"
#include "midi.h"
#include "irqh.h"

extern HMIDIOUT hOut;
extern uint32_t midiOutGetNumDevs();
extern uint32_t midiOutGetDevCapsA(uint32_t, LPMIDIOUTCAPS, uint32_t);

extern HMIDIIN hIn;
extern uint32_t midiInGetNumDevs();
extern uint32_t midiInGetDevCapsA(uint32_t, LPMIDIINCAPS, uint32_t);
#define MIDI_CLOSE		962
#define MIDI_OPEN		961
#define MIDI_DATA		963
#define MIDI_MOREDATA	972
#define MIDI_LONGDATA	964
#define MIDI_ERROR		965
#define MIDI_LONGERROR	966

#pragma comment(lib,"winmm.lib")

/*
  Listing MIDI in device List
*/
uint32_t
mid_inDevList(LPHMIDIOUT phmo)
{
  MMRESULT res;
  uint32_t num, devid;
  MIDIINCAPS InCaps;
  uint32_t menu_no_in;

  menu_no_in = 0;

  /* What's No MIDI in ? */
  num = midiInGetNumDevs();
  if(num == 0){
     return num; // No found
  }
  else{
  for(devid = 0; devid<num; devid++){
    res=midiInGetDevCapsA(devid, &InCaps, sizeof(InCaps));
    if(res == MMSYSERR_NOERROR){
     strcpy(menu_items[MIDI_IN_menu_item][menu_no_in], InCaps.szPname);
     menu_no_in++;
    }
  }
  }
  strcpy(menu_items[MIDI_IN_menu_item][menu_no_in], "\0");

 return (num);
}

/*
  Listing MIDI out device List
*/
uint32_t
mid_outDevList(LPHMIDIOUT phmo)
{
  MMRESULT res;
  uint32_t num, devid;
  MIDIOUTCAPS OutCaps;
  uint32_t menu_no_out;

  menu_no_out = 0;

  /* What's No MIDI out ? */
  num = midiOutGetNumDevs();
  if(num == 0){
     return num; // No found
  }
  else{
  for(devid = 0; devid<num; devid++){
    res=midiOutGetDevCapsA(devid, &OutCaps, sizeof(OutCaps));
    if(res == MMSYSERR_NOERROR){
     strcpy(menu_items[MIDI_OUT_menu_item][menu_no_out], OutCaps.szPname);
     menu_no_out++;
    }
  }
  }
  strcpy(menu_items[MIDI_OUT_menu_item][menu_no_out], "\0");

 return (num);
}

/*
  Select Port and BANK
*/
void
midOutChg(uint32_t port_no, uint32_t bank)
{
	HMIDIOUT hmo;
	uint32_t msg;

	/* All note off */
	for (msg=0x7bb0; msg<0x7bc0; msg++)
	{
	  midiOutShortMsg(hmo, msg);
	}

	midiOutClose(hOut);

	if (midiOutOpen(&hOut, (port_no), 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR)
	{
	  midiOutReset(hOut);
	}

	/* select BANK CC20 LSB */
	//msg = ((bank << 16) | (0x20 << 8) | 0xb0);/*Bank select2*/
	//midiOutShortMsg(hmo, msg);

 return;
}

/*
  MIDI in CallBack
*/
void
midIn_CallBack(HMIDIIN hMidiIn, uint32_t wMsg, uint32_t dwInstance, uint32_t dwParam1, uint32_t dwParam2)
{
  if((MIDI_R35 & 0x01) == 0x00) return;//Rx-FIFO 受信禁止

  switch(wMsg) {
   case MIDI_DATA:
	/* (uint32)msg を 4byte に分解 */
	uint32_t len;
	uint8_t messg[4];
	messg[0] = (uint8_t)(dwParam1 & 0xff);
	dwParam1>>=8;
	messg[1] = (uint8_t)(dwParam1 & 0xff);
	dwParam1>>=8;
	messg[2] = (uint8_t)(dwParam1 & 0xff);

	switch(messg[0] & 0xf0){
	 case 0xc0:
	 case 0xd0:
	  len = 2;
	  break;
	 case 0x80:
	 case 0x90:
	 case 0xa0:
	 case 0xb0:
	 case 0xe0:
	  len = 3;
	  break;
	 default:
	  len = 0;
	  break;
	}
	if((len !=0) && (RxW_point < 250)){
	 Rx_buff[RxW_point] = messg[0];
	 RxW_point++;
	 Rx_buff[RxW_point] = messg[1];
	 RxW_point++;
	 if(len == 3){
	 Rx_buff[RxW_point] = messg[2];
	 RxW_point++;
	 }
	 if(MIDI_IntEnable & 0x20){
	  MIDI_IntFlag |= 0x20; // Rx int
	  MIDI_IntVect =  0x0a; // int vector
	  IRQH_Int(4, &MIDI_Int);//int 4
	 }
	}
    break;
   case MIDI_OPEN:
   case MIDI_CLOSE:
   case MIDI_MOREDATA:
   case MIDI_LONGDATA:
   case MIDI_LONGERROR:
   case MIDI_ERROR:
   default:
    break;
  }

 return;
}

/*
  Select in Port
*/
void
midInChg(uint32_t port_no)
{
  midiInClose(hIn);

  if(midiInOpen(&hIn, port_no, midIn_CallBack, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR){
    midiInReset(hIn);
  }

  midiInStart(hIn);

 return;
}

