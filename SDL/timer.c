// -----------------------------------------------------------------------
//   55.6/61fpsキープ用たいまー
// -----------------------------------------------------------------------
#include <unistd.h>
#include "common.h"
#include "crtc.h"
#include "mfp.h"
#include "winx68k.h"

uint64_t	timercnt = 0;
uint64_t	tick = 0;

void Timer_Init(void)
{
	tick = Get_nsecCount();
}

void Timer_Reset(void)
{
	tick = Get_nsecCount();
}

uint16_t Timer_GetCount(void)
{
	uint64_t ticknow = Get_nsecCount();//1ns単位
	uint64_t dif;
	if(ticknow>=tick){dif = ticknow-tick; }
	else{dif = UINT64_MAX-tick+ticknow;}//補正

	uint64_t TIMEBASE;
	if(CRTC_Regs[0x29]&0x10){
	  TIMEBASE = VSYNC_HIGH * 567 / VLINE_TOTAL;//HSYNC:31KHz
	}
	else{
	  TIMEBASE = VSYNC_NORM * 283 / VLINE_TOTAL;//HSYNC:15.75KHz
	}
	if((TIMEBASE < (VSYNC_NORM - 5000)) || (TIMEBASE > (VSYNC_HIGH + 5000))){
	  TIMEBASE = ((CRTC_Regs[0x29]&0x10)?VSYNC_HIGH:VSYNC_NORM);
	}

	timercnt += dif/100;//100ns単位
	tick = ticknow;
	if ( timercnt>=TIMEBASE ) {
//		timercnt = 0;
		timercnt -= TIMEBASE;
		if ( timercnt>=(TIMEBASE*2) ) timercnt = 0;
		return 1;
	}
	else{
		if((TIMEBASE-timercnt)>5000){//over 500μs
		 usleep(50);//50μs sleep
		}
		return 0;
	}
}
