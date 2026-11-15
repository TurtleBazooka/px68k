// ---------------------------------------------------------------------------------------
//  SCC.C - Z8530 SCC（PortB:Mouse  PortA:RS-232C(NotUse)）
// ---------------------------------------------------------------------------------------

#include "common.h"
#include "scc.h"
#include "m68000.h"
#include "irqh.h"
#include "mouse.h"

uint8_t MouseX = 0;
uint8_t MouseY = 0;
uint8_t MouseSt = 0;

uint8_t SCC_RegsA[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t SCC_RegNumA = 0;
uint8_t SCC_RegSetA = 0;
uint8_t SCC_RegsB[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t SCC_RegNumB = 0;
uint8_t SCC_RegSetB = 0;
uint8_t SCC_Vector = 0;
uint8_t SCC_Dat[3] = {0, 0, 0};
uint8_t SCC_DatNum = 0;


// -----------------------------------------------------------------------
//   わりこみ
// -----------------------------------------------------------------------
int32_t SCC_Int(uint8_t irq)
{
	int32_t ret = -1;
	IRQH_IRQCallBack(irq);
	if ( (irq==5)&&(!(SCC_RegsB[9]&2)) )
	{
		if (SCC_RegsB[9]&1)
		{
			if (SCC_RegsB[9]&0x10)
				ret = ((int32_t)(SCC_Vector&0x8f)+0x20);
			else
				ret = ((int32_t)(SCC_Vector&0xf1)+4);
		}
		else
			ret = ((int32_t)SCC_Vector);
	}

	return ret;
}

// -----------------------------------------------------------------------
//   割り込みのチェック
// -----------------------------------------------------------------------
void SCC_IntCheck(void)
{
	if ( (SCC_DatNum) && ((SCC_RegsB[1]&0x18)==0x10) && (SCC_RegsB[9]&0x08) )
	{
		IRQH_Int(5, &SCC_Int);
	}
	else if ( (SCC_DatNum==3) && ((SCC_RegsB[1]&0x18)==0x08) && (SCC_RegsB[9]&0x08) )
	{
		IRQH_Int(5, &SCC_Int);
	}
}

// -----------------------------------------------------------------------
//   初期化
// -----------------------------------------------------------------------
void SCC_Init(void)
{
	MouseX = 0;
	MouseY = 0;
	MouseSt = 0;
	SCC_RegNumA = 0;
	SCC_RegSetA = 0;
	SCC_RegNumB = 0;
	SCC_RegSetB = 0;
	SCC_Vector = 0;
	SCC_DatNum = 0;
}

// -----------------------------------------------------------------------
//   I/O Write (Z8530)
// -----------------------------------------------------------------------
void SCC_Write(uint32_t adr, uint8_t data)
{
	/*0xe98000 ~ 0xe99fff*/
	switch(adr & 0x07)
	{
	case 0x01:// SCC Command B
		if (SCC_RegSetB)
		{
			if (SCC_RegNumB == 5)
			{
				if ( (!(SCC_RegsB[5]&2))&&(data&2)&&(SCC_RegsB[3]&1)&&(!SCC_DatNum) )	// データが無い時だけにしやう（闇の血族）
				{			// マウスデータ生成
					Mouse_SetData();
					SCC_DatNum = 3;
					SCC_Dat[2] = MouseSt;
					SCC_Dat[1] = MouseX;
					SCC_Dat[0] = MouseY;
				}
			}
			else if (SCC_RegNumB == 2) SCC_Vector = data;
			SCC_RegSetB = 0;
			SCC_RegsB[SCC_RegNumB] = data;
			SCC_RegNumB = 0;
		}
		else
		{
			if (!(data&0xf0))
			{
				data &= 15;
				SCC_RegSetB = 1;
				SCC_RegNumB = data;
			}
			else
			{
				SCC_RegSetB = 0;
				SCC_RegNumB = 0;
			}
		}
		break;
	case 0x03:// SCC data B
		break;
	case 0x05:// SCC Command A
		if (SCC_RegSetA)
		{
			SCC_RegSetA = 0;
			switch (SCC_RegNumA)
			{
			case 2:
				SCC_RegsB[2] = data;
				SCC_Vector = data;
				break;
			case 9:
				SCC_RegsB[9] = data;
				break;
			}
		}
		else
		{
			data &= 15;
			if (data)
			{
				SCC_RegSetA = 1;
				SCC_RegNumA = data;
			}
			else
			{
				SCC_RegSetA = 0;
				SCC_RegNumA = 0;
			}
		}
		break;
	case 0x07:// SCC data A
		break;
	default:
		break;
	}

}

// -----------------------------------------------------------------------
//   I/O Read (Z8530)
// -----------------------------------------------------------------------
uint8_t SCC_Read(uint32_t adr)
{
	uint8_t ret=0xff;

	/*0xe98000 ~ 0xe99fff*/
	switch(adr & 0x07)
	{
	case 0x01:// SCC Command B
		if (!SCC_RegNumB)
			ret = ((SCC_DatNum)?1:0);
		else
			ret = 0x00;

		SCC_RegNumB = 0;
		SCC_RegSetB = 0;
		break;
	case 0x03:// SCC data B
		if (SCC_DatNum)
		{
			SCC_DatNum--;
			ret = SCC_Dat[SCC_DatNum];
		}
		else
		{
			ret = 0x00;
		}
		break;
	case 0x05:// SCC Command A
		switch(SCC_RegNumA)
		{
		case 0:
			ret = 4;			// 送信バッファ空（Xna）
			break;
		case 3:
			ret = ((SCC_DatNum)?4:0);
			break;
		default:
			ret = 0x00;
			break;
		}
		SCC_RegNumA = 0;
		SCC_RegSetA = 0;
		break;
	case 0x07:// SCC data A
			ret = 0x00; //Not Use
		break;
	default:
		break;
	}

	return ret;
}
