// ---------------------------------------------------------------------------------------
//  BG.C - BGとスプライト
//  ToDo：透明色の処理チェック（特に対Text間）
// ---------------------------------------------------------------------------------------

#include "common.h"
#include "windraw.h"
#include "palette.h"
#include "tvram.h"
#include "crtc.h"
#include "bg.h"

	uint8_t 	BG[0x8000];
	uint8_t 	Sprite_Regs[0x800];
	uint8_t 	BG_Regs[0x12];
	uint16_t	BG_CHREND = 0;
	uint16_t	BG_BG0TOP = 0;
	uint16_t	BG_BG0END = 0;
	uint16_t	BG_BG1TOP = 0;
	uint16_t	BG_BG1END = 0;
	uint8_t 	BG_CHRSIZE = 16;
	uint32_t	BG_AdrMask = 511;
	uint32_t	BG0ScrollX = 0, BG0ScrollY = 0;
	uint32_t	BG1ScrollX = 0, BG1ScrollY = 0;

	int32_t 	BG_HAdjust = 0;
	int32_t 	BG_VLINE = 0;

	uint8_t 	BG_Dirty0[64*64];
	uint8_t 	BG_Dirty1[64*64];
	uint8_t 	BGCHR8[8*8*256];
	uint8_t 	BGCHR16[16*16*256];

	uint32_t	BG_LineBuf32[1600];
	uint16_t	BG_PriBuf[1600];

	int32_t 	VLINEBG = 0;


// -----------------------------------------------------------------------
//   初期化
// -----------------------------------------------------------------------
void BG_Init(void)
{
	int_fast16_t i;

	memset(Sprite_Regs, 0, sizeof(Sprite_Regs));
	memset(BG, 0, sizeof(BG));
	memset(BGCHR8, 0, sizeof(BGCHR8));
	memset(BGCHR16, 0, sizeof(BGCHR16));
	memset(BG_LineBuf32, 0, sizeof(BG_LineBuf32));
	for (i=0; i<0x12; i++)
		BG_Write(0xeb0800+i, 0);
	BG_CHREND = 0x8000;
}


// -----------------------------------------------------------------------
//   I/O Read
// -----------------------------------------------------------------------
uint8_t BG_Read(uint32_t adr)
{
	// 0xeb0000 ~ 0xebffff
	switch(adr)
	{
	 case 0xeb0000 ... 0xeb03ff:
		adr &= 0x0003ff;
#ifndef __BIG_ENDIAN__
		adr ^= 1;
#endif
		return Sprite_Regs[adr];
		break;
	 case 0xeb0400 ... 0xeb07ff:
		break;
	 case 0xeb0800 ... 0xeb0811:
		return BG_Regs[adr&0x00001f];
		break;
	 case 0xeb8000 ... 0xebffff:
		return BG[adr&0x007fff];
		break;
	 default:
		break;
	}
	return 0xff;
}


// -----------------------------------------------------------------------
//   I/O Write
// -----------------------------------------------------------------------

#define UPDATE_TDL(t)			\
{								\
	int_fast16_t i;				\
	for (i = 0; i < 16; i++) {	\
		TextDirtyLine[(t)] = 1;	\
		(t) = ((t) + 1) & 0x3ff;\
	}							\
}

void BG_Write(uint32_t adr, uint8_t data)
{
	uint32_t bg16chr;
	int32_t s1, s2, v = 0;

	s1 = (((BG_Regs[0x11]  &4)?2:1)-((BG_Regs[0x11]  &16)?1:0));
	s2 = (((CRTC_Regs[0x29]&4)?2:1)-((CRTC_Regs[0x29]&16)?1:0));
	if ( !(BG_Regs[0x11]&16) ) v = ((BG_Regs[0x0f]>>s1)-(CRTC_Regs[0x0d]>>s2));

	// 0xeb0000 ~ 0xebffff
	switch(adr)
	{
	 case 0xeb0000 ... 0xeb03ff:
		adr &= 0x0003ff;
		if((adr & 0x0007) == 0x0000) data &= 0x03; 
		if((adr & 0x0007) == 0x0002) data &= 0x03;
		if((adr & 0x0007) == 0x0004) data &= 0xcf;
		if((adr & 0x0007) == 0x0006) data &= 0x00;
		if((adr & 0x0007) == 0x0007) data &= 0x03;
#ifndef __BIG_ENDIAN__
		adr ^= 1;
#endif
		if (Sprite_Regs[adr] != data)
		{

			uint16_t t0, t, *pw;

			v = BG_VLINE - 16 - v;
			// get YPOS pointer (Sprite_Regs[] is little endian)
			pw = (uint16_t *)(Sprite_Regs + (adr & 0x3f8) + 2);

			t = t0 = (*pw + v) & 0x3ff;
			UPDATE_TDL(t);

			Sprite_Regs[adr] = data;

			t = (*pw + v) & 0x3ff;
			if (t != t0) {
				UPDATE_TDL(t);
			}

		}
		break;
	 case 0xeb0400 ... 0xeb07ff:
		break;
	 case 0xeb0800 ... 0xeb0811:
		adr &= 0x00001f;
		if (BG_Regs[adr]==data) return;	// データに変化が無ければ帰る
		BG_Regs[adr] = data;
		switch(adr)
		{
		case 0x00:
			BG_Regs[0x00] &= 0x03;
		case 0x01:
			BG0ScrollX = (((uint32_t)BG_Regs[0x00]<<8)+BG_Regs[0x01])&BG_AdrMask;
			TVRAM_SetAllDirty();
			break;
		case 0x02:
			BG_Regs[0x02] &= 0x03;
		case 0x03:
			BG0ScrollY = (((uint32_t)BG_Regs[0x02]<<8)+BG_Regs[0x03])&BG_AdrMask;
			TVRAM_SetAllDirty();
			break;
		case 0x04:
			BG_Regs[0x04] &= 0x03;
		case 0x05:
			BG1ScrollX = (((uint32_t)BG_Regs[0x04]<<8)+BG_Regs[0x05])&BG_AdrMask;
			TVRAM_SetAllDirty();
			break;
		case 0x06:
			BG_Regs[0x06] &= 0x03;
		case 0x07:
			BG1ScrollY = (((uint32_t)BG_Regs[0x06]<<8)+BG_Regs[0x07])&BG_AdrMask;
			TVRAM_SetAllDirty();
			break;

		case 0x08:		// BG On/Off Changed
			TVRAM_SetAllDirty();
			break;

		case 0x0d:
			BG_HAdjust = (int32_t)(BG_Regs[0x0d]-(CRTC_HSTART+4))*8;	// 水平方向は解像度による1/2はいらない？（Tetris）
			TVRAM_SetAllDirty();
			break;
		case 0x0f:
			BG_VLINE = (int32_t)(BG_Regs[0x0f]-CRTC_VSTART)/((BG_Regs[0x11]&4)?1:2);	// BGとその他がずれてる時の差分
			TVRAM_SetAllDirty();
			break;

		case 0x11:		// BG ScreenRes Changed
			if (data&3)
			{
				if ((BG_BG0TOP==0x4000)||(BG_BG1TOP==0x4000))
					BG_CHREND = 0x4000;
				else if ((BG_BG0TOP==0x6000)||(BG_BG1TOP==0x6000))
					BG_CHREND = 0x6000;
				else
					BG_CHREND = 0x8000;
			}
			else
				BG_CHREND = 0x2000;
			BG_CHRSIZE = ((data&3)?16:8);
			BG_AdrMask = ((data&3)?1023:511);
			BG_HAdjust = (int32_t)(BG_Regs[0x0d]-(CRTC_HSTART+4))*8;		// 水平方向は解像度による1/2はいらない？（Tetris）
			BG_VLINE   = (int32_t)(BG_Regs[0x0f]-CRTC_VSTART)/((BG_Regs[0x11]&4)?1:2);	// BGとその他がずれてる時の差分
			break;
		case 0x09:		// BG Plane Cfg Changed
			TVRAM_SetAllDirty();
			if (data&0x08)
			{
				if (data&0x30)
				{
					BG_BG1TOP = 0x6000;
					BG_BG1END = 0x8000;
				}
				else
				{
					BG_BG1TOP = 0x4000;
					BG_BG1END = 0x6000;
				}
			}
			else
				BG_BG1TOP = BG_BG1END = 0;
			if (data&0x01)
			{
				if (data&0x06)
				{
					BG_BG0TOP = 0x6000;
					BG_BG0END = 0x8000;
				}
				else
				{
					BG_BG0TOP = 0x4000;
					BG_BG0END = 0x6000;
				}
			}
			else
				BG_BG0TOP = BG_BG0END = 0;
			if (BG_Regs[0x11]&3)
			{
				if ((BG_BG0TOP==0x4000)||(BG_BG1TOP==0x4000))
					BG_CHREND = 0x4000;
				else if ((BG_BG0TOP==0x6000)||(BG_BG1TOP==0x6000))
					BG_CHREND = 0x6000;
				else
					BG_CHREND = 0x8000;
			}
			break;
		case 0x0b:
			break;
		default:
			break;
		}
		Draw_DrawFlag = 1;
		break;
	 case 0xeb8000 ... 0xebffff:
		adr &= 0x007fff;
		if (BG[adr]==data) return;			// データに変化が無ければ帰る
		BG[adr] = data;
		if (adr<0x2000)//PCG Area (8x8,16x16)
		{
			BGCHR8[adr*2]   = data>>4;
			BGCHR8[adr*2+1] = data&15;
		}
		bg16chr = ((adr&3)*2)+((adr&0x3c)*4)+((adr&0x40)>>3)+((adr&0x7f80)*2);
		BGCHR16[bg16chr]   = data>>4;
		BGCHR16[bg16chr+1] = data&15;

		if (adr<BG_CHREND)				// パターンエリア
		{
			TVRAM_SetAllDirty();
		}
		if ((adr>=BG_BG1TOP)&&(adr<BG_BG1END))	// BG1 MAPエリア
		{
			TVRAM_SetAllDirty();
		}
		if ((adr>=BG_BG0TOP)&&(adr<BG_BG0END))	// BG0 MAPエリア
		{
			TVRAM_SetAllDirty();
		}
		break;
	 default:
		break;
	}

}

// -----------------------------------------------------------------------
//   1ライン分の描画
// -----------------------------------------------------------------------

struct SPRITECTRLTBL {
	uint16_t	sprite_posx;
	uint16_t	sprite_posy;
	uint16_t	sprite_ctrl;
	uint16_t	sprite_ply;
} __attribute__ ((packed));
typedef struct SPRITECTRLTBL SPRITECTRLTBL_T;

INLINE void
Sprite_DrawLineMcr(uint16_t pri)
{
	SPRITECTRLTBL_T *sct = (SPRITECTRLTBL_T *)Sprite_Regs;
	uint32_t y;
	uint32_t t;
	int32_t n;

	for (n = 127; n >= 0; --n) {
		if ((sct[n].sprite_ply & 0x0003) == pri) {
			SPRITECTRLTBL_T *sctp = &sct[n];

			t = (sctp->sprite_posx + BG_HAdjust) & 0x3ff;
			if (t >= TextDotX + 16)
				continue;

			y = sctp->sprite_posy & 0x3ff;
			y -= VLINEBG;
			y += BG_VLINE;
			y = -y;
			y += 16;

			// add y, 16; jnc .spline_lpcnt
			if (y <= 15) {
				uint8_t *p;
				uint8_t pal;
				int32_t i, d;
				uint8_t bh, dat;
				
				switch(sctp->sprite_ctrl & 0xc000)
				{
				case 0x4000://水平反転
					p = &BGCHR16[((sctp->sprite_ctrl << 8) & 0xff00) + (y * 16) + 15];
					d = -1;
					break;
				case 0x8000://垂直反転
					p = &BGCHR16[((sctp->sprite_ctrl << 8) & 0xff00)  + (((y * 16) & 0xff) ^ 0xf0)];
					d = 1;
					break;
				case 0xc000://水平・垂直反転
					p = &BGCHR16[((sctp->sprite_ctrl << 8) & 0xff00) + (((y * 16) & 0xff) ^ 0xf0) + 15];
					d = -1;
					break;
				default://反転なし
					p = &BGCHR16[((sctp->sprite_ctrl << 8) & 0xff00)  + (y * 16)];
					d = 1;
					break;
				}

				for (i = 0; i < 16; i++, t++, p += d) {
					pal = *p & 0x0f;//色下位4bit
					if (pal) {
						pal |= (sctp->sprite_ctrl >> 4) & 0xf0;//色上位4bit
						if (BG_PriBuf[t] >= n * 8) {
							BG_LineBuf32[t] = TextPal32[pal];
							Text_TrFlag[t] |= 2;
							BG_PriBuf[t] = n * 8;
						}
					}
				}
			}
		}
	}
}

#define BG_DRAWLINE_LOOPY(cnt) \
{ \
	bl = bl << 4;							\
	for (j = 0; j < cnt; j++, esi += d, edi++) {			\
		dat = *esi | bl;					\
		if (dat == 0)						\
			continue;					\
		if ((dat & 0xf) || !(Text_TrFlag[edi + 1] & 2)) {	\
			BG_LineBuf32[1 + edi] = TextPal32[dat];		\
			Text_TrFlag[edi + 1] |= 2;			\
		}							\
	}								\
}

#define BG_DRAWLINE_LOOPY_NG(cnt) \
{									\
	bl = bl << 4;					\
	for (j = 0; j < cnt; j++, esi += d, edi++) {	\
		dat = *esi & 0xf;			\
		if (dat) {					\
			dat |= bl;				\
			BG_LineBuf32[1 + edi] = TextPal32[dat];	\
			Text_TrFlag[edi + 1] |= 2;		\
		}						\
	}							\
}

void bg_drawline_loopx8(uint16_t BGTOP, uint32_t BGScrollX, uint32_t BGScrollY, int32_t adjust, int32_t ng)
{
       uint8_t dat, bl;
       int32_t i, j, d;
       uint32_t ebp, edx, edi, ecx;
       uint16_t si;
       uint8_t *esi;

       ebp = ((BGScrollY + VLINEBG - BG_VLINE) & 7) << 3;
       edx = BGTOP + (((BGScrollY + VLINEBG - BG_VLINE) & 0x1f8) << 4);
       edi = ((BGScrollX - adjust) & 7) ^ 15;
       ecx = ((BGScrollX - adjust) & 0x1f8) >> 2;

       for (i = TextDotX >> 3; i >= 0; i--) {
               bl = BG[ecx + edx];
               si = (uint16_t)BG[ecx + edx + 1] << 6;

               if (bl < 0x40) {
                       esi = &BGCHR8[si + ebp];
                       d = +1;
               } else if ((bl - 0x40) & 0x80) {
                       esi = &BGCHR8[si + 0x3f - ebp];
                       d = -1;
               } else if ((int8_t)bl >= 0x40) {// このキャストは大事！
                       esi = &BGCHR8[si + ebp + 7];
                       d = -1;
               } else {
                       esi = &BGCHR8[si + 0x38 - ebp];
                       d = +1;
               }
	       if (ng) {
		       BG_DRAWLINE_LOOPY_NG(8);
	       } else {
		       BG_DRAWLINE_LOOPY(8);
	       }
               ecx += 2;
               ecx &= 0x7f;
       }
}

void bg_drawline_loopx16(uint16_t BGTOP, uint32_t BGScrollX, uint32_t BGScrollY, int32_t adjust, int32_t ng)
{
       uint8_t dat, bl;
       int32_t i, j, d;
       uint32_t ebp, edx, edi, ecx;
       uint16_t si;
       uint8_t *esi;

       ebp = ((BGScrollY + VLINEBG - BG_VLINE) & 15) << 4;
       edx = BGTOP + (((BGScrollY + VLINEBG - BG_VLINE) & 0x3f0) << 3);
       edi = ((BGScrollX - adjust) & 15) ^ 15;
       ecx = ((BGScrollX - adjust) & 0x3f0) >> 3;

       for (i = TextDotX >> 4; i >= 0; i--) {
		bl = BG[ecx + edx];
		si = BG[ecx + edx + 1] << 8;

		if (bl < 0x40) {
			esi = &BGCHR16[si + ebp];
			d = +1;
		} else if ((bl - 0x40) & 0x80) {
			esi = &BGCHR16[si + 0xff - ebp];
			d = -1;
		} else if ((int8_t)bl >= 0x40) {// このキャストは大事！
			esi = &BGCHR16[si + ebp + 15];
			d = -1;
		} else {
			esi = &BGCHR16[si + 0xf0 - ebp];
			d = +1;
		}
		if (ng) {
			BG_DRAWLINE_LOOPY_NG(16);
		} else {
			BG_DRAWLINE_LOOPY(16);
		}
		ecx += 2;
		ecx &= 0x7f;
	}
}

INLINE void
BG_DrawLineMcr8(uint16_t BGTOP, uint32_t BGScrollX, uint32_t BGScrollY)
{
       bg_drawline_loopx8(BGTOP, BGScrollX, BGScrollY, BG_HAdjust, 0);
}

INLINE void
BG_DrawLineMcr16(uint16_t BGTOP, uint32_t BGScrollX, uint32_t BGScrollY)
{
	bg_drawline_loopx16(BGTOP, BGScrollX, BGScrollY, BG_HAdjust, 0);
}

INLINE void
BG_DrawLineMcr8_ng(uint16_t BGTOP, uint32_t BGScrollX, uint32_t BGScrollY)
{
       bg_drawline_loopx8(BGTOP, BGScrollX, BGScrollY, BG_HAdjust, 1);
}

INLINE void
BG_DrawLineMcr16_ng(uint16_t BGTOP, uint32_t BGScrollX, uint32_t BGScrollY)
{
       bg_drawline_loopx16(BGTOP, BGScrollX, BGScrollY, 0, 1);
}

LABEL void
BG_DrawLine(int32_t opaq, int32_t gd)
{
	int_fast32_t i;
	void (*func8)(uint16_t, uint32_t, uint32_t), (*func16)(uint16_t, uint32_t, uint32_t);

	if (opaq) {
		for (i = 16; i < TextDotX + 16; ++i) {
			BG_LineBuf32[i] = TextPal32[0];
			BG_PriBuf[i] = 0xffff;
		}
	} else {
		for (i = 16; i < TextDotX + 16; ++i) {
			BG_PriBuf[i] = 0xffff;
		}
	}

	func8  = (gd)? BG_DrawLineMcr8 : BG_DrawLineMcr8_ng;
	func16 = (gd)? BG_DrawLineMcr16 : BG_DrawLineMcr16_ng;

	Sprite_DrawLineMcr(1);
	if ((BG_Regs[9] & 8) && (BG_CHRSIZE == 8)) { // BG1 on
		(*func8)(BG_BG1TOP, BG1ScrollX, BG1ScrollY);
	}
	Sprite_DrawLineMcr(2);
	if (BG_Regs[9] & 1) { // BG0 on
		if (BG_CHRSIZE == 8) {
			(*func8)(BG_BG0TOP, BG0ScrollX, BG0ScrollY);
		} else {
			(*func16)(BG_BG0TOP, BG0ScrollX, BG0ScrollY);
		}
	}
	Sprite_DrawLineMcr(3);
}

