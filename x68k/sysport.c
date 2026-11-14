// ---------------------------------------------------------------------------------------
//  SYSPORT.C - X68k System Port
// ---------------------------------------------------------------------------------------

#include "common.h"
#include "prop.h"
#include "sysport.h"
#include "palette.h"

uint8_t	SysPort[7];

// -----------------------------------------------------------------------
//   初期化
// -----------------------------------------------------------------------
void SysPort_Init(void)
{

	memset(SysPort, 0, sizeof(SysPort));

}


// -----------------------------------------------------------------------
//   らいと
// -----------------------------------------------------------------------
void SysPort_Write(uint32_t adr, uint8_t data)
{
	/*0xe8e000 ~ 0xe8ffff*/
	switch(adr & 0x0f)
	{
	case 0x01:
		if (SysPort[1]!=(data & 0x0f))
		{
			SysPort[1] = data & 0x0f;
			//Pal_ChangeContrast(SysPort[1]);  即設定ではなく追従させる。
		}
		break;
	case 0x03:
		SysPort[2] = data & 0x0b;
		break;
	case 0x05:
		SysPort[3] = data & 0x1f;
		break;
	case 0x07:
		SysPort[4] = data & 0x0e;
		break;
	case 0x0d:
		SysPort[5] = data;
		break;
	case 0x0f:
		SysPort[6] = data & 0x0f;
		break;
	default:
		break;
	}
}


// -----------------------------------------------------------------------
//   りーど
// -----------------------------------------------------------------------
uint8_t SysPort_Read(uint32_t adr)
{
	uint8_t ret=0xff;// 初期値

	/*0xe8e000 ~ 0xe8ffff*/
	switch(adr & 0x0f)
	{
	case 0x01:
		ret &= 0xf0;
		ret |= SysPort[1];
		break;
	case 0x03:
		ret &= 0xf4;
		ret |= SysPort[2];
		break;
	case 0x05:
		ret &= 0xe0;
		ret |= SysPort[3];
		break;
	case 0x07:
		ret &= 0xf1;
		ret |= SysPort[4];
		break;
	case 0x0b:		// 10MHz:0xff、16MHz:0xfe、030(25MHz):0xdcをそれぞれ返すらしい
		switch(Config.XVIMode)
		{
		case 1:			// XVI or RedZone
		case 2:
			ret = 0xfe;
			break;
		case 3:			// 030
			ret = 0xdc;
			break;
		default:		// 10MHz
			ret = 0xff;
			break;
		}
		break;
	case 0x0d:
		ret = SysPort[5];
		break;
	case 0x0f:
		ret &= 0xf0;
		ret |= SysPort[6];
		break;
	default:
		break;
	}

	return ret;
}
