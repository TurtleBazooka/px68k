// ---------------------------------------------------------------------------------------
//  RTC.C - RTC (Real Time Clock / RICOH RP5C15)
// CLKOUT - TimerLED (not emulate)
// ALARM    PowerLED & MFP-I0
// ---------------------------------------------------------------------------------------

#include "common.h"
#include "mfp.h"

#include <time.h>

uint8_t	RTC_Regs[2][16];
uint8_t	RTC_Bank = 0;
static int32_t RTC_Timer1 = 0;
static int32_t RTC_Timer16 = 0;


// -----------------------------------------------------------------------
//   初期化(ホントはBuckUpされてる)
// -----------------------------------------------------------------------
void RTC_Init(void)
{
	memset(&RTC_Regs[1][0], 0, 16);//Alarm clear
	RTC_Regs[0][13] = 0x00;
	RTC_Regs[0][14] = 0x00;
	RTC_Regs[0][15] = 0x00;
	RTC_Regs[1][10] = 0x01;//24-hour mode(X68000 default)
}


// -----------------------------------------------------------------------
//   とけいのりーど
// -----------------------------------------------------------------------
uint8_t FASTCALL RTC_Read(uint32_t adr)
{
	uint8_t ret = 0xff;
	uint8_t tm24;

	struct tm *tm;
	time_t t;
	t = time(NULL);
	tm = localtime(&t);

	if(RTC_Regs[1][10] & 0x01)//24-hour system
	{
		tm24 = (tm->tm_hour);
	}
	else//12-hour system
	{
		tm24 = (tm->tm_hour) % 12;
		if ((tm->tm_hour) >=12) tm24 += 20;//PM set
	}

	RTC_Bank = RTC_Regs[0][13] & 0x01;

	/*0xe8a000 ~ 0xe8bfff*/
	if (RTC_Bank == 0)
	{
		switch(adr & 0x1f)
		{
		case 0x01: ret&=0xf0; ret|=((tm->tm_sec)%10 & 0x0f); break;
		case 0x03: ret&=0xf0; ret|=((tm->tm_sec)/10 & 0x0f); break;
		case 0x05: ret&=0xf0; ret|=((tm->tm_min)%10 & 0x0f); break;
		case 0x07: ret&=0xf0; ret|=((tm->tm_min)/10 & 0x0f); break;
		case 0x09: ret&=0xf0; ret|=((tm24)%10 & 0x0f); break;
		case 0x0b: ret&=0xf0; ret|=((tm24)/10 & 0x0f); break;
		case 0x0d: ret&=0xf0; ret|=((tm->tm_wday) & 0x0f); break;
		case 0x0f: ret&=0xf0; ret|=((tm->tm_mday)%10 & 0x0f); break;
		case 0x11: ret&=0xf0; ret|=((tm->tm_mday)/10 & 0x0f); break;
		case 0x13: ret&=0xf0; ret|=((tm->tm_mon+1)%10 & 0x0f); break;
		case 0x15: ret&=0xf0; ret|=((tm->tm_mon+1)/10 & 0x0f); break;
		case 0x17: ret&=0xf0; ret|=(((tm->tm_year)-80)%10 & 0x0f); break;
		case 0x19: ret&=0xf0; ret|=(((tm->tm_year)-80)/10 & 0x0f); break;
		case 0x1b: ret&=0xf0; ret|=(RTC_Regs[0][13] & 0x0d); break;
		case 0x1d: ret&=0xf0; ret|=(RTC_Regs[0][14] & 0x0f); break;
		case 0x1f: ret&=0xf0; ret|=(RTC_Regs[0][15] & 0x0f); break;
		default: break;
		}
	}
	else
	{
		switch(adr & 0x1f)
		{
		case 0x01: ret&=0xf0; ret|=(RTC_Regs[1][0] & 0x07); break;
		case 0x03: ret&=0xf0; ret|=(RTC_Regs[1][1] & 0x01); break;
		case 0x05: ret&=0xf0; ret|=(RTC_Regs[1][2] & 0x0f); break;
		case 0x07: ret&=0xf0; ret|=(RTC_Regs[1][3] & 0x07); break;
		case 0x09: ret&=0xf0; ret|=(RTC_Regs[1][4] & 0x0f); break;
		case 0x0b: ret&=0xf0; ret|=(RTC_Regs[1][5] & 0x03); break;
		case 0x0d: ret&=0xf0; ret|=(RTC_Regs[1][6] & 0x07); break;
		case 0x0f: ret&=0xf0; ret|=(RTC_Regs[1][7] & 0x0f); break;
		case 0x11: ret&=0xf0; ret|=(RTC_Regs[1][8] & 0x03); break;
		case 0x13: ret&=0xf0; break;
		case 0x15: ret&=0xf0; ret|=(RTC_Regs[1][10] & 0x01); break;
		case 0x17: ret&=0xf0; ret|= ((((tm->tm_year)-80)%4) & 0x03); break;
		case 0x19: ret&=0xf0; break;
		case 0x1b: ret&=0xf0; ret|=(RTC_Regs[1][13] & 0x0d); break;
		case 0x1d: ret&=0xf0; ret|=(RTC_Regs[0][14] & 0x0f); break;
		case 0x1f: ret&=0xf0; ret|=(RTC_Regs[0][15] & 0x0f); break;
		default: break;
		}
	}
	return ret;
}


// -----------------------------------------------------------------------
//   らいと (日付や時間のSetは行わない。保存するだけで使用しない)
// -----------------------------------------------------------------------
void FASTCALL RTC_Write(uint32_t adr, uint8_t data)
{
	RTC_Bank = RTC_Regs[0][13] & 0x01;

	/*0xe8a000 ~ 0xe8bfff*/
	if (RTC_Bank == 0)
	{
		switch(adr & 0x1f)
		{
		case 0x01: RTC_Regs[0][0] = (data & 0x0f); break;// 1-second
		case 0x03: RTC_Regs[0][1] = (data & 0x07); break;// 10-second
		case 0x05: RTC_Regs[0][2] = (data & 0x0f); break;// 1-min
		case 0x07: RTC_Regs[0][3] = (data & 0x07); break;// 10-min
		case 0x09: RTC_Regs[0][4] = (data & 0x0f); break;// 1-hor
		case 0x0b: RTC_Regs[0][5] = (data & 0x03); break;// 10-hor
		case 0x0d: RTC_Regs[0][6] = (data & 0x07); break;// day of the week
		case 0x0f: RTC_Regs[0][7] = (data & 0x0f); break;// 1-day counter
		case 0x11: RTC_Regs[0][8] = (data & 0x03); break;// 10-day counter
		case 0x13: RTC_Regs[0][9] = (data & 0x0f); break;// 1-month counter
		case 0x15: RTC_Regs[0][10] = (data & 0x01); break;// 10-month counter
		case 0x17: RTC_Regs[0][11] = (data & 0x0f); break;// 1-year counter
		case 0x19: RTC_Regs[0][12] = (data & 0x0f); break;// 10-year counter
		case 0x1b: RTC_Regs[0][13] = RTC_Regs[1][13] = data&0x0d; break;	// Alarm/Timer Enable制御 BANK
		case 0x1d: RTC_Regs[0][14] = RTC_Regs[1][14] = data&0x0f; break;	// TEST Mode
		case 0x1f: RTC_Regs[0][15] = RTC_Regs[1][15] = data&0x0c; break;	// Alarm端子出力制御
		default: break;
		}
	}
	else
	{
		switch(adr & 0x1f)
		{
		case 0x01: RTC_Regs[1][0] = (data & 0x07); break;// Clock OutPut select
		case 0x03: RTC_Regs[1][1] = (data & 0x01); break;// ADJ
		case 0x05: RTC_Regs[1][2] = (data & 0x0f); break;// Alarm min x1
		case 0x07: RTC_Regs[1][3] = (data & 0x07); break;// Alarm min x10
		case 0x09: RTC_Regs[1][4] = (data & 0x0f); break;// Alarm hor x1
		case 0x0b: RTC_Regs[1][5] = (data & 0x03); break;// Alarm hor x10
		case 0x0d: RTC_Regs[1][6] = (data & 0x07); break;// Alarm day of week
		case 0x0f: RTC_Regs[1][7] = (data & 0x0f); break;// 1 day alarm
		case 0x11: RTC_Regs[1][8] = (data & 0x03); break;// 10 day alarm
		case 0x13: RTC_Regs[1][9] = 0x00; break;
		case 0x15: RTC_Regs[1][10] = (data & 0x01); break;// 12or24 select
		case 0x17: RTC_Regs[1][11] = (data & 0x03); break;// Leap year counter
		case 0x19: RTC_Regs[1][12] = 0x00; break;
		case 0x1b: RTC_Regs[0][13] = RTC_Regs[1][13] = (data & 0x0d); break; // Alarm/Timer Enable制御 BANK
		case 0x1d: RTC_Regs[0][14] = RTC_Regs[1][14] = (data & 0x0f); break; // TEST Mode
		case 0x1f: RTC_Regs[0][15] = RTC_Regs[1][15] = (data & 0x0c); break; // Alarm端子出力制御
		default: break;
		}
	}

	if (RTC_Regs[0][15] & 0x01)// Resets all alarm reg.
	{
	 memset(&RTC_Regs[1][1], 0, 8);//Alarm clear
	}

	if (RTC_Regs[0][15] & 0x02)// Timer Reset 
	{
	 RTC_Timer1 = RTC_Timer16 = 0;
	}
}

// Ret:/ALARM out  (1Hz,16Hz,ALARM)
int RTC_Timer(int32_t clock)
{
	int alarm_out = 0;//Alarm端子の出力 1Hz & 16Hz & Alarm

	RTC_Timer1  += clock;
	RTC_Timer16 += clock;

	// == 1Hz 割り込みと出力 ==
	if ( !(RTC_Regs[0][15] & 0x08) )// 出力enable
	{
		if ( RTC_Timer1>=10000000 )
		{
		  MFP_Int(15);
		  RTC_Timer1 -= 10000000;
		}
		if( RTC_Timer1<(10000000/2) )   alarm_out = 1;//Alarm端子 1Hz H/L
	}
	if ( RTC_Timer1>=10000000 ) RTC_Timer1 -= 10000000;

	// == 16Hz 割り込みと出力 ==
	if ( !(RTC_Regs[0][15] & 0x04) )// 出力enable
	{
		if ( RTC_Timer16>=625000 )
		{
		  MFP_Int(15);
		  RTC_Timer16 -= 625000;
		}
		if( RTC_Timer16<(625000/2) )    alarm_out = 1;//Alarm端子 16Hz H/L
	}
	if ( RTC_Timer16>=625000 ) RTC_Timer16 -= 625000;

	// == ALARM出力 ==
	if ( !(RTC_Regs[0][13] & 0x04) )// AlarmEN
	{
		// none
	}

return alarm_out;
}
