// ------------------------------------------------------------------------------
//  SRAM.C - SRAM (16kb) Area
//$ed0000	'Ｘ68000',$57 SRAMチェックデータ
//$ed0008-b RAM Size
//$ed000c	ROM 起動アドレスへのポインタ $00fc0000:InSCSI-IPL $00ea0020:ExSCSI-IPL
//$ed0010	$00ed_0100	SRAM 起動アドレス
//$ed0018	$0000=STD boot
//$ed001c	$00 KeyBoardの状態
//$ed005a	Set SASI Active Drive(0~15)
//~
//$ed006f	'V' ($00:SCSI無効 $56='V':SCSI有効)
//$ed0070	$0f SCSI bit0,1,2=Own-ID  bit3:1=CZ-6BS1
//$ed0071	$00 SASIドライブ接続フラグ
// ------------------------------------------------------------------------------

#include	"common.h"
#include	"dosio.h"
#include	"prop.h"
#include	"winx68k.h"
#include	"sysport.h"
#include	"x68kmemory.h"
#include	"sram.h"

	uint8_t	SRAM[0x4000];
	char	SRAMFILE[] = "sram.dat";

// -----------------------------------------------------------
//   SRAM all Clear
// -----------------------------------------------------------
void
SRAM_Clear()
{
	uint32_t i;

	Memory_WriteB(0xe8e00d, 0x31);	/* Allow SRAM Access(91byte)*/
	for(i=0xed0000; i<0xed4000; i++)
	{
	  Memory_WriteB(i, 0xff);
	}
	Memory_WriteB(0xe8e00d, 0x55);	/* Block SRAM Access(91byte)*/
	return;
}
// -----------------------------------------------------------
//   Set Memorry Size
// -----------------------------------------------------------
void
SRAM_SetRamSize(uint8_t size)
{
	Memory_WriteB(0xe8e00d, 0x31);	/* Allow SRAM Access(91byte)*/

	 Memory_WriteB(0xed0008, 0x00);
	 Memory_WriteB(0xed0009, (size&0xf0));
	 Memory_WriteB(0xed000a, 0x00);
	 Memory_WriteB(0xed000b, 0x00);

	Memory_WriteB(0xe8e00d, 0x55);	/* Block SRAM Access(91byte)*/
	return;
}
// -----------------------------------------------------------
//   SCSI-IPL 起動をSRAMに設定
// -----------------------------------------------------------
void
SRAM_SetSCSIMode(int32_t mode)/*1:Ex-SCSI 2:In-SCSI*/
{

	Memory_WriteB(0xe8e00d, 0x31);	/* Allow SRAM Access(91byte)*/
	switch(mode){/*SRAM SCSI set*/
		case 0:/*No-SCSI*/
			Memory_WriteB(0xed006f, 0x00);	/*No SCSI*/
			Memory_WriteB(0xed0070, 0x07);	/*Set ID=7*/
			Memory_WriteB(0xed0071, 0x00);	/*SASI flag all 0*/
			break;
		case 1:/*SCSI EX*/
			Memory_WriteB(0xed000c, 0x00);	/*ExSCSI-IPL*/
			Memory_WriteB(0xed000d, 0xea);
			Memory_WriteB(0xed000e, 0x00);
			Memory_WriteB(0xed000f, 0x20);
			Memory_WriteB(0xed006f, 'V');	/*Activate SCSI*/
			Memory_WriteB(0xed0070, 0x0f);	/*ExternalSCSI Set ID=7*/
			Memory_WriteB(0xed0071, 0x00);	/*SASI flag all 0*/
			break;
		case 2:/*SCSI IN*/
			Memory_WriteB(0xed000c, 0x00);	/*InSCSI-IPL*/
			Memory_WriteB(0xed000d, 0xfc);
			Memory_WriteB(0xed000e, 0x00);
			Memory_WriteB(0xed000f, 0x00);
			Memory_WriteB(0xed006f, 'V');	/*Activate SCSI*/
			Memory_WriteB(0xed0070, 0x07);	/*InternalSCSI Set ID=7*/
			Memory_WriteB(0xed0071, 0x00);	/*SASI flag all 0*/
			break;
		default:
			break;
	}
	Memory_WriteB(0xe8e00d, 0x55);	/* Block SRAM Access(91byte)*/

	return;
}

// -----------------------------------------------------------
//   SASI 有効ドライブをSRAMに設定
// -----------------------------------------------------------
void 
SRAM_SetSASIDrive(uint8_t drive)/*0~15 Active SASI drive*/
{
	if(drive>15) return;

	Memory_WriteB(0xe8e00d, 0x31);	/* Allow SRAM Access(91byte)*/

		Memory_WriteB(0xed005a, drive); /*SASI Dive (set active)*/

	Memory_WriteB(0xe8e00d, 0x55);	/* Block SRAM Access(91byte)*/

	return;
}

// -----------------------------------------------------------------------
//   役に立たないうぃるすチェック
// -----------------------------------------------------------------------
void SRAM_VirusCheck(void)
{

	if (!Config.SRAMWarning) return;				// Warning発生モードでなければ帰る

	if ( (cpu_readmem24_dword(0xed3f60)==0x60000002)
	   &&(cpu_readmem24_dword(0xed0010)==0x00ed3f60) )		// 特定うぃるすにしか効かないよ
	{
#if 0 /* XXX */
		int ret = MessageBox(hWndMain,
			"このSRAMデータはウィルスに感染している可能性があります。\n該当個所のクリーンアップを行いますか？",
			"けろぴーからの警告", MB_ICONWARNING | MB_YESNO);
		if (ret == IDYES)
		{
			for (int_fast16_t i=0x3c00; i<0x4000; i++)
			SRAM[i]    = 0xFF;
			SRAM[0x11] = 0x00;
			SRAM[0x10] = 0xed;
			SRAM[0x13] = 0x01;
			SRAM[0x12] = 0x00;
			SRAM[0x19] = 0x00;
		}
#endif /* XXX */
		SRAM_Cleanup();
		SRAM_Init();			// Virusクリーンアップ後のデータを書き込んでおく
	}
}


// -----------------------------------------------------------------------
//   初期化(Init S-RAM)
// -----------------------------------------------------------------------
void SRAM_Init(void)
{
	int_fast32_t i;
	uint16_t tmp;
	FILEH fp;

	memset(SRAM, 0xff, 0x4000);

	fp = File_OpenCurDir((char *)SRAMFILE);
	if (fp)
	{
		File_Read(fp, SRAM, 0x4000);
		File_Close(fp);
#ifndef __BIG_ENDIAN__
	/*for little endian guys!*/
	for (i=0; i<0x4000; i+=2)
	{
		tmp = *(uint16_t *)&SRAM[i];
		*(uint16_t *)&SRAM[i] = ((tmp >> 8) & 0x00ff) | ((tmp << 8) & 0xff00);
	}
#endif
	}
}


// -----------------------------------------------------------------------
//   撤収(Clear S-RAM)
// -----------------------------------------------------------------------
void SRAM_Cleanup(void)
{
	uint16_t tmp;
	FILEH fp;

/*for little endian guys!*/
#ifndef __BIG_ENDIAN__
	for (int_fast32_t i=0; i<0x4000; i+=2)
	{
		tmp = *(uint16_t *)&SRAM[i];
		*(uint16_t *)&SRAM[i] = ((tmp >> 8) & 0x00ff) | ((tmp << 8) & 0xff00);
	}
#endif

	fp = File_OpenCurDir((char *)SRAMFILE);
	if (!fp)
		fp = File_CreateCurDir((char *)SRAMFILE, FTYPE_SRAM);
	if (fp)
	{
		File_Write(fp, SRAM, 0x4000);
		File_Close(fp);
	}
}


// -----------------------------------------------------------------------
//   りーど(Read S-RAM)
// -----------------------------------------------------------------------
uint8_t FASTCALL SRAM_Read(uint32_t addr)
{
  addr &= 0xffff;

  if (addr<0x4000){
    if(addr & 1){
     return(*(uint16_t *)&SRAM[(addr & 0x3ffe)] & 0xff);   /*奇数Byte*/
    }
    return((*(uint16_t *)&SRAM[(addr & 0x3ffe)] >> 8) & 0xff);/*偶数Byte*/
  }

  return 0xff;
}


// -----------------------------------------------------------------------
//   らいと(Write S-RAM)
// -----------------------------------------------------------------------
void FASTCALL SRAM_Write(uint32_t adr, uint8_t data)
{

	if ( (SysPort[5]==0x31)&&(adr<0xed4000) )
	{
		if ((adr==0xed0018)&&(data==0xb0))	// SRAM起動への切り替え（簡単なウィルス対策）
		{
			if (Config.SRAMWarning)		// Warning発生モード（デフォルト）
			{
#if 0 /* XXX */
				int ret = MessageBox(hWndMain,
					"SRAMブートに切り替えようとしています。\nウィルスの危険がない事を確認してください。\nSRAMブートに切り替え、継続しますか？",
					"けろぴーからの警告", MB_ICONWARNING | MB_YESNO);
				if (ret != IDYES)
				{
					data = 0;	// STDブートにする
				}
#endif /* XXX */
			}
		}
		adr &= 0x3fff;
#ifndef __BIG_ENDIAN__
		adr ^= 1;
#endif
		SRAM[adr] = data;
	}
}
