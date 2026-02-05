/*
=== SDL2で漢字フォントをTrueTypeから生成する必殺モード ===

X68000 FONT-ROM Address info.(768KB)

$00000 16x16 JIS非漢字752文字 32x752=$5e00
$05e00 16x16 JIS第1/2第1水準漢字3,008文字、第2水準漢字3,478文字 =$32AC0

$3a000 8x8[1Byte] 8x256=$800
$3a800 8x16[1Byte] 16x256=$1000
$3b800 12(16)x12 16x12x256/8=$1800
$3d000 12(16)x24[1Byte] 2x24x256=$3000

$40000 24x24 JIS非漢字 3x24x752=$d380
$4d380 24x24 JIS第1/2 3x24x6486=$72030

$bf400 6(8)x12 [1Byte] 12x256=$0C00 ※X68030のみ

$c0000 END

by kameya 2022/11/02
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef SDL2
#include <SDL2/SDL.h>
#include <SDL2_ttf/SDL_ttf.h>
#else
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#endif

#include "common.h"
#include "dosio.h"
#include "mkcgrom.h"

#define FONT_PATH16 "./KH-Dot-Kagurazaka-16.ttf"//16x16 第一候補
#define FONT_PATH24 "./KH-Dot-Hibiya-24.ttf"//24x24 第２候補
#define FONT_PATH12  "./msmincho.ttc"// 12x12第３候補

#define FONT_PATHB  "./msmincho.ttc"// おススめ

#ifdef __MACH__
#define FONT_PATH   "/System/Library/Fonts/ヒラギノ角ゴシック W0.ttc"//default
#endif

#ifdef __linux__
#define FONT_PATH   "/usr/share/fonts/truetype/fonts-japanese-mincho.ttf"//default
#endif

#ifdef _WIN32
#define FONT_PATH   "c://Windows/Fonts/msmincho.ttc"//default
#endif

  // SDL2 TrueType drawing
  SDL_Surface *surface;
  TTF_Font *font;

  char utf[100];

extern uint32_t x68_hikanji[][4];
extern uint32_t x68_kanji1[][4];
extern uint32_t x68_kanji2[][4];

/*-------- bit set -----------*/
void
Font_dotset(uint8_t *draw_add,uint32_t size_x,uint32_t size_y,uint32_t ch,uint32_t dx,uint32_t dy)
{
 if (dx>=size_x) return;
 if (dy>=size_y) return;

  uint8_t *d_adrs;

 d_adrs = draw_add + ((size_x * size_y)/8*ch) + size_x/8*dy + dx/8 ;
 uint8_t dt1 = *d_adrs;
 uint8_t dt2 = 0x80 >> (dx % 8);

 *d_adrs = (dt1 | dt2);

 return;
}

/*-------- utf set -----------*/
void
set_utf(uint32_t c) /*漢字コード用*/
{

	if((c & 0xffff00) == 0 ){utf[0]=c; utf[1]=0x00; utf[2]=0x00;utf[3]=0x00; return;}//1byte
	if((c & 0xff0000) == 0 ){utf[0]=c>>8; utf[1]=c; utf[2]=0x00;utf[3]=0x00; return;}//2byte
	utf[0]=c>>16; utf[1]=c>>8; utf[2]=c;utf[3]=0x00;//3byte
	return;
}

void
set_utf1(uint32_t ch,uint32_t ch1)/*1byteコードhalf用 */
{
	uint32_t chb=ch*16+ch1;

	if(chb>0x7d){utf[0]=0x20;utf[1]=0x00;return;}
	//chb-=0x20;
	utf[0]=x68_ank_h[chb];
	utf[1]=0x00;//1byte

	return;
}

/*---- 1byte領域(full/half width)描画 ----*/
int32_t
getfont(uint8_t *addr, uint32_t size_x, uint32_t size_y,int32_t fullw) {
  uint32_t ch,ch1,x,y;
  uint32_t *fntbuf;
  char prt[100];

  for(ch=0; ch<16; ch++){
   for(ch1=0; ch1<16; ch1++){
	if(fullw == 0){set_utf1(ch,ch1);}//half width
	else{set_utf(x68_ank[ch*16+ch1]);}//full width
#ifndef SDL2
	surface = TTF_RenderText_Blended(font, utf, 0, (SDL_Color){255,255,255});
#else
	surface = TTF_RenderUTF8_Blended(font, utf, (SDL_Color){255,255,255,255});
#endif
	fntbuf=surface->pixels;
	for(y=0; y<surface->h; y++){
	for(x=0; x<surface->w; x++){
	   if(*(fntbuf+(surface->pitch/4)*y+x)==0xffffff){strcat(prt,"-");}
	   else{
	    strcat(prt,"●");
	    if((size_x == 8)&&(size_y == 12)){Font_dotset(addr,size_x,size_y,ch*16+ch1,x*6/surface->w,y*size_y/surface->h);}//6x12
	    if((size_x == 8)&&(size_y == 16)){//8x16
		   if(surface->w<size_x){ Font_dotset(addr,size_x,size_y,ch*16+ch1,x+(size_x-surface->w)/2,y*size_y/surface->h); }
		   else{ Font_dotset(addr,size_x,size_y,ch*16+ch1,x*size_x/surface->w,y*size_y/surface->h); }
		}
		if((size_x ==16)&&(size_y == 16)){//16x16
		   if(surface->w<size_x){ Font_dotset(addr,size_x,size_y,ch*16+ch1,x+(size_x-surface->w)/2,y); }
		   else{ Font_dotset(addr,size_x,size_y,ch*16+ch1,x*8/surface->w,y); }
	    }
	    if((size_x == 16)&&(size_y==12)){Font_dotset(addr,size_x,size_y,ch*16+ch1,x,y);}//12x12
	    if(size_y==24){//24x24 or 12x24
	       if(surface->w<12){Font_dotset(addr,size_x,size_y,ch*16+ch1,x+(12-surface->w)/2,y);}
		   else{ Font_dotset(addr,size_x,size_y,ch*16+ch1,x*12/surface->w,y); }
	    }
	   }
	}
	 strcat(prt,"\n");
	 if(ch==100){ printf("%s",prt);}
	 prt[0]='\0';
	}
	 if(ch==100){printf("%dx%d  %d=%s\n",size_x,size_y,ch*16+ch1,utf);}
   }
  }

return TRUE;
}

/*---- JIS非漢字/第１水準/第２水準 描画 ----*/
int32_t
getfont_j(uint8_t *addr, uint32_t size_x, uint32_t size_y, uint32_t flg) {
  uint32_t ch,x,y;
  uint32_t *fntbuf;
  uint32_t *addr1;
  char prt[100];

  if(flg==0){addr1=(uint32_t *)x68_hikanji;}
  if(flg==1){addr1=(uint32_t *)x68_kanji1;}
  if(flg==2){addr1=(uint32_t *)x68_kanji2;}

  ch=0;
  while(*(addr1+(ch*4)+2) != 0){
	set_utf(*(addr1+(ch*4)+2));
#ifndef SDL2
	surface = TTF_RenderText_Blended(font, utf, 0, (SDL_Color){255,255,255});
#else
	surface = TTF_RenderUTF8_Blended(font, utf, (SDL_Color){255,255,255,255});
#endif
	fntbuf=surface->pixels;
	for(y=0; y<surface->h; y++){
	for(x=0; x<surface->w; x++){
	   if(*(fntbuf+(surface->pitch/4)*y+x)==0xffffff){ strcat(prt,"-");}
	   else{
	    strcat(prt,"●");
	    Font_dotset(addr,size_x,size_y,ch,x,y);
	   }
	   
	}
	 strcat(prt,"\n");
	 if(ch==100){ printf("%s",prt);}
	 prt[0]='\0';
	}
	 if(ch==100){printf("%dx%d  %d=%s\n",size_x,size_y,ch,utf);}
	 ch++;
  }

return TRUE;
}

/*---- 8x16 から 8x8 を生成する ----*/
static void cpy2fnt8(uint8_t *src, uint8_t *dst, uint32_t size_x, uint32_t size_y)
{
	uint32_t i,j;
	uint8_t *c;

	for(i=0; i<256; i++)
	{
		for(j=0; j<8; j++)
		{
		c=(src+(i*16));
		*(dst+(i*8)+j) = (*(c+(j*2)) | *(c+(j*2)+1));
		}
	}

}

/*---- 8x8 を元に各サイズを生成する ----*/
static void cpy2fnt16(uint8_t *src, uint8_t *dst, uint32_t size_x, uint32_t size_y)
{
	uint32_t i,j;
	uint8_t *c;

	for(i=0; i<256; i++)
	{
		for(j=0; j<8; j++)
		{
		c=(src+(i*8)+j);
		*(dst+(i*16)+j*2 )  = *c;
		*(dst+(i*16)+j*2+1) = *c;
		}
	}

}

static void cpy2fnt12(uint8_t *src, uint8_t *dst, uint32_t size_x, uint32_t size_y)
{
	uint32_t i,j;
	uint8_t *c;

	for(i=0; i<256; i++)
	{
		for(j=0; j<8; j++)
		{
		c=src+(i*8)+j;
		*(dst+(i*24)+((j+2)*2)) = *c;
		}
	}

}

static void cpy2fnt24(uint8_t *src, uint8_t *dst, uint32_t size_x, uint32_t size_y)
{
	uint32_t i,j;
	uint8_t *c;

	for(i=0; i<256; i++)
	{
		for(j=0; j<8; j++)
		{
		c=src+(i*8)+j;
		*(dst+(i*48)+j*6) = *c;
		*(dst+(i*48)+j*6+2) = *c;
		*(dst+(i*48)+j*6+4) = *c;
		}
	}

}

/*---- Loading TrueType ----*/
int32_t
set_font(char *TTfont, uint32_t size)
{
  int32_t flg = TRUE;

  font = TTF_OpenFont(TTfont, size);
  if (font){
   return TRUE;//指定font OK
  }
  else{
  switch(size){//デフォルト定義でやってみそ
   case 12:
    font = TTF_OpenFont(FONT_PATH12, size);//12x12はmsmincho推し
    flg = 2;
    break;
   case 16:
    font = TTF_OpenFont(FONT_PATH16, size);//16x16はKH推し
    flg = 2;
    break;
   case 24:
    font = TTF_OpenFont(FONT_PATH24, size);//24x24はKH推し
    flg = 2;
    break;
   default:
    break;
  }

  if (!font){
    font = TTF_OpenFont(FONT_PATHB, size);
    flg = 2;// 第３候補
  }

  if (!font){
    font = TTF_OpenFont(FONT_PATH, size);
    flg = 3;// System FONT
  }

  }

  if (!font){
	printf("TrueTypeフォント%s(%dポイント)を開けませんでした。\n",TTfont,size);
	return FALSE;
  }
  return flg;
}

/*==== 全フォント生成 ====*/
int32_t
make_cgromdat(uint8_t *buf, char *FONT1, char *FONT2, uint32_t x68030)
{

  uint32_t i;
  uint32_t size_x = 16;
  uint32_t size_y = 16;

	// Clear buffer
	memset(buf, 0, 0xc0000);

	//SDL2/3_TTF initialize(No Err check!)
	TTF_Init();

	// 8x16
	size_x = 8;
	size_y = 16;
	i = set_font(FONT1, 16);
	if (i == FALSE){ return FALSE; }
	getfont(buf + 0x3a800,size_x,size_y,1);//全フォントを16x16->8/16にスケーリングで生成
	memset(buf+0x3a800+(0x20*16), 0, (94*16));
	getfont(buf + 0x3a800,size_x,size_y,0); //部分的に8/16で置き換え
	TTF_CloseFont(font);
	memset(buf+0x3a800+(0x82*16)+7, 0, 2);//「｜」の真ん中に切れ目を入れる

	if(i == 3){// System Font
	 memcpy(buf + 0x3a800 + 32*16, x68_chr816, 16*32);// (internal 8x16 FONT ! 0~9)
	 memcpy(buf + 0x3a800 + 64*16, x68_chr816A, 16*32);// (internal 8x16 FONT @ A~Z)
	 memcpy(buf + 0x3a800 + 97*16, x68_chr816a, 16*34);// (internal 8x16 FONT a~z{|}- )
	}

	// 8x8 (8x16からスケーリング)
	//cpy2fnt8(buf + 0x3a800, buf + 0x3a000,8,8);  //(8x16からスケーリング)
	 memcpy(buf + 0x3a000, x68_chr8, 8*0xff); // (internal 8x8 FONT)

	// 8x16を8x8から生成
	//cpy2fnt16(buf + 0x3a000, buf + 0x3a800,8,16); //8x8->8x16にスケーリングで生成

	// 16(12)x12
	size_x = 16;
	size_y = 12;
	if (set_font(FONT1, size_y) == 0){ return FALSE; }
	getfont(buf + 0x3b800,size_x,size_y,1);
	TTF_CloseFont(font);
	memset(buf+0x3b800+(0x82*24)+10, 0, 4);
	//cpy2fnt12(buf + 0x3a000, buf + 0x3b800,12,12);//8x8->12x12にスケーリングで生成

	// 16(12)x24
	size_x = 16;
	size_y = 24;
	if (set_font(FONT2, 24) == 0){ return FALSE; }
	getfont(buf + 0x3d000,size_x,size_y,1);//全フォントを24x24->12/24にスケーリングで生成
	memset(buf+0x3d000+(0x20*48), 0, (94*48));
	getfont(buf + 0x3d000,size_x,size_y,0);//部分的に8/16で置き換え
	TTF_CloseFont(font);
	memset(buf+0x3d000+(0x82*48)+22, 0, 4);//「｜」の真ん中に切れ目を入れる
	//cpy2fnt24(buf + 0x3a000, buf + 0x3d000,12,24);//8x8->12x24にスケーリングで生成

	if(x68030){// only X68030 6x12 ANK Font
	// 8(6)x12
	size_x = 8;
	size_y = 12;
	if (set_font(FONT1, size_y) == 0){ return FALSE; }
	getfont(buf + 0xbf400,size_x,size_y,1);
	TTF_CloseFont(font);
	memset(buf+0xbf400+(0x82*12)+5, 0, 2);//「｜」の真ん中に切れ目を入れる
	}

  /*16x16 JIS非漢字752文字*/
  printf("16x16 JIS非漢字752文字\n");
  size_x = 16;
  size_y = 16;
  if (set_font(FONT1, size_y) == 0){ return FALSE; }
  getfont_j(buf + 0x00000,size_x,size_y,0);
  TTF_CloseFont(font);

  /*16x16 第1水準漢字3,008文字*/
  printf("16x16 第1水準漢字3,008文字\n");
  size_x = 16;
  size_y = 16;
  if (set_font(FONT1, size_y) == 0){ return FALSE; }
  getfont_j(buf + 0x05e00,size_x,size_y,1);
  TTF_CloseFont(font);

  /*16x16 第2水準漢字3,478文字*/
  printf("16x16 第2水準漢字3,478文字\n");
  size_x = 16;
  size_y = 16;
  if (set_font(FONT1, size_y) == 0){ return FALSE; }
  getfont_j(buf + 0x1D600,size_x,size_y,2);
  TTF_CloseFont(font);

  /*24x24 JIS非漢字752文字*/
  printf("24x24 JIS非漢字752文字\n");
  size_x = 24;
  size_y = 24;
  if (set_font(FONT2, size_y) == 0){ return FALSE; }
  getfont_j(buf + 0x40000,size_x,size_y,0);
  TTF_CloseFont(font);

  /*24x24 第1水準漢字3,008文字*/
  printf("24x24 第1水準漢字3,008文字\n");
  size_x = 24;
  size_y = 24;
  if (set_font(FONT2, size_y) == 0){ return FALSE; }
  getfont_j(buf + 0x4d380,size_x,size_y,1);
  TTF_CloseFont(font);

  /*24x24 第2水準漢字3,478文字*/
  printf("24x24 第2水準漢字3,478文字\n");
  size_x = 24;
  size_y = 24;
  if (set_font(FONT2, size_y) == 0){ return FALSE; }
  getfont_j(buf + 0x82180,size_x,size_y,2);
  TTF_CloseFont(font);

	return(TRUE);
}
