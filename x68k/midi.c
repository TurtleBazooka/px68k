// ---------------------------------------------------------------------------------------
//  MIDI.C - MIDI Board (CZ-6BM1) emulator
//                           Powered by ぷにゅさん〜
// ---------------------------------------------------------------------------------------

// 2022/3/21:Win/Mac/Linux 向け対応追加
// 4/18 未明: エクスクルーシヴがうまく通らないのを修正
// 4/18 朝　: レナムで鳴らなかったのを修正
// 4/18 昼　: エクスクルーシヴ送信完了を完全に待つ事でドラキュラおっけー

// ToDo: ・エクスクルーシヴ送信中(例えばドラキュラの音色設定中)に
//         流石に1MHzまで落ち込むのは…
//         MIDIの状態を返すポートを教えてもらうこと〜
//       ・MT-32でのランニングステータスの仕様をチェック
//       ・終了時の音源のリセット
//       ・IPLリセット時は実機でもリセットされない筈だが、
//         ここら辺は好みなので configで設定できてもいいかも

#include "common.h"
#include "prop.h"
#include "winx68k.h"
#include "dosio.h"
#include "x68kmemory.h"
#include "irqh.h"
#include "winui.h"
#include "m68000.h"
#include "midi.h"

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
	uint32_t time;
	uint8_t msg;
} DELAYBUFITEM;

static DELAYBUFITEM DelayBuf[MIDIDELAYBUF];
static int32_t DBufPtrW = 0;
static int32_t DBufPtrR = 0;

// ------------------------------------------------------------------
// ねこみぢ6、MIMPIトーンマップ対応関係
// ------------------------------------------------------------------

enum {
	MIMPI_LA = 0,
	MIMPI_PCM,
	MIMPI_GS,
	MIMPI_RHYTHM,
};

static	uint8_t		LOADED_TONEMAP = 0;
static	uint8_t		ENABLE_TONEMAP = 0;
static	uint8_t		TONE_CH[16];
static	uint8_t		TONEBANK[3][128];
static	uint8_t		TONEMAP[3][128];

// ------------------------------------------------------------------


static uint8_t EXCV_LARESET[] = { 0xf0, 0x41, 0x10, 0x16, 0x12, 0x7f, 0x01, 0xf7};
static uint8_t EXCV_GMRESET[] = { 0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7};
static uint8_t EXCV_GSRESET[] = { 0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7};
static uint8_t EXCV_XGRESET[] = { 0xf0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xf7};


#define	MIDI_EXCLUSIVE		0xf0
#define MIDI_TIMECODE		0xf1
#define MIDI_SONGPOS		0xf2
#define MIDI_SONGSELECT		0xf3
#define	MIDI_TUNEREQUEST	0xf6
#define	MIDI_EOX			0xf7
#define	MIDI_TIMING			0xf8
#define MIDI_START			0xfa
#define MIDI_CONTINUE		0xfb
#define	MIDI_STOP			0xfc
#define	MIDI_ACTIVESENSE	0xfe
#define	MIDI_SYSTEMRESET	0xff

// -----------------------------------------------------------------------
//   割り込み
// -----------------------------------------------------------------------
int32_t FASTCALL MIDI_Int(uint8_t irq)
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


// -----------------------------------------------------------------------
//   たいまを進める
// -----------------------------------------------------------------------
void FASTCALL MIDI_Timer(int32_t clk)
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


// -----------------------------------------------------------------------
//   MIDIモジュールの設定
// -----------------------------------------------------------------------
void MIDI_SetModule(void)
{
	if (Config.MIDI_SW)
		MIDI_MODULE = MIDI_ResetType[Config.MIDI_Type];
	else
		MIDI_MODULE = MIDI_NOTUSED;
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
		}else if(mid_outDevList(&hOut) > 0){ /* if MIDI found?*/
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
		}else if(mid_inDevList(&hIn) > 0){ /* if MIDI found?*/
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
uint8_t FASTCALL MIDI_Read(uint32_t adr)
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
	default:
	 	BusErrFlag = 1;
		return 0xff;
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
		DelayBuf[DBufPtrW].time = timeGetTime();
		DelayBuf[DBufPtrW].msg  = msg;
		DBufPtrW = newptr;
	}
}


void MIDI_DelayOut(uint32_t delay)
{
	uint32_t t = timeGetTime();
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
void FASTCALL MIDI_Write(uint32_t adr, uint8_t data)
{
	switch(adr)
	{
	case 0xeafa00 ... 0xeafa0f: /*CZ-6BM1(this 1st MIDI)*/
	  if(Config.MIDI_SW){break;}
	case 0xeafa10 ... 0xeafaff: /*CZ-6BM1(2nd MIDI)*/
	case 0xeafb00 ... 0xeafbff: /*CZ-6BN1(parallel)*/
	case 0xeafc00 ... 0xeafdff: /*CZ-6BF1(RS232C 5ch)*/
	case 0xeafe00 ... 0xeaffff: /*CZ-6BG1(GPIB)*/
	default:
	 	BusErrFlag = 1;
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


// -----------------------------------------------------------------------
// MIMPIトーンファイル読み込み（ねこみぢ6）
// -----------------------------------------------------------------------

static int32_t exstrcmp(char *str, char *cmp) {

	char	c;

	while(*cmp) {
		c = *str++;
		if ((c >= 'a') && (c <= 'z')) {
			c -= 0x20;
		}
		if (c != *cmp++) {
			return(TRUE);
		}
	}
	return(FALSE);
}

static void cutdelimita(char **buf) {

	char	c;

	for(;;) {
		c = **buf;
		if (!c) {
			break;
		}
		if (c > ' ') {
			break;
		}
		(*buf)++;
	}
}

static int32_t getvalue(char **buf, int32_t cutspace) {

	int32_t		ret = 0;
	int32_t		valhit = 0;
	char		c;

	if (cutspace) {
		cutdelimita(buf);
	}
	for (;; valhit=1) {
		c = **buf;
		if (!c) {
			if (!valhit) {
				return(-1);
			}
			else {
				break;
			}
		}
		if ((c < '0') || (c > '9')) {
			break;
		}
		ret = ret * 10 + (c - '0');
		(*buf)++;
	}
	return(ret);
}

static int32_t file_readline(FILEH fh, char *buf, int32_t len) {

	int32_t		pos;
	int32_t		readsize;
	int32_t		i;

	if (len < 2) {
		return(-1);
	}
	pos = File_Seek(fh, 0, FSEEK_CUR);
	if (pos == -1) {
		return(-1);
	}
	readsize = File_Read(fh, buf, len-1);
	if (readsize == -1) {
		return(-1);
	}
	if (!readsize) {
		return(-1);
	}
	for (i=0; i<readsize; i++) {
		pos++;
		if ((buf[i] == 0x0a) || (buf[i] == 0x0d)) {
			break;
		}
	}
	buf[i] = '\0';
	if (File_Seek(fh, pos, FSEEK_SET) != pos) {
		return(-1);
	}
	return(i);
}

static void mimpidefline_analaize(char *buf) {

	cutdelimita(&buf);
	if (*buf == '@') {
		int		ch;
		buf++;
		ch = getvalue(&buf, FALSE);
		if ((ch < 1) || (ch > 16)) {
			return;
		}
		ch--;
		cutdelimita(&buf);
		if (!exstrcmp(buf, "LA")) {
			TONE_CH[ch] = MIMPI_LA;
		}
		else if (!exstrcmp(buf, "PCM")) {
			TONE_CH[ch] = MIMPI_PCM;
		}
		else if (!exstrcmp(buf, "GS")) {
			TONE_CH[ch] = MIMPI_GS;
		}
		else if (!exstrcmp(buf, "RHYTHM")) {
			TONE_CH[ch] = MIMPI_RHYTHM;
		}
	}
	else {
		int	mod, num, bank, tone;
		mod = getvalue(&buf, FALSE);
		if ((mod < 0) || (mod >= MIMPI_RHYTHM)) {
			return;
		}
		num = getvalue(&buf, TRUE);
		if ((num < 1) || (num > 128)) {
			return;
		}
		num--;
		tone = getvalue(&buf, TRUE);
		if ((tone < 1) || (tone > 128)) {
			return;
		}
		if (*buf == ':') {
			buf++;
			bank = tone - 1;
			tone = getvalue(&buf, TRUE);
			if ((tone < 1) || (tone > 128)) {
				return;
			}
			TONEBANK[mod][num] = bank;
		}
		TONEMAP[mod][num] = tone-1;
	}
}

int32_t MIDI_SetMimpiMap(char *filename) {

	uint8_t		b;
	FILEH		fh;
	char		buf[128];

	LOADED_TONEMAP = 0;
	memset(TONE_CH,     0, sizeof(TONE_CH));
	memset(TONEBANK[0], 0, sizeof(TONEBANK));
	for (b=0; b<128; b++) {
		TONEMAP[0][b] = b;
		TONEMAP[1][b] = b;
		TONEMAP[2][b] = b;
	}
	TONE_CH[9] = MIMPI_RHYTHM;

	if ((filename == NULL) || (!filename[0])) {
		ENABLE_TONEMAP = 0;
		return(FALSE);
	}
	fh = File_Open(filename);
	if (fh == (FILEH)-1) {
		ENABLE_TONEMAP = 0;
		return(FALSE);
	}
	while(file_readline(fh, buf, sizeof(buf)) >= 0) {
		mimpidefline_analaize(buf);
	}
	File_Close(fh);

	LOADED_TONEMAP = 1;
	return(TRUE);
}

int32_t MIDI_EnableMimpiDef(int32_t enable) {

	ENABLE_TONEMAP = 0;
	if ((enable) && (LOADED_TONEMAP)) {
		ENABLE_TONEMAP = 1;
		return(TRUE);
	}
	return(FALSE);
}

// -----------------------------------------------------------------------
