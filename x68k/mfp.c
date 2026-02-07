// ---------------------------------------------------------------------------------------
//  MFP.C - MFP (Multi-Function Periferal)
// ---------------------------------------------------------------------------------------

#include "mfp.h"
#include "rtc.h"
#include "irqh.h"
#include "crtc.h"
#include "m68000.h"
#include "windraw.h"
#include "winx68k.h"
#include "keyboard.h"

extern uint8_t traceflag;
uint8_t testflag=0;
uint8_t LastKey = 0;

uint8_t MFP[24];
uint8_t Timer_TBO = 0;
static uint8_t Timer_Reload[4] = {0, 0, 0, 0};
static int32_t Timer_Tick[4] = {0, 0, 0, 0};
static const int32_t Timer_Prescaler[8] = {1, 10, 25, 40, 125, 160, 250, 500};

uint8_t keyLED;
uint8_t keyREP_DELAY;
uint8_t keyREP_TIME;

// -----------------------------------------------------------------------
//   優先割り込みのチェックをし、該当ベクタを返す
// -----------------------------------------------------------------------
int32_t FASTCALL MFP_IntCallback(uint8_t irq)
{
	uint8_t flag;
	int32_t vect;
	int32_t offset = 0;
	IRQH_IRQCallBack(irq);
	if (irq!=6) return -1;
	for (flag=0x80, vect=15; flag; flag>>=1, vect--)
	{
		if ((MFP[MFP_IPRA]&flag)&&(MFP[MFP_IMRA]&flag)&&(!(MFP[MFP_ISRA]&flag)))
			break;
	}
	if (!flag)
	{
		offset = 1;
		for (flag=0x80, vect=7; flag; flag>>=1, vect--)
		{
			if ((MFP[MFP_IPRB]&flag)&&(MFP[MFP_IMRB]&flag)&&(!(MFP[MFP_ISRB]&flag)))
				break;
		}
	}
	if (!flag)
	{
		Error("MFP Int w/o Request. Default Vector(-1) has been returned.");
		return -1;
	}

	MFP[MFP_IPRA+offset] &= (~flag);
	if (MFP[MFP_VR]&8)
		MFP[MFP_ISRA+offset] |= flag;
	vect |= (MFP[MFP_VR]&0xf0);
	for (flag=0x80; flag; flag>>=1)
	{
		if ((MFP[MFP_IPRA]&flag)&&(MFP[MFP_IMRA]&flag)&&(!(MFP[MFP_ISRA]&flag)))
		{
			IRQH_Int(6, &MFP_IntCallback);
			break;
		}
		if ((MFP[MFP_IPRB]&flag)&&(MFP[MFP_IMRB]&flag)&&(!(MFP[MFP_ISRB]&flag)))
		{
			IRQH_Int(6, &MFP_IntCallback);
			break;
		}
	}
	return vect;
}


// -----------------------------------------------------------------------
//   割り込みが取り消しになってないか調べます
// -----------------------------------------------------------------------
void MFP_RecheckInt(void)
{
	uint8_t flag;
	IRQH_IRQCallBack(6);
	for (flag=0x80; flag; flag>>=1)
	{
		if ((MFP[MFP_IPRA]&flag)&&(MFP[MFP_IMRA]&flag)&&(!(MFP[MFP_ISRA]&flag)))
		{
			IRQH_Int(6, &MFP_IntCallback);
			break;
		}
		if ((MFP[MFP_IPRB]&flag)&&(MFP[MFP_IMRB]&flag)&&(!(MFP[MFP_ISRB]&flag)))
		{
			IRQH_Int(6, &MFP_IntCallback);
			break;
		}
	}
}


// -----------------------------------------------------------------------
//   割り込み発生
// -----------------------------------------------------------------------
void MFP_Int(int32_t irq)		// 'irq' は 0が最優先（HSYNC/GPIP7）、15が最下位（ALARM）
{				// ベクタとは番号の振り方が逆になるので注意〜
	uint8_t flag = 0x80;
	if (irq<8)
	{
		flag >>= irq;
		if (MFP[MFP_IERA]&flag)
		{
			MFP[MFP_IPRA] |= flag;
			if ((MFP[MFP_IMRA]&flag)&&(!(MFP[MFP_ISRA]&flag)))
			{
				IRQH_Int(6, &MFP_IntCallback);
			}
		}
	}
	else
	{
		irq -= 8;
		flag >>= irq;
		if (MFP[MFP_IERB]&flag)
		{
			MFP[MFP_IPRB] |= flag;
			if ((MFP[MFP_IMRB]&flag)&&(!(MFP[MFP_ISRB]&flag)))
			{
				IRQH_Int(6, &MFP_IntCallback);
			}
		}
	}
}


// -----------------------------------------------------------------------
//   初期化
// -----------------------------------------------------------------------
void MFP_Init(void)
{
	int_fast32_t i;
	static const uint8_t initregs[24] = {
		0x7b, 0x06, 0x00, 0x18, 0x3e, 0x00, 0x00, 0x00,
		0x00, 0x18, 0x3e, 0x40, 0x08, 0x01, 0x77, 0x01,
		0x0d, 0xc8, 0x14, 0x00, 0x88, 0x01, 0x81, 0x00
	};
	memcpy(MFP, initregs, 24);
	for (i=0; i<4; i++) Timer_Tick[i] = 0;
}


// -----------------------------------------------------------------------
//   I/O Read
// -----------------------------------------------------------------------
uint8_t FASTCALL MFP_Read(uint32_t adr)
{
	uint8_t ret = 0xff;
	uint8_t reg = (uint8_t)((adr&0x3f)>>1);// Reg0~23(31)

	switch(adr & 0x3f)
	{
	case 0x01://Reg00  [HSYNC][CIRQ][1][H-SYNC][FMIRQ][POWSW][EXPON:1][ALM]
		ret = 0x22;//default

		if(RTC_Timer(0)!=0)//ALM 1Hz and 16Hz 出力レベル
		{
			ret |= 0x01;
		}

		if ( (vline>=CRTC_VSTART)&&(vline<CRTC_VEND) )//V-DISP 1:垂直表示 0:帰線時間
		{
			ret |= 0x10;
		}

		int32_t hpos = (int32_t)(ICount%HSYNC_CLK);//H-SYNC
		if ( (hpos>=((int32_t)CRTC_Regs[5]*HSYNC_CLK/CRTC_Regs[1]))&&(hpos<((int32_t)CRTC_Regs[7]*HSYNC_CLK/CRTC_Regs[1])) )
			ret &= 0x7f;
		else
			ret |= 0x80;

		if (vline!=CRTC_IntLine)//ラスタ割込 1:要求なし
		{
			ret |= 0x40;
		}
		break;
	case 0x03://Reg01
	case 0x05://Reg02
	case 0x07://Reg03
	case 0x09://Reg04
	case 0x0b://Reg05
	case 0x0d://Reg06
	case 0x0f://Reg07
	case 0x11://Reg08
	case 0x13://Reg09
	case 0x15://Reg10
	case 0x17://Reg11
	case 0x19://Reg12
	case 0x1b://Reg13
	case 0x1d://Reg14
	case 0x1f://Reg15
	case 0x21://Reg16
	case 0x23://Reg17
	case 0x25://Reg18
	case 0x27://Reg19
	case 0x29://Reg20
	case 0x2d://Reg22
		ret = MFP[reg];
		break;
	case 0x2b://Reg21
		if (KeyBufRP!=KeyBufWP)
			ret = MFP[reg] & 0x7f;
		else
			ret = MFP[reg] | 0x80;
		break;
	case 0x2f://Reg23
		ret = LastKey;
		KeyIntFlag = 0;
		break;
	default:
		break;
	}

	return ret;

}


// -----------------------------------------------------------------------
//   I/O Write
// -----------------------------------------------------------------------
void FASTCALL MFP_Write(uint32_t adr, uint8_t data)
{
	uint8_t reg=(uint8_t)((adr&0x3f)>>1);// Reg0~23(31)

	switch(adr & 0x3f)
	{
	case 0x01://Reg00 (保存するだけ)
	case 0x03://Reg01
	case 0x05://Reg02
		MFP[reg] = data;
		break;
	case 0x07://Reg03
	case 0x09://Reg04
		MFP[reg] = data;
		MFP[reg+2] &= data;  // 禁止されたものはIPRA/Bを落とす
		MFP_RecheckInt();
		break;
	case 0x0b://Reg05
	case 0x0d://Reg06
	case 0x0f://Reg07
	case 0x11://Reg08
		MFP[reg] &= data;
		MFP_RecheckInt();
		break;
	case 0x13://Reg09
	case 0x15://Reg10
		MFP[reg] = data;
		MFP_RecheckInt();
		break;
	case 0x17://Reg11
	case 0x19://Reg12
		MFP[reg] = data;
		break;
	case 0x1b://Reg13
		MFP[reg] = data;
		if ( MFP[reg]&0x10 ) Timer_TBO = 0;
		break;
	case 0x1d://Reg14
		MFP[reg] = data;
		break;
	case 0x1f://Reg15
		Timer_Reload[0] = MFP[reg] = data;
		break;
	case 0x21://Reg16
		Timer_Reload[1] = MFP[reg] = data;
		break;
	case 0x23://Reg17
		Timer_Reload[2] = MFP[reg] = data;
		break;
	case 0x25://Reg18
		Timer_Reload[3] = MFP[reg] = data;
		break;
	case 0x27://Reg19
	case 0x29://Reg20
	case 0x2b://Reg21
		MFP[reg] = data;
		break;
	case 0x2d://Reg22
		MFP[reg] = data|0x80; // Txは常にEnableに
		break;
	case 0x2f://Reg23
		if(data & 0x80){
		  if(keyLED != data){
		    draw_soft_kbd(0,0,data);
		  }
		  keyLED = data;
		}
		else if((data & 0x60) == 0x60){
		  keyREP_DELAY = data;
		}
		else if((data & 0x70) == 0x70){
		  keyREP_TIME = data;
		}
		break;
	default:
		break;
	}

}

int16_t timertrace = 0;
//static int TimerACounted = 0;
// -----------------------------------------------------------------------
//   たいまの時間を進める（も少し奇麗に書き直そう……）
// -----------------------------------------------------------------------
void FASTCALL MFP_Timer(int32_t clock)
{
	if ( (!(MFP[MFP_TACR]&8))&&(MFP[MFP_TACR]&7) ) {
		int32_t t = Timer_Prescaler[MFP[MFP_TACR]&7];
		Timer_Tick[0] += clock;
		while ( Timer_Tick[0]>=t ) {
			Timer_Tick[0] -= t;
			MFP[MFP_TADR]--;
			if ( !MFP[MFP_TADR] ) {
				MFP[MFP_TADR] = Timer_Reload[0];
				MFP_Int(2);
			}
		}
	}

	if ( MFP[MFP_TBCR]&7 ) {
		int32_t t = Timer_Prescaler[MFP[MFP_TBCR]&7];
		Timer_Tick[1] += clock;
		while ( Timer_Tick[1]>=t ) {
			Timer_Tick[1] -= t;
			MFP[MFP_TBDR]--;
			if ( !MFP[MFP_TBDR] ) {
				MFP[MFP_TBDR] = Timer_Reload[1];
				MFP_Int(7);
			}
		}
	}

	if ( MFP[MFP_TCDCR]&0x70 ) {
		int32_t t = Timer_Prescaler[(MFP[MFP_TCDCR]&0x70)>>4];
		Timer_Tick[2] += clock;
		while ( Timer_Tick[2]>=t ) {
			Timer_Tick[2] -= t;
			MFP[MFP_TCDR]--;
			if ( !MFP[MFP_TCDR] ) {
				MFP[MFP_TCDR] = Timer_Reload[2];
				MFP_Int(10);
			}
		}
	}

	if ( MFP[MFP_TCDCR]&7 ) {
		int32_t t = Timer_Prescaler[MFP[MFP_TCDCR]&7];
		Timer_Tick[3] += clock;
		while ( Timer_Tick[3]>=t ) {
			Timer_Tick[3] -= t;
			MFP[MFP_TDDR]--;
			if ( !MFP[MFP_TDDR] ) {
				MFP[MFP_TDDR] = Timer_Reload[3];
				MFP_Int(11);
			}
		}
	}
}


void FASTCALL MFP_TimerA(void)
{
	if ( (MFP[MFP_TACR]&15)==8 ) {					// いべんとかうんともーど（VDispでカウント）
		if ( MFP[MFP_AER]&0x10 ) {				// VDisp割り込みとタイミングが違ってるのが気になるといえば気になる（ぉぃ
			if (vline==CRTC_VSTART) MFP[MFP_TADR]--;	// 本来は同じだと思うんだけどなぁ……それじゃ動かんし（汗
		} else {
			if ( CRTC_VEND>=VLINE_TOTAL ) {
				if ( vline==(VLINE_TOTAL-1) ) MFP[MFP_TADR]--;	// 表示期間の終わりでカウントらしひ…（ろーどす）
			} else {
				if ( vline==CRTC_VEND ) MFP[MFP_TADR]--;
			}
		}
		if ( !MFP[MFP_TADR] ) {
			MFP[MFP_TADR] = Timer_Reload[0];
			MFP_Int(2);
		}
	}
}
