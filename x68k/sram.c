// ---------------------------------------------------------------------------------------
//  SRAM.C - SRAM (16kb) Area
// ---------------------------------------------------------------------------------------

#include	"common.h"
#include	"fileio.h"
#include	"prop.h"
#include	"winx68k.h"
#include	"sysport.h"
#include	"x68kmemory.h"
#include	"sram.h"

	uint8_t	SRAM[0x4000];
	char	SRAMFILE[] = "sram.dat";


// -----------------------------------------------------------------------
//   役に立たないうぃるすチェック
// -----------------------------------------------------------------------
void SRAM_VirusCheck(void)
{

	if (!Config.SRAMWarning) return;				// Warning発生モードでなければ帰る

	if ( (cpu_readmem24_dword(0xed3f60)==0x60000002)
	   &&(cpu_readmem24_dword(0xed0010)==0x00ed3f60) )		// 特定うぃるすにしか効かないよxAｷ
	{
#if 0 /* XXX */
		int ret = MessageBox(hWndMain,
			"このSRAMデータはウィルスに感染している可能性があります。\n該当個所のクリーンアップを行いますか？",
			"けろぴーからの警告", MB_ICONWARNING | MB_YESNO);
		if (ret == IDYES)
		{
			for (int_fast16_t i=0x3c00; i<0x4000; i++)
				SRAM[i] = 0xFF;
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
	uint8_t tmp;
	FILEH fp;

	for (i=0; i<0x4000; i++)
		SRAM[i] = 0xFF;

	fp = File_OpenCurDir((char *)SRAMFILE);
	if (fp)
	{
		File_Read(fp, SRAM, 0x4000);
		File_Close(fp);
		/*for little endian guys!*/
#ifndef C68K_BIG_ENDIAN
		for (i=0; i<0x4000; i+=2)
		{
			tmp = SRAM[i];
			SRAM[i] = SRAM[i+1];
			SRAM[i+1] = tmp;
		}
#endif
	}
}


// -----------------------------------------------------------------------
//   撤収(Clear S-RAM)
// -----------------------------------------------------------------------
void SRAM_Cleanup(void)
{
	uint8_t tmp;
	FILEH fp;

	for (int_fast32_t i=0; i<0x4000; i+=2)
	{
		tmp = SRAM[i];
		SRAM[i] = SRAM[i+1];
		SRAM[i+1] = tmp;
	}

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
uint8_t FASTCALL SRAM_Read(uint32_t adr)
{
	adr &= 0xffff;
	adr ^= 1;
	if (adr<0x4000)
		return SRAM[adr];
	else
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
		adr &= 0xffff;
		adr ^= 1;
		SRAM[adr] = data;
	}
}
