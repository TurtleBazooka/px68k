/* 
 * Copyright (c) 2003 NONAKA Kimihiro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <unistd.h>
#include "common.h"
#include "SDL3/SDL.h"

#ifdef USE_OGLES20
#include "SDL3/SDL3_opengles.h"
#endif

#include "winx68k.h"
#include "winui.h"

#include "prop.h"
#include "GamePad.h"
#include "keyboard.h"
#include "x68kmemory.h"
#include "mfp.h"

#if defined(USE_OGLES20)
extern uint16_t *menu_buffer;
extern uint16_t *kbd_buffer;

extern static GLuint texid[11];

extern GLint attr_pos, attr_uv, texture;
extern GLuint shader_prog, v_shader, f_shader;
extern SDL_DisplayMode sdl_dispmode;
#else
extern SDL_Surface *menu_surface;
#endif

/* Drawing SDL3 buffer */
extern SDL_Window *sdl_window;

/*UTF8 conv Table and util. by Kameya*/
#include "kanjiconv.c"
#include "menu_str_utf8.txt"

extern void Update_Screen(uint32_t);

/********** menu 関連ルーチン **********/

// 画面タイプを変更する
enum ScrType {x68k, pc98};
int32_t scr_type = x68k;

struct _px68k_menu {
	uint32_t *sbp;  // surface buffer ptr
	uint32_t *mlp; // menu locate ptr
	uint32_t mcolor; // color of chars to write
	uint32_t mbcolor; // back ground color of chars to write
	int32_t ml_x;
	int32_t ml_y;
	int32_t mfs; // menu font size;
} p6m;

//menuデザイン定義(行数)
#define p6m_colum_h 1//menu上段行数
#define p6m_colum_m 12//menu中段行数
#define p6m_colum_l 2//menu下段行数
#define p6m_colum_f 15//File選択menu行数

/* sjis→jisコード変換 */
static uint16_t sjis2jis(uint16_t w)
{
	uint8_t wh, wl;

	wh = w / 256;
	wl = w % 256;

	wh <<= 1;
	if (wl < 0x9f) {
		wh += (wh < 0x3f)? 0x1f : -0x61;
		wl -= (wl > 0x7e)? 0x20 : 0x1f;
	} else {
		wh += (wh < 0x3f)? 0x20 : -0x60;
		wl -= 0x7e;
	}

	return (wh * 256 + wl);
}

/* JISコードから0 originのindexに変換する */
/* ただし0x2921-0x2f7eはX68KのROM上にないので飛ばす */
static uint16_t jis2idx(uint16_t jc)
{
	if (jc >= 0x3000) {
		jc -= 0x3021;
	} else {
		jc -= 0x2121;
	}
	jc = jc % 256 + (jc / 256) * 0x5e;

	return jc;
}

#define isHankaku(s) ((s) >= 0x20 && (s) <= 0x80 || (s) >= 0xa0 && (s) <= 0xdf)

#if defined(ANDROID)
// display width 800, buffer width 1024 だけれど 800 にしないとだめ
#define MENU_WIDTH 800
#else
#define MENU_WIDTH 800
#endif

// IN:sjis；Shift-JIS Code   fs : font size (8 or 16/24)
// OUT:TRUE/FALSE  addr：FONT-ROM Address
// 半角文字の場合は16bitの上位8bitにデータを入れておくこと
// (半角or全角の判断ができるように)
static BOOL get_font_addr(uint16_t sjis, int32_t fs, uint32_t *addr)
{

	// 1Byte code
	uint8_t half_byte = (sjis >> 8);
	if (isHankaku(half_byte)) {
		switch (fs) {
		case 8:
			*addr = (0x3a000 + half_byte * (1 * 8));
			return TRUE;
		case 16:
			*addr = (0x3a800 + half_byte * (1 * 16));
			return TRUE;
		case 24:
			*addr = (0x3d000 + half_byte * (2 * 24));
			return TRUE;
		default:
			*addr = 0;
			return FALSE;
		}
	}

	// 2byte code(S-JIS)
	uint16_t jis = sjis2jis(sjis);
	uint16_t j_idx = (uint32_t)jis2idx(jis);
	uint8_t jhi = (uint8_t)(jis >> 8);
	int32_t fsb; // file size in bytes

	if (fs == 16) {
		fsb = 2 * 16;
	} else if (fs == 24) {
		fsb = 3 * 24;
	} else {
		*addr = 0;
		return FALSE;
	}

#if 0
	printf("sjis code = 0x%x\n", sjis);
	printf("jis code = 0x%x\n", jis);
	printf("jhi 0x%x j_idx 0x%x\n", jhi, j_idx);
#endif

	if (jhi >= 0x21 && jhi <= 0x28) {
		// 非漢字
		*addr = ((fs == 16)? 0x0 : 0x40000) + j_idx * fsb;
		return TRUE;
	} else if (jhi >= 0x30 && jhi <= 0x74) {
		// 第一水準/第二水準
		*addr = ((fs == 16)? 0x5e00 : 0x4d380) + j_idx * fsb;
		return TRUE;
	}

	// ここにくることはないはず
	*addr = 0;
	return FALSE;
}

// RGBX8888
static void set_mcolor(uint32_t c)
{
	p6m.mcolor = c;
}

// mbcolor = 0 なら透明色とする
static void set_mbcolor(uint32_t c)
{
	p6m.mbcolor = c;
}

// グラフィック座標
static void set_mlocate(int32_t x, int32_t y)
{
	p6m.ml_x = x, p6m.ml_y = y;
}

// キャラクタ文字の座標 (横軸は1座標が半角文字幅になる)
static void set_mlocateC(int32_t x, int32_t y)
{
	p6m.ml_x = x * p6m.mfs / 2, p6m.ml_y = y * p6m.mfs;
}

static void set_sbp(uint32_t *p)
{
	p6m.sbp = p;
}

// menu font size (16 or 24)
static void set_mfs(int32_t fs)
{
	p6m.mfs = fs;
}

static uint32_t *get_ml_ptr()
{
	p6m.mlp = p6m.sbp + MENU_WIDTH * p6m.ml_y + p6m.ml_x;
	return p6m.mlp;
}

// ・半角文字の場合は16bitの上位8bitにデータを入れておくこと
//   (半角or全角の判断ができるように)
// ・表示した分cursorは先に移動する
static void draw_char(uint16_t sjis)
{
	uint32_t fnt_ad;
	uint32_t *p;
	uint32_t c32;
	int32_t i, k, w, h;

	// char High 
	h = p6m.mfs;

	// get draw pointer
	p = get_ml_ptr();

	// get Font ROM address
	if(get_font_addr(sjis, h, &fnt_ad) == FALSE) return;
	fnt_ad |= 0xf00000; // X68000 font area

	// h=8は半角のみ
	w = (h == 8)? 8 : (isHankaku(sjis >> 8)? h / 2 : h);

	// Drawing char
	for (i = 0; i < h; i++) {
	 if(w/4 > 1){// 8x8 8x16 12x12 12x24 16x16 24x24
	   c32  = Memory_ReadB(fnt_ad) << 24;
	   fnt_ad++;
	 }
	 if(w/4 > 2){//          12x12 12x24 16x16 24x24
	   c32 |= Memory_ReadB(fnt_ad) << 16;
	   fnt_ad++;
	 }
	 if(w/4 > 5){//                            24x24
	   c32 |= Memory_ReadB(fnt_ad) <<  8;
	   fnt_ad++;
	 }
	 for (k = 0; k < w ; k++) {
	  if(0x80000000 & c32)  *(p+k) = p6m.mcolor;
	  else if(p6m.mbcolor) *(p+k) = p6m.mbcolor;
	  c32 <<= 1;
	 }
	 p = p + MENU_WIDTH;
	}

	p6m.ml_x += w;// 次の描画位置
}

/*--- 文字列を画面表示(sjis,utf8対応) ---*/
/*	flg = 0 sjis/utf8 (Auto)*/
/*	flg = 1 utf8 (Force)*/
static void draw_str(char *cp, uint32_t flg)
{
	uint32_t i, len, ret;
	uint8_t *s;
	uint16_t wc;
	char show_str[MAX_PATH];

	/*UTF8 or S-JIS*/
	ret = conv_utf8tosjis((char *)show_str, (char *)cp);
	if(flg==0){ /*auto select*/
		if(ret == 0) strcpy((char *)show_str, (char *)cp);
	}

	len = (uint32_t)strlen((char *)show_str);
	s   = (uint8_t *)show_str;

	for (i = 0; i < len; i++) {
		if (isHankaku(*s)) {
			// 最初の8bitで半全角を判断するので半角の場合は
			// あらかじめ8bit左シフトしておく
			draw_char((uint16_t)*s << 8);
			s++;
		} else {
			wc = (uint16_t)(*s << 8) + *(s + 1);
			draw_char(wc);
			s += 2;
			i++;
		}
		// 8x8描画(ソフトキーボードのFUNCキーは文字幅を縮める)
		if (p6m.mfs == 8) {
			p6m.ml_x -= 3;
		}
		/*Locate X dot check*/
		if((p6m.ml_x) > 790) break;
		/*Locate Y dot check*/
		if((p6m.ml_y) > 590) break;
	}

 return;
}

int32_t WinDraw_MenuInit(void)
{
#if defined(USE_OGLES20)
	//
	menu_buffer = malloc(1024 * 1024 * 2 + 2000);
	if (menu_buffer == NULL) {
		return FALSE;
	}
	set_sbp(menu_buffer);
	set_mfs(24);
#else

	/*SDL3 menu-surface */
	menu_surface = SDL_CreateSurface( 800, 600,SDL_PIXELFORMAT_RGBX8888);


	if (!menu_surface)
		return FALSE;
	set_sbp((uint32_t *)(menu_surface->pixels));
	set_mfs(24);
#endif

	set_mcolor(0xffffff00);// white
	set_mbcolor(0);

	return TRUE;
}

#ifdef USE_OGLES20
static void ogles11_draw_menu(void)
{
	GLfloat texture_coordinates[8];
	GLfloat vertices[8];

	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glBindTexture(GL_TEXTURE_2D, texid[9]);
	//ScrBufから800x600の領域を切り出してテクスチャに転送
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 800, 600, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, menu_buffer);

	// Texture から必要な部分を抜き出す
	// Texutre座標は0.0fから1.0fの間
	SET_GLFLOATS(texture_coordinates,
		     0.0f, (GLfloat)600/1024,
		     0.0f, 0.0f,
		     (GLfloat)800/1024, (GLfloat)600/1024,
		     (GLfloat)800/1024, 0.0f);

	// 実機の解像度(realdisp_w x realdisp_h)に関係なく、
	// 座標は800x600
	SET_GLFLOATS(vertices,
		     40.0f, (GLfloat)600,
		     40.0f, 0.0f,
		     (GLfloat)800, (GLfloat)600,
		     (GLfloat)800, 0.0f);

	CHG_GLFLOATS_800_600(vertices);
	glVertexAttribPointer(attr_pos, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)vertices);
	glVertexAttribPointer(attr_uv, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)texture_coordinates);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	draw_all_buttons(texture_coordinates, vertices, (GLfloat)WinUI_get_vkscale(), 1);

	SDL_GL_SwapWindow(sdl_window);
}
#endif

#include "softkey.c"

/*==Menu描画==*/
void WinDraw_DrawMenu(int32_t menu_state, int32_t mkey_pos, int32_t mkey_y, int32_t *mval_y)
{
	int32_t i, drv;
	char tmp[256];

// ソフトウェアキーボード描画時にset_sbp(kbd_buffer)されているので戻す
#if defined(USE_OGLES20)
	set_sbp(menu_buffer);
#endif
// ソフトウェアキーボード描画時にset_mfs(16)されているので戻す
#if defined(ANDROID) || TARGET_OS_IPHONE
	set_mfs(24);
#endif

	// タイトル
	if (scr_type == x68k) {
		set_mcolor(0x00ffff00); // cyan
		set_mlocateC(0, 0);
		draw_str(twaku_str,1);
		for (i = 0; i < p6m_colum_h; i++) {
		 set_mlocateC(0, 1 + i);
		 draw_str(twaku2_str,1);
		}
		set_mlocateC(0, 1 + p6m_colum_h);
		draw_str(twaku3_str,1);

		set_mcolor(0xffffff00);
		set_mlocateC(1, 1);
		sprintf(tmp, "%s%s", title_str, PX68KVERSTR);
		draw_str(tmp,1);
	} else {
		set_mcolor(0xffffff00); // cyan
		set_mlocateC(0, 0);
		sprintf(tmp, "%s%s", pc98_title1_str[Config.MenuLanguage], PX68KVERSTR);
		draw_str(tmp,1);
		set_mlocateC(0, 3);
		draw_str(pc98_title3_str[Config.MenuLanguage],1);
		set_mcolor(0xffffff00); // cyan
		set_mlocateC(0, 1);
		draw_str(pc98_title2_str,1);
	}

	// 真ん中
	if (scr_type == x68k) {
		set_mcolor(0xffffff00);
		set_mlocate(3 * p6m.mfs / 2, 3.4 * p6m.mfs);
		draw_str(waku_val_str1[Config.MenuLanguage],1);
		set_mlocate(17 * p6m.mfs / 2, 3.4 * p6m.mfs);
		draw_str(waku_val_str2[Config.MenuLanguage],1);

		// 真ん中枠
		set_mcolor(0xffff0000); // yellow
		set_mlocateC(1, 4);
		draw_str(waku_str,1);
		for (i = 0; i < p6m_colum_m; i++) {
			set_mlocateC(1, 5 + i);
			draw_str(waku2_str,1);
		}
		set_mlocateC(1, 5 + p6m_colum_m);
		draw_str(waku3_str,1);
	}

	// アイテム/キーワード
	set_mcolor(0xffffff00);
	for (i = 0; i < p6m_colum_m; i++) {
		set_mlocateC(3, 5 + i);
		if (menu_state == ms_key && i == (mkey_y - mkey_pos)) {
			set_mcolor(0x00000000);
			set_mbcolor(0xffff0000); // yellow;
		} else {
			set_mcolor(0xffffff00);
			set_mbcolor(0x00000000);
		}
		draw_str(menu_item_key[i + mkey_pos],1);
	}

	// アイテム/現在値
	set_mcolor(0xffffff00);
	set_mbcolor(0x00000000);
	for (i = 0; i < p6m_colum_m; i++) {
		if ((menu_state == ms_value || menu_state == ms_hwjoy_set)
		    && i == (mkey_y - mkey_pos)) {
			set_mcolor(0x00000000);
			set_mbcolor(0xffff0000); // yellow);
		} else {
			set_mcolor(0xffffff00);
			set_mbcolor(0x00000000);
		}
		if (scr_type == x68k) {
			set_mlocateC(17, 5 + i);
		} else {
			set_mlocateC(25, 5 + i);
		}

		drv = WinUI_get_drv_num(i + mkey_pos);
		if (drv >= 0  && mval_y[i + mkey_pos] == 0) {
			char *p;
			if (drv < 2) {
				p = (char *)Config.FDDImage[drv];
			} else {
				p = (char *)Config.HDImage[drv - 2];/*SASI*/
				if (p[0] == '\0') {
				  p = (char *)Config.SCSIEXHDImage[drv - 2];/*SCSI*/
				}
			}

			if (p[0] == '\0') {
				draw_str(" -- no disk --",1);
			} else {
			 // 先頭のカレントディレクトリ名を表示しない
			 if (!strncmp(cur_dir_str, p, cur_dir_slen)) {
				p += cur_dir_slen;
			 }
			 if(strlen(p)>44){ /*24dotで44文字以上*/
			  set_mfs(16);
			  if(strlen(p)>66){/*16dotで66文字以上*/
			   p += (strlen(p)-66);
			  }
			 }
			 draw_str(p,0);//Auto(S-JIS/UTF8)
			 set_mfs(24);/*元に戻す*/
			}
		} else {
			draw_str(menu_items[i + mkey_pos][mval_y[i + mkey_pos]],1);
			char *s = menu_item_key[i + mkey_pos];
			if (!strncmp("Sound", s, 5)) {// show Volume slider
			  set_mlocateC(37, 5 + i);
			  draw_str("Vol:",1);
			  for(int j=1;j<16;j++){
			   if(Config.OPM_VOL>j){
			     draw_str("+",1);
			   }else{
			     draw_str("-",1);
			   }
			  }
			}
		}
	}

	if (scr_type == x68k) {
		// 下枠
		set_mcolor(0x00ffff00); // cyan
		set_mbcolor(0x00000000);
		set_mlocateC(0, 18);
		draw_str(swaku_str,1);
		for (i = 0; i < p6m_colum_l; i++) {
		  set_mlocateC(0, 19 + i);
		  draw_str(swaku2_str,1);
		}
		set_mlocateC(0, 19 + p6m_colum_l);
		draw_str(swaku3_str,1);
	}

	// キャプション
	set_mcolor(0xffffff00);
	set_mbcolor(0x00000000);
	set_mlocateC(2, 19);
	if(Config.MenuLanguage == 0){draw_str(item_cap_JPN[mkey_y],1);}
	else						{draw_str(item_cap_US[mkey_y],1);}

	if (menu_state == ms_value) {
		set_mlocateC(2, 20);
	  if(Config.MenuLanguage == 0){draw_str(item_cap2_JPN[mkey_y],1);}
	  else						{draw_str(item_cap2_US[mkey_y],1);}
	}
#if defined(USE_OGLES20)
	ogles11_draw_menu();
#else

	Update_Screen(1); /* Draw Screen for Menu (SDL2) */

#endif
}

/*==File選択Menu描画==*/
void WinDraw_DrawMenufile(struct menu_flist *mfl)
{
	int32_t i;

	// 下枠
	//set_mcolor(0xf800); // red
	//set_mcolor(0xf81f); // magenta
	set_mcolor(0xffffff00);
	set_mbcolor(0x00000001); // 0x0だと透過モード
	set_mlocateC(1, 1);
	draw_str(swaku_str,1);
	for (i = 0; i < p6m_colum_f; i++) {
		set_mlocateC(1, 2 + i);
		draw_str(swaku2_str,1);
	}
	set_mlocateC(1, 2 + p6m_colum_f);
	draw_str(swaku3_str,1);

	for (i = 0; i < p6m_colum_f; i++) {
		if (i + 1 > mfl->num) {
			break;
		}
		if (i == mfl->y) {
			set_mcolor(0x00000000);
			set_mbcolor(0xffffff00);
		} else {
			set_mcolor(0xffffff00);
			set_mbcolor(0x0000000f);
		}
		// ディレクトリだったらファイル名を[]で囲う
		set_mlocateC(3, i + 2);
		if (mfl->type[i + mfl->ptr]) draw_str("[",1);
		draw_str(mfl->name[i + mfl->ptr],0);
		if (mfl->type[i + mfl->ptr]) draw_str("]",1);
	}

	set_mbcolor(0x00000000); // 透過モードに戻しておく

#if defined(USE_OGLES20)
	ogles11_draw_menu();
#else

	Update_Screen(1); /* Draw Screen for Menu (SDL2) */

#endif
}

void WinDraw_ClearMenuBuffer(uint32_t color)
{
#if defined(USE_OGLES20)
	memset(menu_buffer, color, 800*600*4);// 32bit depth
#else
	SDL_FillSurfaceRect(menu_surface, NULL, color);
#endif

}

/********** ソフトウェアキーボード描画 **********/

#if defined(USE_OGLES20)
// display width 800, buffer width 1024 だけれど 800 にしないとだめ
#define KBDBUF_WIDTH 800

#define KBD_FS 16 // keyboard font size : 16

// キーを反転する
void WinDraw_reverse_key(int32_t x, int32_t y)
{
	uint16_t *p;
	int32_t kp;
	int32_t i, j;
	
	kp = Keyboard_get_key_ptr(kbd_kx, kbd_ky);

	p = kbd_buffer + KBDBUF_WIDTH * kbd_key[kp].y + kbd_key[kp].x;

	for (i = 0; i < kbd_key[kp].h; i++) {
		for (j = 0; j < kbd_key[kp].w; j++) {
			*p = ~(*p);
			p++;
		}
		p = p + KBDBUF_WIDTH - kbd_key[kp].w;
	}
}

static void draw_kbd_to_tex()
{
	int32_t i, x, y;
	uint16_t *p;

	// SJIS 漢字コード
	char zen[] = {0x91, 0x53, 0x00};
	char larw[] = {0x81, 0xa9, 0x00};
	char rarw[] = {0x81, 0xa8, 0x00};
	char uarw[] = {0x81, 0xaa, 0x00};
	char darw[] = {0x81, 0xab, 0x00};
	char ka[] = {0x82, 0xa9, 0x00};
	char ro[] = {0x83, 0x8d, 0x00};
	char ko[] = {0x83, 0x52, 0x00};
	char ki[] = {0x8b, 0x4c, 0x00};
	char to[] = {0x93, 0x6f, 0x00};
	char hi[] = {0x82, 0xd0, 0x00};

	kbd_key[12].s = ka;
	kbd_key[13].s = ro;
	kbd_key[14].s = ko;
	kbd_key[16].s = ki;
	kbd_key[17].s = to;
	kbd_key[76].s = uarw;
	kbd_key[94].s = larw;
	kbd_key[95].s = darw;
	kbd_key[96].s = rarw;
	kbd_key[101].s = hi;
	kbd_key[108].s = zen;

	set_sbp(kbd_buffer);
	set_mfs(KBD_FS);
	set_mbcolor(0);
	set_mcolor(0);

	// キーボードの背景
	p = kbd_buffer;
	for (y = 0; y < kbd_h; y++) {
		for (x = 0; x < kbd_w; x++) {
			*p++ = (0x7800 | 0x03e0 | 0x000f);
		}
		p = p + KBDBUF_WIDTH - kbd_w;
	}

	// キーの描画
	for (i = 0; kbd_key[i].x != -1; i++) {
		p = kbd_buffer + kbd_key[i].y * KBDBUF_WIDTH + kbd_key[i].x;
		for (y = 0; y < kbd_key[i].h; y++) {
			for (x = 0; x < kbd_key[i].w; x++) {
				if (x == (kbd_key[i].w - 1)
				    || y == (kbd_key[i].h - 1)) {
					// キーに影をつけ立体的に見せる
					*p++ = 0x0000;
				} else {
					*p++ = 0xffff;
				}
			}
			p = p + KBDBUF_WIDTH - kbd_key[i].w;
		}
		if (strlen(kbd_key[i].s) == 3 && *(kbd_key[i].s) == 'F') {
			// FUNCキー刻印描画
			set_mlocate(kbd_key[i].x + kbd_key[i].w / 2
				    - strlen(kbd_key[i].s) * (8 / 2)
				    + (strlen(kbd_key[i].s) - 1) * 3 / 2,
				    kbd_key[i].y + kbd_key[i].h / 2 - 8 / 2);
			set_mfs(8);
			draw_str(kbd_key[i].s,1);
			set_mfs(KBD_FS);
		} else {
			// 刻印は上下左右ともセンタリングする
			set_mlocate(kbd_key[i].x + kbd_key[i].w / 2
				    - strlen(kbd_key[i].s) * (KBD_FS / 2 / 2),
				    kbd_key[i].y
				    + kbd_key[i].h / 2 - KBD_FS / 2);
			draw_str(kbd_key[i].s,1);
		}
	}

	WinDraw_reverse_key(kbd_kx, kbd_ky);
}

#endif
