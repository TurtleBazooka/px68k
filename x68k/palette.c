// ---------------------------------------------------------------------------------------
//  PALETTE.C - Text/BG/Graphic Palette
// ---------------------------------------------------------------------------------------

#include	"common.h"
#include	"windraw.h"
#include	"tvram.h"
#include	"bg.h"
#include	"crtc.h"
#include	"x68kmemory.h"
#include	"m68000.h"
#include	"palette.h"
#include	"sysport.h"

	uint8_t	Pal_Regs[1024];

	uint8_t	    Contrast_Value;		//コントラスト追従用

	// for 32bit depth
	uint32_t	TextPal32[256];
	uint32_t	GrphPal32[256];
	uint32_t	Pal32[65536];

	uint32_t	Ibit32, Abit32;				// 半透明処理とかで使うかも〜
	uint32_t	Pal32_FullMask, Pal32_HalfMask, Pal32_X68kMask;

// 32bit depth Pallete init 
// SDL2では24bitのRGBX8888:RRRRRIxx GGGGGIxx BBBBBIxx 0000000α にしてみやう。
void Pal32_SetColor(void)
{
	uint32_t r, g, b, i;
	uint32_t TempMask, bit;
	uint32_t R[6] = {0, 0, 0, 0, 0, 0};
	uint32_t G[6] = {0, 0, 0, 0, 0, 0};
	uint32_t B[6] = {0, 0, 0, 0, 0, 0};

	r = g = b = 6;				//RGB 6bitまで探す(18bitColor)
	TempMask = 0;				// 使われているビットをチェック（MASK用）
	for (bit=0x80000000; bit; bit>>=1)
	{					// 各色毎に左（上位）から5ビットずつ拾う
		if ( (WinDraw_Pal32R&bit)&&(r) )
		{
			R[--r] = bit;
			TempMask |= bit;
		}
		if ( (WinDraw_Pal32G&bit)&&(g) )
		{
			G[--g] = bit;
			TempMask |= bit;
		}
		if ( (WinDraw_Pal32B&bit)&&(b) )
		{
			B[--b] = bit;
			TempMask |= bit;
		}
	}

	Ibit32 = B[0] | R[0] | G[0]; //共通Ibit=6bit目 0x04040400
	Abit32 = 0x00000001;  // α透明bitの割付場所
	Pal32_X68kMask = TempMask; //RGB有効 6bitのMASK 0xfcfcfc00
	Pal32_FullMask = (WinDraw_Pal32R | WinDraw_Pal32G | WinDraw_Pal32B);//0xffffff00
	Pal32_HalfMask = ((WinDraw_Pal32R>>1 & WinDraw_Pal32R) | (WinDraw_Pal32G>>1 &
						WinDraw_Pal32G) | (WinDraw_Pal32B>>1 & WinDraw_Pal32B))<<1;//0xfefefe00

	//printf("Ibit32:%08x Abit32:%08x Pal32_X68kMask:%08x Pal32_FullMask:%08x Pal32_HalfMask:%08x\n",Ibit32,Abit32,Pal32_X68kMask,Pal32_FullMask,Pal32_HalfMask);

	Pal32_ChangeContrast(15);

}


// -----------------------------------------------------------------------
//   初期化
// -----------------------------------------------------------------------
void Pal_Init(void)
{
	memset(Pal_Regs, 0, 1024);

	memset(TextPal32,  0, 1024);
	memset(GrphPal32,  0, 1024);
	Pal32_SetColor();
}


// -----------------------------------------------------------------------
//   I/O Read
// -----------------------------------------------------------------------
uint16_t Pal_Read16(uint32_t adr)
{
	if (adr<0xe82400){
	  adr &= 0x0003fe;
	  return (uint16_t)((Pal_Regs[adr]<<8) | Pal_Regs[adr | 1]);
	}
	return 0xffff;
}

uint8_t Pal_Read(uint32_t adr)
{
	if (adr<0xe82400){
	  adr &= 0x0003ff;
	  return Pal_Regs[adr];
	}
	return 0xff;
}

// -----------------------------------------------------------------------
//   I/O Write
// -----------------------------------------------------------------------
void Pal_Write16(uint32_t adr, uint16_t data)
{

	if (adr>=0xe82400) return;

	adr &= 0x0003ff;
	if (((Pal_Regs[adr&0x3fe]<<8) | Pal_Regs[adr | 1]) == data) return;

	Pal_Regs[adr&0x3fe] = (uint8_t)(data>>8 & 0xff);
	Pal_Regs[adr | 1]   = (uint8_t)(data    & 0xff);
	TVRAM_SetAllDirty();

	if (adr<0x200)
	{
		GrphPal32[adr/2] = Pal32[data];
	}
	else if (adr<0x400)
	{
		TextPal32[(adr-0x200)/2] = Pal32[data];
	}
}

void Pal_Write(uint32_t adr, uint8_t data)
{
	uint16_t pal;

	if (adr>=0xe82400) return;

	adr &= 0x0003ff;
	if (Pal_Regs[adr] == data) return;

	if (adr<0x200)
	{
		Pal_Regs[adr] = data;
		TVRAM_SetAllDirty();
		pal = (Pal_Regs[adr&0xffe] << 8) | Pal_Regs[adr|1];
		GrphPal32[adr/2] = Pal32[pal];
	}
	else if (adr<0x400)
	{
		if (MemByteAccess) return;		// TextPalはバイトアクセスは出来ないらしい（神戸恋愛物語）
		Pal_Regs[adr] = data;
		TVRAM_SetAllDirty();
		pal = (Pal_Regs[adr&0xffe] << 8) | Pal_Regs[adr | 1];
		TextPal32[(adr-0x200)/2] = Pal32[pal];
	}
}

// -----------------------------------------------------------------------
//   こんとらすとはブラウン管みたいにゆっくり追従させる。
// -----------------------------------------------------------------------
void Pal_TrackContrast(void)
{
	if(SysPort[1] == Contrast_Value){ return; }

	if(SysPort[1] > Contrast_Value){Contrast_Value++;}
	if(SysPort[1] < Contrast_Value){Contrast_Value--;}

	Pal32_ChangeContrast(Contrast_Value);
}

// -----------------------------------------------------------------------
//   こんとらすと変更（パレットに対するSDL_Surface側の表示色で実現してます ^^;）
// X68000のRGBI(16bit:GGGGGRRRRRBBBBBI)をRGBX8888の24bitColorに輝度情報含めて変換っすよ
// -----------------------------------------------------------------------
void Pal32_ChangeContrast(int32_t num)
{
	uint32_t i;
	uint64_t palr, palg, palb;
	uint32_t pal;

	if(num > 15) num = 15;

	TVRAM_SetAllDirty();
	
	// Pal32を輝度に合わせて変更する
	for (i=0; i<65536; i++)
	{
		palg = (i&0xf800) >> 10;// Green 5bit
		palr = (i&0x07c0) >> 5; // Red  5bit
		palb = (i&0x003e);      // Blue 5bit
		if (i&0x0001){ palb |= 0x0001;palr |= 0x0001;palg |= 0001; }// IbitはRGB共通 6bit
		palg = (uint64_t)(((palg*4*num)/15) << 16) & WinDraw_Pal32G;
		palr = (uint64_t)(((palr*4*num)/15) << 24) & WinDraw_Pal32R;
		palb = (uint64_t)(((palb*4*num)/15) << 8 ) & WinDraw_Pal32B;
		Pal32[i] = (uint32_t)(palr | palb | palg);
		if(i & 0x0001) Pal32[i] |= Abit32; // 透明処理保存用(明るさを変化させてもココが残ってる)
	}

	// TextPal32とGrphPal32も強制変更する
	for (i=0; i<256; i++)
	{
		pal = Pal_Regs[i*2];
		pal = (pal<<8)+Pal_Regs[i*2+1];
		GrphPal32[i] = Pal32[pal];

		pal = Pal_Regs[i*2+512];
		pal = (pal<<8)+Pal_Regs[i*2+513];
		TextPal32[i] = Pal32[pal];
	}
}
