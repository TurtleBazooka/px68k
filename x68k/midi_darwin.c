/*
 macOS用CoreMIDI 対応の関数群
 Win.のMIDI関数とコンパチ(winmm.lib)で置き換えてます。
 CoreMIDIで検出された機器をリストします。
 （MIMPIによる音色変換は削除しました）
 
 2022/3/27  by kameya
*/

#import <CoreMIDI/MIDIServices.h>
#import <CoreMIDI/CoreMIDI.h>
#import <mach/mach_time.h>

#include "prop.h"
#include "winui.h"
#include "x68kmemory.h"
#include "irqh.h"
#include "dosio.h"


	/* for CoreMIDI */
	MIDIClientRef mid_client;
	MIDIPortRef mid_out_port;
	MIDIEndpointRef mid_endpoint;
	MIDIDeviceRef mid_device;
	MIDIPacket* mid_Packet = 0;

	MIDIPortRef mid_in_port;
	MIDIEndpointRef mid_source;

	const uint8_t packetBuf[200];
	MIDIPacketList *packetList = (MIDIPacketList *)packetBuf;

	const char mid_name[] = {"px68k-MIDI"};


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

static uint8_t EXCV_LARESET[] = { 0xf0, 0x41, 0x10, 0x16, 0x12, 0x7f, 0x01, 0xf7};
static uint8_t EXCV_GMRESET[] = { 0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7};
static uint8_t EXCV_GSRESET[] = { 0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7};
static uint8_t EXCV_XGRESET[] = { 0xf0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xf7};

// -----------------------------------------------------------------------
//   割り込み
// -----------------------------------------------------------------------
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

// -----------------------------------------------------------------------
//   たいまを進める
// -----------------------------------------------------------------------
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

/*
 call back from OS
*/
void
mid_In_callback(const MIDIPacketList* packetList,
				  void* readProcRefCon,
				  void* srcConnRefCon)
{
  if((MIDI_R35 & 0x01) == 0x00) return;//Rx-FIFO 受信禁止

  MIDIPacket *packet = (MIDIPacket *)packetList->packet;
  uint32_t count = packetList->numPackets;
  for(uint32_t j=0; j<count; j++){
   for(uint32_t i=0; i<packet->length; i++){
	 Rx_buff[RxW_point] = packet->data[i];
	 if(RxW_point < 250){ RxW_point++; }// buffer full
   }
   packet = MIDIPacketNext(packet);
  }

  if(MIDI_IntEnable & 0x20){// 割り込み許可？
    MIDI_IntFlag |= 0x20; // Rx int 発生
    MIDI_IntVect =  0x0a; // set vector
    IRQH_Int(4, &MIDI_Int);//int 4
  }

 return;
}

/*
   MIDI out port Open (CoreMIDI synth)
*/
uint32_t
mid_outDevList()
{
	uint32_t Device_num = 0;
	OSStatus err_sts;

	/* Create Client for coreMIDI */
	err_sts = MIDIClientCreate(CFSTR("px68k"), NULL, NULL, &mid_client);
	if (err_sts != noErr)
	{
		p6logd("MIDI:CoreMIDI: No out client created.\n");
		return Device_num;
	}

	// Create OutPort for client 
	err_sts = MIDIOutputPortCreate(mid_client, CFSTR("px68k MIDI out_Port"), &mid_out_port);

	if (err_sts != noErr)
	{
		p6logd("MIDI:CoreMIDI: No out port created.\n");
		return Device_num;
	}

	/* Get the MIDIEndPoint */
	mid_endpoint = 0;
	//uint32_t core_mid_num = MIDIGetNumberOfDevices();//仮想ポート含まない
	//mid_endpoint = MIDIGetDevice(core_mid_num);
	uint32_t core_mid_num = (uint32_t)MIDIGetNumberOfDestinations();//仮想ポート含む
	if (core_mid_num == 0)
	{
		  return Device_num; // No found
	}

	/* Store MIDI out port LIST */
	CFStringRef strRef,epstrRef,devstrRef;
	MIDIEntityRef mid_entity;
	 for(uint32_t i = 0; i<core_mid_num; i++){
		mid_endpoint = MIDIGetDestination(i);
		MIDIEndpointGetEntity(mid_endpoint, &mid_entity);
		MIDIEntityGetDevice(mid_entity, &mid_device);
		if((mid_endpoint) && (i<8)){// MAX item check(８個までLISTに制限)
			MIDIObjectGetStringProperty(mid_endpoint, kMIDIPropertyName, &epstrRef);
			MIDIObjectGetStringProperty(mid_device, kMIDIPropertyName, &devstrRef);
			if (devstrRef != NULL && CFStringCompare(epstrRef, devstrRef, 0) != kCFCompareEqualTo) {
			 strRef = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@:%@"), devstrRef, epstrRef);
			} else {
			 strRef = epstrRef;
			}
			//kCFStringEncodingUTF8：UTF8でポートの名前(string)を取り出す
			CFStringGetCString(strRef, menu_items[MIDI_OUT_menu_item][Device_num], sizeof(menu_items[MIDI_OUT_menu_item][Device_num]), kCFStringEncodingUTF8);
			p6logd("Find MIDI out:%s\n",menu_items[MIDI_OUT_menu_item][Device_num]);
			Device_num ++;
		}
	 }
	CFRelease(strRef);
	strcpy(menu_items[MIDI_OUT_menu_item][Device_num],"\0"); // Menu END 

return core_mid_num;
}

/*
 Search and Store MIDI in Device LIST 
*/
uint32_t
mid_inDevList()
{

  uint32_t Device_num = 0;
  OSStatus err_sts;

  // Create Client for coreMIDI
  //err_sts = MIDIClientCreate(CFSTR("px68k"), NULL, NULL, &mid_client);

  //if (err_sts != noErr)
  //{
  //  p6logd("MIDI:CoreMIDI: No in client created.\n");
  //  return Device_num;
  //}

  // Create input port
  err_sts = MIDIInputPortCreate(mid_client, CFSTR("px68k MIDI in_Port"),
                            mid_In_callback, NULL, &mid_in_port);
  if (err_sts != noErr)
  {
    p6logd("MIDI:CoreMIDI: No port in created.\n");
    return Device_num; // No found
  }

  uint32_t core_mid_num = (uint32_t)MIDIGetNumberOfSources();
  if (core_mid_num == 0)
  {
    return Device_num; // No found
  }

	/* Store MIDI in port LIST */
	mid_source = 0;
	CFStringRef strRef,epstrRef,devstrRef;
	MIDIEntityRef mid_entity;
	 for(uint32_t i = 0; i<core_mid_num; i++){
	  mid_source = MIDIGetSource(i);
	  MIDIEndpointGetEntity(mid_source, &mid_entity);
	  MIDIEntityGetDevice(mid_entity, &mid_device);
	  if((mid_source) && (i<8)){// MAX item check(８個までLISTに制限)
	   MIDIObjectGetStringProperty(mid_source, kMIDIPropertyName, &epstrRef);
	   MIDIObjectGetStringProperty(mid_device, kMIDIPropertyName, &devstrRef);
	   if (devstrRef != NULL && CFStringCompare(epstrRef, devstrRef, 0) != kCFCompareEqualTo) {
	    strRef = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@:%@"), devstrRef, epstrRef);
	   } else {
	    strRef = epstrRef;
	   }
	   //kCFStringEncodingUTF8：UTF8でポートの名前(string)を取り出す
	   CFStringGetCString(strRef, menu_items[MIDI_IN_menu_item][Device_num], sizeof(menu_items[MIDI_IN_menu_item][Device_num]),kCFStringEncodingUTF8);
	   p6logd("Find MIDI in :%s\n",menu_items[MIDI_IN_menu_item][Device_num]);
	   Device_num ++;
	  }
	 }
	CFRelease(strRef);
	strcpy(menu_items[MIDI_IN_menu_item][Device_num],"\0"); // Menu END

return core_mid_num;
}


/*-----------------------------------------
   set/change MIDI in Port 
------------------------------------------*/
void midInChg(uint32_t port_no)
{
	OSStatus err_sts;

	/* CoreMIDI endpoint change */
	mid_source = MIDIGetSource(port_no);
	err_sts = MIDIPortConnectSource(mid_in_port, mid_source, NULL);
	if (err_sts != noErr){
		p6logd("MIDI in Change error.\n");
	}

 return;
}

/*-----------------------------------------
   MIDI Port close
------------------------------------------*/
uint32_t
midiOutClose()
{

	OSStatus err_sts;

	/* Disconnect in Port */
	err_sts = MIDIPortDisconnectSource(mid_in_port, mid_source);
	if (err_sts != noErr) p6logd("Disconnect MIDI-Source err\n");
	hIn  = 0;

	/* Dispose out Port */
	err_sts = MIDIPortDispose(mid_out_port);
	if (err_sts != noErr) p6logd("Dispose MIDI-out Port err\n");
	//err_sts = MIDIPortDispose(mid_in_port);
	//if (err_sts != noErr) p6logd("Dispose MIDI-in Port err\n");
	hOut = 0;

	/* Dispose out endpoint */
	//err_sts = MIDIEndpointDispose(mid_endpoint);
	//if (err_sts != noErr) p6logd("Dispose MIDI-out Endpoint err\n");
	//err_sts = MIDIEndpointDispose(mid_source);
	//if (err_sts != noErr) p6logd("Dispose MIDI-in Endpoint err\n");

	/* Dispose Client */
	err_sts = MIDIClientDispose(mid_client);
	if (err_sts != noErr) p6logd("Dispose MIDI-Client err\n");

	return TRUE;
}

// -----------------------------------------------------------------------
//   MIDI packet送信
// -----------------------------------------------------------------------
void MIDI_SendPacket(uint8_t *excv, int32_t length)
{
	// エクスクルーシヴ送信
	if(length == 0){ //length check
	  return;
	}

	/*(CoreMIDI) initialize MIDIPacketList */
	mid_Packet = MIDIPacketListInit(packetList);

	/*(CoreMIDI) 電文とタイムスタンプ入れてPacketList更新 */
	MIDIPacketListAdd(packetList, (ByteCount)sizeof(packetBuf), mid_Packet, 
				mach_absolute_time(), length, (uint8_t *)excv);

	/*(CoreMIDI) PacketList送信 */
	MIDISend(mid_out_port,mid_endpoint,packetList);

	return;

}

/*-----------------------------------------
   set/change MIDI out Port 
------------------------------------------*/
void midOutChg(uint32_t port_no, uint32_t bank)
{
	OSStatus err_sts;


	/* All note off */
	 uint8_t txbuf[5];
	 for (uint8_t msg=0xb0; msg<0xbf; msg++) {// all note off(消音)
	   txbuf[0] = msg; txbuf[1] = 0x7b; txbuf[2] = 0x00;
	   MIDI_SendPacket(txbuf,3);
	 }

	/* CoreMIDI endpoint change */
	mid_endpoint = MIDIGetDestination(port_no);
	if (mid_endpoint == 0){
		p6logd("MIDI Change error.\n");
	}

	/* select BANK CC20 LSB */
	//txbuf[0] = 0xb0; txbuf[1] = 0x20; txbuf[2] = (bank & 0xff);/*Bank select2*/
	//MIDI_SendPacket(txbuf,3);

 return;
}

// -----------------------------------------------------------------------
//   send RESET
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
				MIDI_SendPacket(EXCV_LARESET, sizeof(EXCV_LARESET));
				break;
			case MIDI_SC55:
			case MIDI_SC88:
			case MIDI_GS:
				MIDI_SendPacket(EXCV_GSRESET, sizeof(EXCV_GSRESET));
				break;
			case MIDI_XG:
				MIDI_SendPacket(EXCV_XGRESET, sizeof(EXCV_XGRESET));
				break;
			default:
				MIDI_SendPacket(EXCV_GMRESET, sizeof(EXCV_GMRESET));
				break;
		}
	   uint8_t txbuf[5];
	   for (uint8_t msg=0xb0; msg<0xbf; msg++) {// all note off(消音)
	     txbuf[0] = msg; txbuf[1] = 0x7b; txbuf[2] = 0x00; 
	     MIDI_SendPacket(txbuf,3);
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

	//MIDI_EXCVWAIT = 0;

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

	if (Config.MIDI_SW && Config.MIDI_Reset)  MIDI_Reset();// りせっと〜

 return;
}

// -----------------------------------------------------------------------
//   撤収〜
// -----------------------------------------------------------------------
void MIDI_Cleanup(void) {

	if (hOut) {
	  uint8_t txbuf[5];
	  for (uint8_t msg=0xb0; msg<0xbf; msg++) {// all note off(消音)
	     txbuf[0] = msg; txbuf[1] = 0x7b; txbuf[2] = 0x00; 
	     MIDI_SendPacket(txbuf,3);
	  }

	  if (Config.MIDI_SW && Config.MIDI_Reset)  MIDI_Reset();// りせっと〜

	  midiOutClose();
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
      MIDI_SendPacket(Tx_buff, leng);
      Tx_ptr = 1;
    }
    else{
     Tx_ptr ++;
    }
    break;
  case 0xf0:
    if((Tx_buff[Tx_ptr-1] == 0xf7)&&(data != 0xf7)){//exclusive message and end.
      MIDI_SendPacket(Tx_buff,Tx_ptr);
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

// Make Delay in
static void AddDelayBuf(uint8_t msg)
{
	int32_t newptr = (DBufPtrW+1)%MIDIDELAYBUF;
	if ( newptr!=DBufPtrR ) {
		DelayBuf[DBufPtrW].time = Get_msecCount();
		DelayBuf[DBufPtrW].msg  = msg;
		DBufPtrW = newptr;
	}
}

// Make Delay out
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
	default:
	 	BusErrFlag = 1;
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

