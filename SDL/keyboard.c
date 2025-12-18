/* 
 * Copyright (c) 2003,2008 NONAKA Kimihiro
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

#ifdef SDL2
#include "SDL2/SDL.h"
#else
#include "SDL3/SDL.h"
#endif

#include "GamePad.h"
#include "keymap.h"
#include "common.h"
#include "prop.h"
#include "keyboard.h"
#include "mfp.h"
#include "windraw.h"
#include "dosio.h"

// key input buffer
uint8_t	KeyBufWP;
uint8_t	KeyBufRP;
uint8_t	KeyBuf[KeyBufSize];
uint8_t	KeyEnable = 1;
uint8_t	KeyIntFlag = 0;

// key configuration MAP
char KEYCONFFILE[] = "keyconf.dat";
uint8_t KeyTable[SCANTABLE_MAX*2];

// ----------------------------------
//	SoftWare Keyboard MAP
// ----------------------------------
struct keyboard_key kbd_key[] = {
#include "keytbl.inc"
};

#if defined(ANDROID) || TARGET_OS_IPHONE
// キーボードの座標
int32_t kbd_x = 800, kbd_y = 0, kbd_w = 766, kbd_h = 218;
#endif


extern uint8_t traceflag;

// Convert hex to bin
uint32_t
hex2bin(char *str , uint32_t keta)
{
  uint32_t ans=0;
  char c;

  for(uint32_t i=0; i<keta; i++){
   c=*str++;
   if((c >= '0') && (c <= '9')){c -= '0'; }
   if((c >= 'A') && (c <= 'F')){c -= '7'; }
   if((c >= 'a') && (c <= 'f')){c -= 'W'; }
   ans = ans*16 + c;
  }
return ans;
}

// Key MAP init
void
Keymap_Init(void)
{
	FILEH fp;

	// Read Key Configuration File
	fp = File_OpenCurDir(KEYCONFFILE);
	if(fp)
	{
	  //File Read
	  File_Read(fp, KeyTable, SCANTABLE_MAX*2);
	  File_Close(fp);

	  //KeyMap rewrite
	  uint32_t map, key, x68key, i=0;
	  while(1)
	  {
	   map = hex2bin((char*)&KeyTable[i],2); i+=3;
	   key = hex2bin((char*)&KeyTable[i],3); i+=4;
	   x68key = hex2bin((char*)&KeyTable[i],2); i+=2;
	   if(map > 1) break;//終了

	   if((map<2)&&(key<SCANTABLE_MAX))
	     ScanTable[map][key] = (uint8_t)x68key;//KeyMap書き換え

	   while(KeyTable[i]!='\n'){i++;} i++; //改行まで飛ばす
	  }
	}

}

// Key buffer init
void
Keyboard_Init(void)
{

	// buff init.
	KeyBufWP = 0;
	KeyBufRP = 0;
	memset(KeyBuf, 0, KeyBufSize);
	KeyEnable = 1;
	KeyIntFlag = 0;

}


// P6K: PX68K_KEYBOARD
//      ~ ~   ~
#define P6K_UP 1
#define P6K_DOWN 2

//Keycode:bit0〜6 bit7:keyDown/Up
void send_keycode(uint8_t code, int32_t flag)
{
	uint8_t newwp;

	if (code != NC) {
		newwp = ((KeyBufWP + 1) & (KeyBufSize - 1));
		if (newwp != KeyBufRP) {
			KeyBuf[KeyBufWP] = code | ((flag == P6K_UP)? 0x80 : 0);
			KeyBufWP = newwp;
			//p6logd("KeyBufWP:%d KeyBuf[]:%x\n", KeyBufWP,KeyBuf[KeyBufWP]);
		}
	}

}

static int32_t shiftdown = 0;

// ----------------------------------
//	WM_KEYDOWN〜 (SDL_Keycode SDL_Scancode)
// ----------------------------------
void
Keyboard_KeyDown(uint32_t sdl_key, uint32_t sdl_scan)
{

	uint8_t code;
	uint8_t newwp;

#if !SDL_VERSION_ATLEAST(2, 0, 0) //==SDL1==
	if (sdl_key > SCANTABLE_MAX){
	  sdl_scan = sdl_key -(SCANTABLE_MAX - 0xe0);
	}
	else{
	 sdl_scan = sdl_key;
	}
#endif

	if (sdl_scan < SCANTABLE_MAX) { 		//SDL1/2キー ScanCode
		  code = ScanTable[shiftdown][sdl_scan];
	}
	else{
	  return;
	}
	if(code == 0x70){ shiftdown = 1; }

	//printf("Keyboard_KeyDown: ");
	//printf("wp=0x%x, code=0x%x\n", wp, code);
	//printf("SDLK_UP: 0x%x\n", SDLK_UP);

	// keycode remapping
	if(code & 0x80){
	 if(shiftdown){	// shift ON
		send_keycode(0x70, P6K_UP);
		send_keycode((code&0x7f), P6K_DOWN);
		send_keycode(0x70, P6K_DOWN);
	 }
	 else{			// shift off
		send_keycode(0x70, P6K_DOWN);
		send_keycode((code&0x7f), P6K_DOWN);
		send_keycode(0x70, P6K_UP);
	 }
	}
	else{
	 send_keycode(code, P6K_DOWN);
	}

	//printf("JoyKeyState: 0x%x\n", JoyKeyState);

	switch (sdl_scan) {
	case SDL_SCANCODE_UP:
		//puts("key up");
		if (!(JoyKeyState&JOY_DOWN))
			JoyKeyState |= JOY_UP;
		break;

	case SDL_SCANCODE_DOWN:
		if (!(JoyKeyState&JOY_UP))
			JoyKeyState |= JOY_DOWN;
		break;

	case SDL_SCANCODE_LEFT:
		if (!(JoyKeyState&JOY_RIGHT))
			JoyKeyState |= JOY_LEFT;
		break;

	case SDL_SCANCODE_RIGHT:
		if (!(JoyKeyState&JOY_LEFT))
			JoyKeyState |= JOY_RIGHT;
		break;

	case SDL_SCANCODE_Z:
		//puts("key z");
		if (Config.JoyKeyReverse)
			JoyKeyState |= JOY_TRGA;
		else
			JoyKeyState |= JOY_TRGB;
		break;

	case SDL_SCANCODE_X:
		//puts("key x");
		if (Config.JoyKeyReverse)
			JoyKeyState |= JOY_TRGB;
		else
			JoyKeyState |= JOY_TRGA;
		break;
	}
}

// ------------------------------------
//	WM_KEYUP  (SDL_Keycode SDL_Scancode)
// ------------------------------------
void
Keyboard_KeyUp(uint32_t sdl_key, uint32_t sdl_scan)
{
	int32_t  code;
	uint8_t newwp;

#if !SDL_VERSION_ATLEAST(2, 0, 0) //==SDL1==
	if (sdl_key > SCANTABLE_MAX){
	  sdl_scan = sdl_key -(SCANTABLE_MAX - 0xe0);
	}
	else{
	 sdl_scan = sdl_key;
	}
#endif

	if (sdl_scan < SCANTABLE_MAX) { 		//SDL1/2キー ScanCode
		  code = ScanTable[shiftdown][sdl_scan];
	}
	else{
	  return;
	}
	if(code == 0x70){ shiftdown = 0; }

	// keycode remapping
	if(code & 0x80){
	 if(shiftdown){	// shift ON
		send_keycode(0x70, P6K_UP);
		send_keycode((code&0x7f), P6K_UP);
		send_keycode(0x70, P6K_DOWN);
	 }
	 else{			// shift off
		send_keycode(0x70, P6K_DOWN);
		send_keycode((code&0x7f), P6K_UP);
		send_keycode(0x70, P6K_UP);
	 }
	}
	else{
	 send_keycode(code, P6K_UP);
	}


	//printf("JoyKeyState: 0x%x\n", JoyKeyState);

	switch(sdl_scan) {
	case SDL_SCANCODE_UP:
		JoyKeyState &= ~JOY_UP;
		break;

	case SDL_SCANCODE_DOWN:
		JoyKeyState &= ~JOY_DOWN;
		break;

	case SDL_SCANCODE_LEFT:
		JoyKeyState &= ~JOY_LEFT;
		break;

	case SDL_SCANCODE_RIGHT:
		JoyKeyState &= ~JOY_RIGHT;
		break;

	case SDL_SCANCODE_Z:
		if (Config.JoyKeyReverse)
			JoyKeyState &= ~JOY_TRGA;
		else
			JoyKeyState &= ~JOY_TRGB;
		break;

	case SDL_SCANCODE_X:
		if (Config.JoyKeyReverse)
			JoyKeyState &= ~JOY_TRGB;
		else
			JoyKeyState &= ~JOY_TRGA;
		break;
	}

}

// ----------------------------------
//	Key Check
//	1フレーム中に4回（2400bps/10bit/60fpsとすれば、だが）呼ばれて、MFPにデータを送る
// ----------------------------------

void
Keyboard_Int(void)
{
	if (KeyBufRP != KeyBufWP) {
		//p6logd("KeyBufRP:%d, KeyBufWP:%d\n", KeyBufRP, KeyBufWP);
		if (!KeyIntFlag) {
			LastKey = KeyBuf[KeyBufRP];
			KeyBufRP = ((KeyBufRP+1)&(KeyBufSize-1));
			KeyIntFlag = 1;
			MFP_Int(3);
		}
	} else if (!KeyIntFlag) {
		LastKey = 0;
	}
}

/********** ソフトウェアキーボード **********/

#if defined(USE_OGLES20)

// 選択しているキーの座標 (初期値'Q')
int32_t  kbd_kx = 1, kbd_ky = 2;

#define KEYXMAX 32767
int32_t Keyboard_get_key_ptr(int32_t x, int32_t y)
{
	int32_t i, j;
	int32_t p = 0;
	int32_t tx, ty; // tmp x, tmp y

	tx = KEYXMAX;
	ty = 0;
	i = 0;
	while (kbd_key[i].x != -1) {
		// skip dummy
		if (kbd_key[i].c == 0) {
			i++;
			continue;
		}
		if (tx > kbd_key[i].x) {
			if (ty == y)
				break;
			ty++;
		}
		tx = kbd_key[i++].x;
	}

	return i + x;
}

static void set_ptr_to_kxy(int32_t p)
{
	int32_t kx = 0, ky = 0;
	int32_t i;

	for (i = 0; i < p; i++) {
		if (kbd_key[i].x > kbd_key[i + 1].x) {
			ky++;
			kx = 0;
		} else {
			kx++;
		}
	}

	kbd_kx = kx, kbd_ky = ky;
}

static void set_near_key(int32_t p, int32_t tx)
{
	while (kbd_key[p].x < kbd_key[p + 1].x) {
		if (kbd_key[p].x >= tx) {
			// x座標の一番近いキーに移動する
			if (abs(kbd_key[p].x - tx)
			    > abs(tx - kbd_key[p - 1].x)) {
				p--;
			}
			break;
		}
		p++;
	}
	set_ptr_to_kxy(p);
}

static void key_up(void)
{
	int32_t p;
	int32_t c = 2;
	int32_t tx; // tmp x

	p = Keyboard_get_key_ptr(kbd_kx, kbd_ky);
	tx = kbd_key[p].x;

	// 上の行の先頭を探す
	while (p > 0) {
		if (kbd_key[p - 1].x > kbd_key[p].x) {
			c--;
			if (c == 0) {
				break;
			}
		}
		p--;
	}

	if (kbd_key[p].x == tx) {
		set_ptr_to_kxy(p);
		return;
	}

	set_near_key(p, tx);
}

static void key_down(void)
{
	int32_t p;
	int32_t tx; // tmp x

	p = Keyboard_get_key_ptr(kbd_kx, kbd_ky);
	tx = kbd_key[p].x;

	// 下の行の先頭を探す
	while (kbd_key[p].x != -1) {
		if (kbd_key[p + 1].x < kbd_key[p].x) {
			p++;
			break;
		}
		p++;
	}

	set_near_key(p, tx);
}


// dx と dy はどちらか一方は 0 とする。 dx: -1/0/+1 dy: -1/0/+1
static void mv_key(int32_t dx, int32_t dy)
{
	int32_t p;

	if ((kbd_ky == 0 && dy == -1) || (kbd_ky == 5 && dy == +1)) {
		return;
	}

	if (kbd_kx == 0 && dx == -1) {
		return;
	}
	p = Keyboard_get_key_ptr(kbd_kx, kbd_ky);
	if (dx == +1 &&
	    kbd_key[p + 1].x != -1 && kbd_key[p].x > kbd_key[p + 1].x) {
		return;
	}

	WinDraw_reverse_key(kbd_kx, kbd_ky);

	// 飛ばし先の微調整が必要なものたち
	// カーソルキー周りは面倒なので微調整なし
	if (kbd_kx == 12 && kbd_ky == 3 && dx == +1) {
		// ] の右となりは RET に飛ばす
		kbd_kx = 14, kbd_ky = 2;
	} else if (kbd_kx == 12 && kbd_ky == 5 && dx == +1) {
		// テンキーの . の右となりは ENT に飛ばす
		kbd_kx = 19, kbd_ky = 4;
	} else {
		// 微調整の必要なし
		kbd_kx += dx;
	}

	if (dy < 0) {
		key_up();
	}
	if (dy > 0) {
		key_down();
	}

	// RETの左側のダミーキーはRETに移動する
	if (kbd_kx == 13 && kbd_ky == 2) {
		if (dx == 0) {
			// 上下からダミーキーに行った場合
			kbd_kx = 14;
		} else {
			// 左右からダミーキーに行った場合
			kbd_kx += dx;
		}
	}

	WinDraw_reverse_key(kbd_kx, kbd_ky);
}

static void send_key(int32_t flag)
{

	uint8_t code;

	code = kbd_key[Keyboard_get_key_ptr(kbd_kx, kbd_ky)].c;
	//p6logd("sendkey: %d", code);

	send_keycode(code, flag);
}

void Keyboard_skbd(void)
{
	static kx = 1, ky = 2;
	uint8_t joy;

	joy = get_joy_downstate();
	reset_joy_downstate();

	if (!(joy & JOY_UP)) {
		mv_key(0, -1);
	}
	if (!(joy & JOY_DOWN)) {
		mv_key(0, +1);
	}
	if (!(joy & JOY_LEFT)) {
		mv_key(-1, 0);
	}
	if (!(joy & JOY_RIGHT)) {
		mv_key(+1, 0);
	}
	if (!(joy & JOY_TRG1)) {
		send_key(P6K_DOWN);
	}
	if (!(joy & JOY_TRG2)) {
		// BSキーをkey down
		send_keycode(0xf, P6K_DOWN);
	}

	joy = get_joy_upstate();
	reset_joy_upstate();
	if (joy & JOY_TRG1) {
		send_key(P6K_UP);
	}
	if (joy & JOY_TRG2) {
		// BSキーをkey up
		send_keycode(0xf, P6K_UP);
	}

}

int32_t skbd_mode = FALSE;

void Keyboard_ToggleSkbd(void)
{
	skbd_mode = (skbd_mode == TRUE)? FALSE : TRUE;
}

#endif //(USE_OGLES20)

int32_t Keyboard_IsSwKeyboard(void)
{
#if defined(USE_OGLES20)
	if (kbd_x < 700) {
		return TRUE;
	} else {
		return FALSE;
	}
#else
	return FALSE;
#endif
}
