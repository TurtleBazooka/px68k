// ---------------------------------------------------------------------------------------
//  IOC.C - I/O Controller
// ---------------------------------------------------------------------------------------

#include "common.h"
#include "dosio.h"
#include "ioc.h"

	uint8_t	IOC_IntStat = 0;
	uint8_t	IOC_IntVect = 0;

	uint8_t prnt_strb1 = 0x01;;
	uint8_t X68_printdata,IOC_str;
	char	PRNTFILE[] = "PrintOut.txt";

// -----------------------------------------------------------------------
//   初期化〜
// -----------------------------------------------------------------------
void IOC_Init(void)
{
	IOC_IntStat = 0;
	IOC_IntVect = 0;
}

/* OutPut to Printer(file) */
void
X68_printout(uint8_t prn_data)
{
	FILEH fp;
	char prn_chr[10];

	prn_chr[0] = prn_data;
	fp = File_OpenCurDir((char *)PRNTFILE);
	if (!fp)
		fp = File_CreateCurDir((char *)PRNTFILE, FTYPE_TEXT);
	if (fp)
	{
		File_Seek(fp,0,FSEEK_END);
		File_Write(fp, prn_chr, 1);
		File_Close(fp);
	}
}

/* Print data */
void
PRN_Write(uint32_t adr, uint8_t val)
{
	/*== 0xe8c001 ~ e8c003==*/
	switch( adr )
	{
	case 0xe8c001:
		X68_printdata = val; /*data latch*/
		break;
	case 0xe8c003:
		if((val & 0x01)==0x01 && (prnt_strb1 & 0x01)==0x00){/*strobe L→H*/
		  IOC_IntStat |= 0x20;/*Busy*/
		  X68_printout(X68_printdata);
		}
		prnt_strb1 = val;
		break;
	default:
		break;
	}

}

// -----------------------------------------------------------------------
//   りーど
// -----------------------------------------------------------------------
uint8_t IOC_Read(uint32_t adr)
{
	uint8_t IOC_str;

	if (adr==0xe9c001){
	  if((IOC_IntStat & 0x20) == 0x00){/*printer busy*/
	    IOC_str = IOC_IntStat;
	    IOC_IntStat |= 0x20;
	    return IOC_str;
	  }
	  IOC_str = IOC_IntStat;
	  IOC_IntStat &= 0xdf;
	  return IOC_str;
	}

	return 0xff;
}


// -----------------------------------------------------------------------
//   らいと
// -----------------------------------------------------------------------
void IOC_Write(uint32_t adr, uint8_t data)
{
	/*== 0xe9c001 ~ e9c003==*/
	switch( adr )
	{
	case 0xe9c001://割り込みMASK
		IOC_IntStat &= 0xf0;
		IOC_IntStat |= data&0x0f;
		break;
	case 0xe9c003://割り込みVector
		IOC_IntVect = (data&0xfc);
		break;
	default:
		break;
	}

}
