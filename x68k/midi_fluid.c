/*
 fluidsynth利用のMIDI出力ルーチン群
 要 framework版/共有ライブラリ版libfluidsynth
 要 SoundFont2(Configで指定)
 libfluidsynth is under LGPLv2.1 licence なんだけど
 LINK(共有ライブラリ)するだけならその限りではないそうです。

 2022/5/29  by kameya (このソースはご自由にご利用ください)
*/
#include <fluidsynth.h>
#include <stdint.h>
#include "x68kmemory.h"
#include "prop.h"
#include "winui.h"
#include "irqh.h"
#include "dosio.h"

// for each machine.
// for Fluidsynth MIDI Library

	/*fluid set*/
    fluid_settings_t* settings;
    fluid_synth_t* synth;
    fluid_audio_driver_t* adriver;
	fluid_sequencer_t* sequencer;

	const char mid_name[] = {"px68k-MIDI"};
	const char synth_name[] = {"fluidsynth"};

#define sf_default	"/usr/local/share/soundfonts/default.sf2"

// for  CZ-6BM1 I/F
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

uint32_t	hOut = 0;
uint32_t	hIn  = 0;

uint8_t		MIDI_EXCVBUF[MIDIBUFFERS];

static uint32_t Tx_ptr;
static uint8_t  Tx_buff[MIDIBUFFERS];

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

// ------------------------------------------------------------------

static char EXCV_LARESET[] = { 0xf0, 0x41, 0x10, 0x16, 0x12, 0x7f, 0x01, 0xf7};
static char EXCV_GMRESET[] = { 0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7};
static char EXCV_GSRESET[] = { 0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7};
static char EXCV_XGRESET[] = { 0xf0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xf7};

/*
  fluid_synth Open
  and Load SoundFont2 (Loadできないと音出ないよ)
*/
uint32_t
mid_outDevList(void)
{
	uint32_t Device_num = 0;

	/*==setting synth==*/
	settings = new_fluid_settings();
	if(!settings) return 0; // fluid error
	//fluid_settings_setstr(settings, "audio.driver", "dsound"); //win
	//fluid_settings_setstr(settings, "audio.driver", "coreaudio"); //mac
	//fluid_settings_setstr(settings, "audio.driver", "alsa"); //Linux
	fluid_settings_setint(settings, "synth.polyphony", 128);
	fluid_settings_setint(settings, "synth.reverb.active", FALSE);

    /*==Set in synthesizer ==*/
    synth = new_fluid_synth(settings);
    adriver = new_fluid_audio_driver(settings, synth);
	sequencer = new_fluid_sequencer2(0);

	/*++ Load SoundFont.sf2 ++*/
	int32_t fluid_res;
	if(Config.SoundFontFile[0]){
		fluid_res = fluid_synth_sfload(synth, (char*)Config.SoundFontFile, 1);
		if(fluid_res > 0){
		  p6logd("fluidsynth:Loading SoundFont %s OK\n",(char*)Config.SoundFontFile);
		}
	}
	else{
		fluid_res = fluid_synth_sfload(synth, sf_default, 1);
		if(fluid_res < 0){
		  p6logd("fluidsynth:Can't load SoundFont...\n");
		}
	}

	/* set menu */
	strcpy(menu_items[MIDI_OUT_menu_item][Device_num],synth_name);
	Device_num ++;
	strcpy(menu_items[MIDI_OUT_menu_item][Device_num],"\0"); /* Menu END */

 return 1;
}

/* No support */
uint32_t
mid_inDevList(void)
{
 return 0;
}

// ---Exception ---
int32_t MIDI_Int(uint8_t irq)
{
	int32_t ret = -1;

	IRQH_IRQCallBack(irq);
	if ( irq==4 )
	{
		ret = (int32_t)(MIDI_Vector|MIDI_IntVect);
	}

	return ret;
}


// --- Timer ---
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
				MIDI_IntVect  = 0x0c;
				IRQH_Int(4, &MIDI_Int);
			}
		}
	}

	if (MIDI_MTimerMax)
	{
		 MIDI_MTimerVal -= clk;
		if (MIDI_MTimerVal<0)		// 割り込み発生（魔法大作戦）
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


// --- kind of MIDI module ---
void MIDI_SetModule(void)
{
	if (Config.MIDI_SW)
		MIDI_MODULE = MIDI_ResetType[Config.MIDI_Type];
	else
		MIDI_MODULE = MIDI_NOTUSED;
}

// --- send short mess. ---
uint32_t
midiOutShortMsg( uint8_t messg0, uint8_t messg1 , uint8_t messg2)
{
	int32_t val;

	/* Send msg */
	switch(messg0 & 0xf0){
		case 0xc0://prog. chg
			fluid_synth_program_change(synth,messg0&0x0f, messg1);
			break;
		case 0xd0://chnnel press.
			fluid_synth_channel_pressure(synth,messg0&0x0f, messg1);
			break;
		case 0x80://note off (chan key vel)
			fluid_synth_noteoff(synth, messg0&0x0f, messg1);
			break;
		case 0x90://     on
			fluid_synth_noteon(synth, messg0&0x0f, messg1, messg2);
			break;
		case 0xa0://key press.
			fluid_synth_key_pressure(synth, messg0&0x0f, messg1, messg2);//fluid v1.xx not implement
			break;
		case 0xb0://cont.chg
			fluid_synth_cc(synth, messg0&0x0f, messg1, messg2);
			break;
		case 0xe0://pitch wheel chg
			val = (messg2 << 7 | messg1) - 2^13 ;
			fluid_synth_pitch_wheel_sens(synth, messg0&0x0f, val);
			break;
		case 0xf0://ないと思うけど
			break;
		default://ありえないハズ
			break;
	}

	return 1;
}


// --- set/change MIDI out Port and select BANK ---
void midOutChg(uint32_t port_no, uint32_t bank)
{

	/* All note off (control change)*/
	 for (uint8_t msg=0x00; msg<0x0f; msg++) {// all note off(消音)
	   fluid_synth_cc(synth, msg, 0x7b, 0x00);
	 }

	/* select BANK CC20 LSB */
	//uint32_t msg = ((bank << 16) | (0x20 << 8) | 0xb0);/*Bank select2*/
	//midiOutShortMsg( msg);

	return;
}


// ---   set/change MIDI in Port ---
void midInChg(uint32_t port_no)
{
	return;// not use
}


// --- MIDI Port close ---
uint32_t
midiOutClose()
{

	if(adriver)
        delete_fluid_audio_driver(adriver);

    if(synth)
        delete_fluid_synth(synth);

    if(settings)
        delete_fluid_settings(settings);

	return 1;
}


//  --- Send reset command ---
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
				fluid_synth_sysex(synth, EXCV_LARESET, sizeof(EXCV_LARESET), NULL, NULL, NULL, 0);
				break;
			case MIDI_SC55:
			case MIDI_SC88:
			case MIDI_GS:
				fluid_synth_sysex(synth, EXCV_GSRESET, sizeof(EXCV_GSRESET), NULL, NULL, NULL, 0);
				break;
			case MIDI_XG:
				fluid_synth_sysex(synth, EXCV_XGRESET, sizeof(EXCV_XGRESET), NULL, NULL, NULL, 0);
				break;
			default:
				fluid_synth_sysex(synth, EXCV_GMRESET, sizeof(EXCV_GMRESET), NULL, NULL, NULL, 0);
				break;
		}
		/* All note off (control change)*/
		for (uint8_t msg=0x00; msg<0x0f; msg++) {// all note off(消音)
		  fluid_synth_cc(synth, msg, 0x7b, 0x00);
		}
	}
}



//  --- initialize ---
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
		 hOut = 1;
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
		 hIn = 1;
		}
		else{
		 strcpy(menu_items[MIDI_IN_menu_item][0],"No Device Found.");
		 strcpy(menu_items[MIDI_IN_menu_item][1],"\0"); /* Menu END*/
		 hIn = 0;
		}
	}

	if (Config.MIDI_SW && Config.MIDI_Reset)  MIDI_Reset();// reset comm.

 return;
}

// --- on the QUIT ---
void MIDI_Cleanup(void) {

	if (hOut) {
	  /* All note off (control change)*/
	  for (uint8_t msg=0x00; msg<0x0f; msg++) {// all note off(消音)
	   fluid_synth_cc(synth, msg, 0x7b, 0x00);
	  }

	  if (Config.MIDI_SW && Config.MIDI_Reset)  MIDI_Reset();// りせっと〜
	  midiOutClose();
	  hOut = 0;
	}
	if (hIn) {
	  hIn = 0;
	}
}

/* --- New MIDI Packet sender ---*/
/*     by kameya 2023/07/07   */
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
   default:// 基本は3
	leng = 3;
	break;
  }

  switch(Tx_buff[0]){ // ===send command===
  case 0x80 ... 0xef:
  case 0xf1 ... 0xff:
    if(data>0x7f){
      Tx_buff[0] = data; // Store Data
      Tx_ptr = 1;
      break;
    }
    if(Tx_ptr == (leng -1)){ // short message 
      midiOutShortMsg( Tx_buff[0] , Tx_buff[1] , Tx_buff[2]);
      Tx_ptr = 1;
    }
    else{
     Tx_ptr ++;
    }
    break;
  case 0xf0:
    if((Tx_buff[Tx_ptr-1] == 0xf7)&&(data != 0xf7)){//exclusive message and end.
      fluid_synth_sysex(synth, (char *)Tx_buff, Tx_ptr, NULL, NULL, NULL, 0);
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


//  --- I/O Read ---
// for  CZ-6BM1 I/F and exp. slot
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


static void AddDelayBuf(uint8_t msg)
{
	int32_t newptr = (DBufPtrW+1)%MIDIDELAYBUF;

	if ( newptr!=DBufPtrR ) {
		DelayBuf[DBufPtrW].time = Get_msecCount();
		DelayBuf[DBufPtrW].msg  = msg;
		DBufPtrW = newptr;
	}
}


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


//  --- I/O Write ---
// for  CZ-6BM1 I/F and exp. slot
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
