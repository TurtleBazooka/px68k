/* for WIN32-API */
#include <windows.h>
#include <mmsystem.h>
#include <tchar.h>
#pragma comment(lib,"winmm.lib")

#include "prop.h"
#include "winui.h"
#include "x68kmemory.h"
#include "irqh.h"
#include "dosio.h"

// for Win API

#define MIDIBUFFERS 1024			// 1024は流石に越えないでしょう^_^;
#define MIDIBUFTIMER 3200			// 10MHz / (31.25K / 10bit) = 3200 が正解になります... 
#define MIDIFIFOSIZE 256
#define MIDIDELAYBUF 4096			// 31250/10 = 3125 byts (1s分) あればおっけ？

enum {						// 各機種リセット用に一応。
	MIDI_NOTUSED,
	MIDI_DEFAULT,
	MIDI_MT32,
	MIDI_CM32L,
	MIDI_CM64,
	MIDI_CM300,
	MIDI_CM500,
	MIDI_SC55,
	MIDI_SC88,
	MIDI_LA,				// 意味もなく追加してみたり
	MIDI_GM,
	MIDI_GS,
	MIDI_XG,
};

HMIDIOUT	hOut = 0;
MIDIHDR		hHdr;

uint8_t		MIDI_EXCVBUF[MIDIBUFFERS];
uint8_t		MIDI_EXCVWAIT;

static uint32_t Tx_ptr;
static uint8_t  Tx_buff[MIDIBUFFERS];

HMIDIIN		hIn = 0;
uint8_t		Rx_buff[MIDIFIFOSIZE];
int32_t		RxW_point;
int32_t		RxR_point;

uint8_t		MIDI_RegHigh = 0;				// X68K用
uint8_t		MIDI_Playing = 0;				// マスタスイッチ
uint8_t		MIDI_Vector = 0;
uint8_t		MIDI_IntEnable = 0;
uint8_t		MIDI_IntVect = 0;
uint8_t		MIDI_IntFlag = 0;
uint32_t	MIDI_Buffered = 0;
int32_t		MIDI_BufTimer = 3333;
uint8_t		MIDI_R05 = 0;
uint8_t		MIDI_R35 = 0;
uint8_t		MIDI_R55 = 0;
uint32_t	MIDI_GTimerMax = 0;
uint32_t	MIDI_MTimerMax = 0;
int32_t		MIDI_GTimerVal = 0;
int32_t		MIDI_MTimerVal = 0;
uint8_t		MIDI_TxFull = 0;
uint8_t		MIDI_MODULE = MIDI_NOTUSED;

static uint8_t MIDI_ResetType[5] = {		// Config.MIDI_Type に合わせて…
	MIDI_LA, MIDI_GM, MIDI_GS, MIDI_XG
};

typedef struct {
	uint64_t time;
	uint8_t msg;
} DELAYBUFITEM;

static DELAYBUFITEM DelayBuf[MIDIDELAYBUF];
static int32_t DBufPtrW = 0;
static int32_t DBufPtrR = 0;



// ------ reset command ------
static uint8_t EXCV_LARESET[] = { 0xf0, 0x41, 0x10, 0x16, 0x12, 0x7f, 0x01, 0xf7};
static uint8_t EXCV_GMRESET[] = { 0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7};
static uint8_t EXCV_GSRESET[] = { 0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7};
static uint8_t EXCV_XGRESET[] = { 0xf0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xf7};


/*
 Exception(割り込み)
*/
int32_t MIDI_Int(uint8_t irq)
{
	int32_t ret;
	IRQH_IRQCallBack(irq);
	if ( irq==4 )
	{
		ret = (int32_t)(MIDI_Vector|MIDI_IntVect);
	}
	else
	{
		ret = -1;
	}

	return ret;
}
/*
  Timer (x3)
*/
void MIDI_Timer(int32_t clk)
{
	if ( !Config.MIDI_SW ) return;	// MIDI OFF時は帰る

	MIDI_BufTimer -= clk;
	if (MIDI_BufTimer<0)
	{
		MIDI_BufTimer += MIDIBUFTIMER;
		if (MIDI_Buffered)
		{
			MIDI_Buffered--;
			if ( (MIDI_Buffered<MIDIFIFOSIZE)&&(MIDI_IntEnable&0x40) )	// Tx FIFO Empty Interrupt（エトプリ）
			{
				MIDI_IntFlag |= 0x40;
				MIDI_IntVect = 0x0c;
				IRQH_Int(4, &MIDI_Int);
			}
		}
	}

	if (MIDI_MTimerMax)
	{
		 MIDI_MTimerVal -= clk;
		if (MIDI_MTimerVal<0)		// みぢたいまー割り込み（魔法大作戦）
		{
			while (MIDI_MTimerVal<0) MIDI_MTimerVal += MIDI_MTimerMax*80;
			if ( (!(MIDI_R05&0x80))&&(MIDI_IntEnable&0x02) )
			{
				MIDI_IntFlag |= 0x02;
				MIDI_IntVect = 0x02;
				IRQH_Int(4, &MIDI_Int);
			}
		}
	}

	if (MIDI_GTimerMax)
	{
		MIDI_GTimerVal -= clk;
		if (MIDI_GTimerVal<0)		// じぇねらるたいまー割り込み（RCD.X）
		{
			while (MIDI_GTimerVal<0) MIDI_GTimerVal += MIDI_GTimerMax*80;
			if ( MIDI_IntEnable&0x80 )
			{
				MIDI_IntFlag |= 0x80;
				MIDI_IntVect = 0x0e;
				IRQH_Int(4, &MIDI_Int);
			}
		}
	}
}

/*
  Set MIDI module type
*/
void MIDI_SetModule(void)
{
	if (Config.MIDI_SW)
		MIDI_MODULE = MIDI_ResetType[Config.MIDI_Type];
	else
		MIDI_MODULE = MIDI_NOTUSED;
}




/*
  Listing MIDI in device List
*/
uint32_t
mid_inDevList(void)
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
mid_outDevList(void)
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

  //if(midiInOpen(&hIn, port_no, midIn_CallBack, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR){
  if(midiInOpen(&hIn, port_no, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR){
    midiInReset(hIn);
  }

  midiInStart(hIn);

 return;
}

// -----------------------------------------------------------------------
//   えくすくるーしぶ ごー
// -----------------------------------------------------------------------
void MIDI_Sendexclusive(uint8_t *excv, int32_t length)
{
	// エクスクルーシヴを送ります
	memcpy(MIDI_EXCVBUF, excv, length);
	hHdr.lpData = (char*)MIDI_EXCVBUF;
	hHdr.dwFlags = 0;
	hHdr.dwBufferLength = length;
	midiOutPrepareHeader(hOut, &hHdr, sizeof(MIDIHDR));
	midiOutLongMsg(hOut, &hHdr, sizeof(MIDIHDR));
	MIDI_EXCVWAIT = 1;
}


// -----------------------------------------------------------------------
//   えくすくるーしぶを送り終えるまで待つお
// -----------------------------------------------------------------------
void MIDI_Waitlastexclusiveout(void) {

	// エクスクルーシヴ送信完了まで待ちましょう〜
	if (MIDI_EXCVWAIT) {
		while(midiOutUnprepareHeader(hOut, &hHdr, sizeof(MIDIHDR))
						== MIDIERR_STILLPLAYING);
		MIDI_EXCVWAIT = 0;
	}
}


// -----------------------------------------------------------------------
//   りせっと〜
// -----------------------------------------------------------------------
void MIDI_Reset(void) {

	memset(DelayBuf, 0, sizeof(DelayBuf));
	DBufPtrW = DBufPtrR = 0;

	if (hOut) {
		switch(MIDI_MODULE) {
			case MIDI_NOTUSED:
				return;
			case MIDI_MT32:
			case MIDI_CM32L:
			case MIDI_CM64:
			case MIDI_LA:
				// ちょっと乱暴かなぁ…
				// 一応 SC系でも通る筈ですけど…
				MIDI_Waitlastexclusiveout();
				MIDI_Sendexclusive(EXCV_LARESET, sizeof(EXCV_LARESET));
				break;
			case MIDI_SC55:
			case MIDI_SC88:
			case MIDI_GS:
				MIDI_Waitlastexclusiveout();
				MIDI_Sendexclusive(EXCV_GSRESET, sizeof(EXCV_GSRESET));
				break;
			case MIDI_XG:
				MIDI_Waitlastexclusiveout();
				MIDI_Sendexclusive(EXCV_XGRESET, sizeof(EXCV_XGRESET));
				break;
			default:
				MIDI_Waitlastexclusiveout();
				MIDI_Sendexclusive(EXCV_GMRESET, sizeof(EXCV_GMRESET));
				break;
		}
		MIDI_Waitlastexclusiveout();
		for (uint32_t msg=0x7bb0; msg<0x7bc0; msg++) {// all note off
			midiOutShortMsg(hOut, msg);
		}
	}
}


// -----------------------------------------------------------------------
//   しょきか〜
// -----------------------------------------------------------------------
void MIDI_Init(void) {

	memset(DelayBuf, 0, sizeof(DelayBuf));
	DBufPtrW = DBufPtrR = 0;

	MIDI_SetModule();
	MIDI_RegHigh = 0;		// X68K
	MIDI_Vector = 0;		// X68K
	MIDI_IntEnable = 0;
	MIDI_IntVect = 0;
	MIDI_IntFlag = 0;
	MIDI_R05 = 0;

	MIDI_EXCVWAIT = 0;

	RxW_point = 0;
	RxR_point = 0;

	Tx_ptr = 0;

	if (!hOut) {
		if(!Config.MIDI_SW){ /* if MIDI inactive?*/
		 strcpy(menu_items[MIDI_OUT_menu_item][0],"inactive.");
		 strcpy(menu_items[MIDI_OUT_menu_item][1],"\0"); /* Menu END*/
		 hOut = 0;
		}else if(mid_outDevList() > 0){ /* if MIDI found?*/
		 midOutChg(0,0); /*Select Port0 Bank 0*/
		}
		else{
		 strcpy(menu_items[MIDI_OUT_menu_item][0],"No Device Found.");
		 strcpy(menu_items[MIDI_OUT_menu_item][1],"\0"); /* Menu END*/
		 hOut = 0;
		}
	}

	if (!hIn) {
		if(!Config.MIDI_SW){ /* if MIDI inactive?*/
		 strcpy(menu_items[MIDI_IN_menu_item][0],"inactive.");
		 strcpy(menu_items[MIDI_IN_menu_item][1],"\0"); /* Menu END*/
		 hIn = 0;
		}else if(mid_inDevList() > 0){ /* if MIDI found?*/
		 midInChg(0); /*Select Port0 */
		}
		else{
		 strcpy(menu_items[MIDI_IN_menu_item][0],"No Device Found.");
		 strcpy(menu_items[MIDI_IN_menu_item][1],"\0"); /* Menu END*/
		 hIn = 0;
		}
	}

	if (Config.MIDI_SW && Config.MIDI_Reset)  MIDI_Reset();// りせっと〜

 return;
}

// -----------------------------------------------------------------------
//   撤収〜
// -----------------------------------------------------------------------
void MIDI_Cleanup(void) {

	if (hOut) {
	  for (uint32_t msg=0x7bb0; msg<0x7bc0; msg++) {// all note off(消音)
	     midiOutShortMsg(hOut, msg);
	  }

	  if (Config.MIDI_SW && Config.MIDI_Reset)  MIDI_Reset();// りせっと〜
	  MIDI_Waitlastexclusiveout();
	  midiOutReset(hOut);
	  midiOutClose(hOut);
	  hOut = 0;
	}
	if (hIn) {
	  midiInClose(hIn);
	  hIn = 0;
	}
}

/* New MIDI Packet sender */
/* by kameya 2023/07/07   */
void  midi_data_out( uint8_t data )
{
  uint32_t leng;

  if (!hOut) { return; } // Sound Not open

  Tx_buff[Tx_ptr] = data; // Store Data

  if(Tx_ptr>sizeof(Tx_buff)-5){// over run check
	Tx_buff[0] = 0x00;
	Tx_ptr = 0;
	return;
  }

  if((Tx_ptr == 0)&&(data>0x7f)){// first command store
   Tx_buff[0] = data; // Store Data
   Tx_ptr = 1;
   return;
  }

  switch(Tx_buff[0]){ // command length
   case 0xc0 ... 0xdf:
   case 0xf1:
   case 0xf3:
	leng = 2;
	break;
   case 0xf4 ... 0xff:
	leng = 1;
	break;
   default:
	leng = 3;
	break;
  }

  switch(Tx_buff[0]){ // send command
  case 0x80 ... 0xef:
  case 0xf1 ... 0xff:
    if(data>0x7f){
      Tx_buff[0] = data; // Store Data
      Tx_ptr = 1;
      break;
    }
    if(Tx_ptr == (leng -1)){ // short message 
      MIDI_Waitlastexclusiveout();
      midiOutShortMsg(hOut, (Tx_buff[0] | Tx_buff[1]<<8 | Tx_buff[2]<<16));
      Tx_ptr = 1;
    }
    else{
     Tx_ptr ++;
    }
    break;
  case 0xf0:
    if((Tx_buff[Tx_ptr-1] == 0xf7)&&(data != 0xf7)){//exclusive message and end.
      MIDI_Waitlastexclusiveout();
      MIDI_Sendexclusive(Tx_buff,Tx_ptr);
      Tx_buff[0] = data;
      Tx_ptr = 1;
    }
    else{
     Tx_ptr ++;
    }
    break;
  default: // error (no command)
    Tx_buff[0] = 0x00;
    Tx_ptr = 0;
    break;
  }

  return;
}

// -----------------------------------------------------------------------
//   I/O Read
// -----------------------------------------------------------------------
uint8_t MIDI_Read(uint32_t adr)
{
	uint8_t ret = 0x00;

	switch(adr)
	{
	case 0xeafa00 ... 0xeafa0f: /*CZ-6BM1(this 1st MIDI)*/
	  if(Config.MIDI_SW){break;}
	case 0xeafa10 ... 0xeafaff: /*CZ-6BM1(2nd MIDI)*/
	case 0xeafb00 ... 0xeafbff: /*CZ-6BN1(parallel)*/
	case 0xeafc00 ... 0xeafdff: /*CZ-6BF1(RS232C 5ch)*/
	case 0xeafe00 ... 0xeaffff: /*CZ-6BG1(GPIB)*/
	default://rm_buserr
	 	BusErrFlag = 1;
		BusErrAdr = addr;
		return 0x00;
	}

	switch(adr & 0x0f) /*CZ-6BM1(1st MIDI)*/
	{
	case 0x01://R00
		ret = (MIDI_Vector | MIDI_IntVect);
		MIDI_IntVect=0x10;
		break;
	case 0x03://R01
		break;
	case 0x05://R02
		ret = MIDI_IntFlag;
		break;
	case 0x07://R03
		break;
	case 0x09:			// R04, 14, ... 94
		switch(MIDI_RegHigh)
		{
		case 0:
			break;
		case 1:
			break;
		case 2:
			break;
		case 3:// R34 FIFO-Rx Status [Rdy OV FE PE BRK OL AHB Bsy]
			if(RxW_point > RxR_point){
			  ret = 0x80;// FIFOにデータあり(NoError)
			}
			break;
		case 4:
			break;
		case 5:// R54 FIFO-Tx Status [emp Rdy - -  - Idl - Bsy]
			if (MIDI_Buffered==0){
				  ret = 0xc0;// FIFO empty & Tx ready & Idl
			}
			else{
				if (MIDI_Buffered<MIDIFIFOSIZE){
				  ret = 0x41;// FIFO に空きがある 送信中
				}
				else{
				  ret = 0x01;// FIFO に空きがない 送信中
				}
			}
			break;
		case 6:
			break;
		case 7:
			break;
		case 8:
			break;
		case 9:
			break;
		}
		break;
	case 0x0b:			// R05, 15, ... 95
		switch(MIDI_RegHigh)
		{
		case 0:
			break;
		case 1:
			break;
		case 2:
			break;
		case 3:
			break;
		case 4:
			break;
		case 5:
			break;
		case 6:
			break;
		case 7:
			break;
		case 8:
			break;
		case 9:
			break;
		}
		break;
	case 0x0d:			// R06, 16, ... 96
		switch(MIDI_RegHigh)
		{
		case 0:
			break;
		case 1://  R16 Rx Read from FIFO
			if(RxW_point > RxR_point){
			  ret = Rx_buff[RxR_point];
			  RxR_point++;
			}
			if(RxR_point == RxW_point){
			  RxW_point = 0;
			  RxR_point = 0;
			}
			break;
		case 2:
			break;
		case 3://  R36
			if(RxW_point > RxR_point){
			  ret = Rx_buff[RxR_point];
			  RxR_point++;
			}
			if(RxR_point == RxW_point){
			  RxW_point = 0;
			  RxR_point = 0;
			}
			break;
		case 4:
			break;
		case 5:
			break;
		case 6:
			break;
		case 7:
			break;
		case 8:
			break;
		case 9:
			break;
		}
		break;
	case 0x0f:			// R07, 17, ... 97
		switch(MIDI_RegHigh)
		{
		case 0:
			break;
		case 1:// R17
			break;
		case 2:
			break;
		case 3:
			break;
		case 4:
			break;
		case 5:
			break;
		case 6:
			break;
		case 7:
			break;
		case 8:
			break;
		case 9:
			break;
		}
		break;
	default:
		break;
	}
	return ret;
}

/*
 Delayed input
*/
static void AddDelayBuf(uint8_t msg)
{
	int32_t newptr = (DBufPtrW+1)%MIDIDELAYBUF;
	if ( newptr!=DBufPtrR ) {
		DelayBuf[DBufPtrW].time = Get_msecCount();
		DelayBuf[DBufPtrW].msg  = msg;
		DBufPtrW = newptr;
	}
}

/*
 Deleyed Output
*/
void MIDI_DelayOut(uint32_t delay)
{
	uint64_t t = Get_msecCount();
	while ( DBufPtrW!=DBufPtrR ) {
		if ( (t-DelayBuf[DBufPtrR].time)>=delay ) {
			midi_data_out(DelayBuf[DBufPtrR].msg);
			DBufPtrR = (DBufPtrR+1)%MIDIDELAYBUF;
		} else
			break;
	}
}


// -----------------------------------------------------------------------
//   I/O Write
// -----------------------------------------------------------------------
void MIDI_Write(uint32_t adr, uint8_t data)
{
	switch(adr)
	{
	case 0xeafa00 ... 0xeafa0f: /*CZ-6BM1(this 1st MIDI)*/
	  if(Config.MIDI_SW){break;}
	case 0xeafa10 ... 0xeafaff: /*CZ-6BM1(2nd MIDI)*/
	case 0xeafb00 ... 0xeafbff: /*CZ-6BN1(parallel)*/
	case 0xeafc00 ... 0xeafdff: /*CZ-6BF1(RS232C 5ch)*/
	case 0xeafe00 ... 0xeaffff: /*CZ-6BG1(GPIB)*/
	default://wm_buserr
	 	BusErrFlag = 2;
		BusErrAdr = addr;
		return;
	}


	switch(adr & 0x0f)
	{
	case 0x01://R00
		break;
	case 0x03://R01
		MIDI_RegHigh = data&0x0f;
		if (data&0x80) MIDI_Init();
		break;
	case 0x05://R02
		break;
	case 0x07://R03 割り込みクリア
		MIDI_IntFlag &= (data ^ 0xff);
		break;
	case 0x09:			// R04, 14, ... 94
		switch(MIDI_RegHigh)
		{
		case 0:
			MIDI_Vector = (data&0xe0);
			break;
		case 1://R14
			break;
		case 2:
			break;
		case 3:
			break;
		case 4://R44
			break;
		case 5:
			break;
		case 6:
			break;
		case 7:
			break;
		case 8:
			MIDI_GTimerMax = (MIDI_GTimerMax&0xff00)|(uint32_t)data;
			break;
		case 9:
			break;
		}
		break;
	case 0x0b:			// R05, 15, ... 95
		switch(MIDI_RegHigh)
		{
		case 0:
			MIDI_R05 = data & 0x0f;
			break;
		case 1://R15
			break;
		case 2://R25
			break;
		case 3:// R35 clear
			if((data & 0x80) != 0){
			  RxW_point = 0;
			  RxR_point = 0;
			}
			MIDI_R35 = data;
			break;
		case 4:
			break;
		case 5://R55 Tx FIFO制御
			MIDI_R55 = data;
			break;
		case 6:
			break;
		case 7:
			break;
		case 8:
			MIDI_GTimerMax = (MIDI_GTimerMax&0xff)|(((uint32_t)(data&0x3f))*256);
			if (data&0x80)
				MIDI_GTimerVal = MIDI_GTimerMax*80;
			break;
		case 9:
			break;
		}
		break;
	case 0x0d:			// R06, 16, ... 96
		switch(MIDI_RegHigh)
		{
		case 0://R06 割り込みイネーブル
			MIDI_IntEnable = data;
			break;
		case 1:
			break;
		case 2:
			break;
		case 3:
			break;
		case 4:
			break;
		case 5:			// Out Data Byte
			if (!MIDI_Buffered)
				MIDI_BufTimer = MIDIBUFTIMER;
			MIDI_Buffered ++;
			AddDelayBuf(data);
			break;
		case 6:
			break;
		case 7:
			break;
		case 8:
			MIDI_MTimerMax = (MIDI_MTimerMax&0xff00)|(uint32_t)data;
			break;
		case 9:
			break;
		}
		break;
	case 0x0f:			// R07, 17, ... 97
		switch(MIDI_RegHigh)
		{
		case 0:
			break;
		case 1://R17
			if((data & 0x01) != 0){
			  if(RxW_point > RxR_point){ RxR_point++; }
			}
			break;
		case 2:
			break;
		case 3:
			break;
		case 4:
			break;
		case 5:
			break;
		case 6:
			break;
		case 7:
			break;
		case 8:
			MIDI_MTimerMax = (MIDI_MTimerMax&0xff)|(((uint32_t)(data&0x3f))*256);
			if (data&0x80)
				MIDI_MTimerVal = MIDI_MTimerMax*80;
			break;
		case 9:
			break;
		}
		break;
	default:
		break;
	}

 return;
}



