// ---------------------------------------------------------------------------------------
//  MERCURY.C - ま〜きゅり〜ゆにっと
// ---------------------------------------------------------------------------------------

#include <math.h>

#include "common.h"
#include "dmac.h"
#include "m68000.h"
#include "irqh.h"
#include "mercury.h"

#ifdef	YMFM
#include "ymfm_wrap.h"
#else
#include "fmg_wrap.h"
#endif

#define MCRY_IRQ 4
#define Mcry_BufSize		48000*2

int32_t	Mcry_WrPtr = 0;
int32_t	Mcry_RdPtr = 0;
int32_t	Mcry_SampleRate = 44100;
int32_t	Mcry_ClockRate = 44100;
int32_t	Mcry_Count = 0;
uint8_t	Mcry_Status = 0;
uint8_t	Mcry_LRTiming = 0;
int16_t	Mcry_OutDataL = 0;
int16_t	Mcry_OutDataR = 0;
int16_t	Mcry_BufL[Mcry_BufSize];
int16_t	Mcry_BufR[Mcry_BufSize];
int32_t	Mcry_PreCounter = 0;

int16_t	Mcry_OldR, Mcry_OldL;
int32_t	Mcry_DMABytes = 0;
static double Mcry_VolumeShift = 65536;
static int32_t Mcry_SampleCnt = 0;
static uint8_t Mcry_Vector = 255;

extern int32_t BusErrFlag;
//extern	m68k_regs regs;


int32_t Mcry_IntCB(uint8_t irq)
{
	int32_t ret = -1;
	IRQH_IRQCallBack(irq);
	if ( irq==MCRY_IRQ ) {
		ret = (int32_t)Mcry_Vector;
	}
	return ret;
}

void Mcry_Int(void)
{
	IRQH_Int(MCRY_IRQ, &Mcry_IntCB);
}


static int32_t Mcry_Clocks[8] = {
	22050, 16000, 22050, 24000
};


int32_t Mcry_IsReady(void)
{
	return (Mcry_SampleCnt>0);
}


// -----------------------------------------------------------------------
//   MPU経過クロック時間分だけデータをバッファに溜める
// -----------------------------------------------------------------------
void Mcry_PreUpdate(int32_t clock)
{
	Mcry_PreCounter += (Mcry_ClockRate*clock);
	while(Mcry_PreCounter>=10000000L)
	{
		Mcry_SampleCnt++;
		Mcry_PreCounter -= 10000000L;
	}
	M288_Timer(clock);
}


// -----------------------------------------------------------------------
//   DSoundからの要求分だけバッファを埋める
// -----------------------------------------------------------------------
void Mcry_Update(int16_t *buffer, int32_t length)
{
	int32_t data;

	if (!length) return;

	M288_Update(buffer, length);
	while (length)
	{
		if ( Mcry_WrPtr==Mcry_RdPtr ) {
			Mcry_SampleCnt = 1;
			DMA_Exec(2); DMA_Exec(2);
		}

		if (Mcry_WrPtr!=Mcry_RdPtr)
		{
			Mcry_OldL = Mcry_BufL[Mcry_RdPtr];
			Mcry_OldR = Mcry_BufR[Mcry_RdPtr];
			Mcry_RdPtr++;
			if (Mcry_RdPtr>=Mcry_BufSize) Mcry_RdPtr=0;
		}

		data = *buffer;
		data += Mcry_OldL;
		if (data>32767) data = 32767;
		else if (data<(-32768)) data = -32768;
		*(buffer++) = (int16_t)data;

		data = *buffer;
		data += Mcry_OldR;
		if (data>32767) data = 32767;
		else if (data<(-32768)) data = -32768;
		*(buffer++) = (int16_t)data;

		length--;
	}
}


// -----------------------------------------------------------------------
//   1回分（1Word x 2ch）のデータをバッファに書き出し
// -----------------------------------------------------------------------
INLINE void Mcry_WriteOne(void)
{
	while (Mcry_Count<Mcry_SampleRate)
	{
		Mcry_BufL[Mcry_WrPtr] = (int16_t)(Mcry_OutDataL/Mcry_VolumeShift);
		Mcry_BufR[Mcry_WrPtr] = (int16_t)(Mcry_OutDataR/Mcry_VolumeShift);
		Mcry_Count += Mcry_ClockRate;
		Mcry_WrPtr++;
		if (Mcry_WrPtr>=Mcry_BufSize) Mcry_WrPtr=0;
	}
	Mcry_Count -= Mcry_SampleRate;
	Mcry_SampleCnt--;
}


// -----------------------------------------------------------------------
//   I/O Write
// -----------------------------------------------------------------------
void Mcry_Write(uint32_t adr, uint8_t data)
{
	/*== 0xecc000 ~ 0xecdfff==*/
	switch(adr & 0xff)
	{
	  case 0x00:
	  case 0x01:
	  case 0x80:
	  case 0x81:// Data Port
		if ( Mcry_SampleCnt<=0 ) return;
		if ( Mcry_Status&2 ) {		// Stereo
			if (Mcry_LRTiming)		// 右
			{
				if (!(Mcry_Status&8)) data=0;	// R Mute
				if (adr&1)			// Low Byte
				{
					Mcry_OutDataR = (Mcry_OutDataR&0xff00)|data;
					Mcry_LRTiming ^= 1;
					Mcry_WriteOne();
				}
				else				// High Byte
				{
					Mcry_OutDataR = (Mcry_OutDataR&0x00ff)|((uint16_t)data<<8);
				}
			}
			else				// 左
			{
				if (!(Mcry_Status&4)) data=0;	// L Mute
				if (adr&1)			// Low Byte
				{
					Mcry_OutDataL = (Mcry_OutDataL&0xff00)|data;
					Mcry_LRTiming ^= 1;
				}
				else				// High Byte
				{
					Mcry_OutDataL = (Mcry_OutDataL&0x00ff)|((uint16_t)data<<8);
				}
			}
		} else {			// Mono
			if (adr&1)			// Low Byte
			{
				Mcry_OutDataR = ((Mcry_Status&8)?((Mcry_OutDataR&0xff00)|data):0);
				Mcry_OutDataL = ((Mcry_Status&4)?((Mcry_OutDataL&0xff00)|data):0);
				Mcry_LRTiming ^= 1;
				Mcry_WriteOne();
			}
			else				// High Byte
			{
				Mcry_OutDataR = ((Mcry_Status&8)?((Mcry_OutDataR&0x00ff)|((uint16_t)data<<8)):0);
				Mcry_OutDataL = ((Mcry_Status&4)?((Mcry_OutDataL&0x00ff)|((uint16_t)data<<8)):0);
			}
		}
		break;
	  case 0x91:
	  case 0x11:
		if (Mcry_Status != data)
		{
			Mcry_Status = data;
			Mcry_SetClock();
		}
		break;
	  case 0xb1: // Int Vector
		Mcry_Vector = data;
		break;
	  case 0xc1:
	  case 0xc3:
	  case 0xc5:
	  case 0xc7: // 満開版まーきゅりー OPN
		M288_Write((uint8_t)((adr>>1)&3), data);
		break;
	  default:
		break;
	}

  return;
}


// -----------------------------------------------------------------------
//   I/O Read
// -----------------------------------------------------------------------
uint8_t Mcry_Read(uint32_t adr)
{
	uint8_t ret = 0;

	/*== 0xecc000 ~ 0xecdfff==*/
	switch(adr & 0xff)
	{
	  case 0x00:
	  case 0x01:
	  case 0x80:
	  case 0x81:// Data Port
		break;
	  case 0xa1:
	  case 0x21:// Status Port
		ret = ((Mcry_Status&0xf0)|0x0f);
		break;
	  case 0x91:
	  case 0x11:
		break;
	  case 0x90:
	  case 0x10:
		ret = (Mcry_LRTiming<<3);
		Mcry_LRTiming ^= 1;
		break;
	  case 0xb1:// Int Vector
		ret = Mcry_Vector;
		break;
	  case 0xc1:
	  case 0xc3:
	  case 0xc5:
	  case 0xc7: // 満開版まーきゅりー OPN
		ret = M288_Read((uint8_t)((adr>>1)&3));
		break;
	  default:
		ret = 0xff;// BusErrFlag = 1にはしない？
		break;
	}

	return ret;

}


// -----------------------------------------------------------------------
//   再生クロック設定
// -----------------------------------------------------------------------
void Mcry_SetClock(void)
{
	Mcry_ClockRate = Mcry_Clocks[(Mcry_Status>>4)&3];
	if (Mcry_Status&0x80) Mcry_ClockRate *= 2;
	Mcry_Count = 0;
	Mcry_PreCounter = 0;
//	Mcry_RdPtr = 0;
//	Mcry_WrPtr = 0;
}


// -----------------------------------------------------------------------
//   ぼりゅーむ設定
// -----------------------------------------------------------------------
void Mcry_SetVolume(uint8_t vol)
{
	if (vol>16) vol=16;
//	if (vol<0) vol=0;

	if (vol)
		Mcry_VolumeShift = pow(1.189207115, (16-vol));
	else
		Mcry_VolumeShift = 65536;		// Mute
	M288_SetVolume(vol);
}


// -----------------------------------------------------------------------
//   初期化〜
// -----------------------------------------------------------------------
void Mcry_Init(int32_t samplerate, const char* path)
{
	memset(Mcry_BufL, 0, Mcry_BufSize*2);
	memset(Mcry_BufR, 0, Mcry_BufSize*2);

	Mcry_WrPtr = 0;
	Mcry_RdPtr = 0;
	Mcry_OutDataL = 0;
	Mcry_OutDataR = 0;
	Mcry_Status = 0;
	Mcry_SampleRate = (int32_t)samplerate;
	Mcry_LRTiming = 0;
	Mcry_PreCounter = 0;

	Mcry_SetClock();

#ifdef _WIN32
char slash = '\\';
#else
char slash = '/';
#endif

	char	m288_path[MAX_PATH];

	snprintf(m288_path, sizeof(m288_path), "%s%c", path, slash);
	M288_Init(8000000, samplerate, m288_path);
}


void Mcry_Cleanup(void)
{
	M288_Cleanup();
}
