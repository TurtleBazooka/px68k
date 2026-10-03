/* 
Software Keyboard for px68k by SDL2
use keymap-table:keytbl.inc
*/

SDL_Window   *sft_kbd_window;
SDL_Renderer *sft_kbd_render;
SDL_Texture  *sft_kbd_texture;
SDL_Surface  *keydraw_buffer;

uint8_t Key_X68, LED_X68;
static uint32_t sleep_counter;

/******* ソフトウェアキーボード描画 *******/

#define softkey_width 766
#define softkey_hight 218

#define softkey_fontsize 16 // keyboard font size:16

/*非表示で初期化*/
void Soft_kbd_CreateScreen(void)
{
	sft_kbd_window  = SDL_CreateWindow("X68000 keyboard", winx+16, winy+380, softkey_width, softkey_hight, SDL_WINDOW_HIDDEN);
	sft_kbd_render  = SDL_CreateRenderer(sft_kbd_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	sft_kbd_texture = SDL_CreateTexture(sft_kbd_render, SDL_PIXELFORMAT_RGBX8888,
							SDL_TEXTUREACCESS_STREAMING, softkey_width, softkey_hight);
	SDL_SetRenderTarget(sft_kbd_render, sft_kbd_texture);

	keydraw_buffer = SDL_CreateRGBSurface(0, softkey_width, softkey_hight, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0);//描画用

	if (sft_kbd_window == NULL) {
		p6logd("Soft_Keyboard Window: %ld", sft_kbd_window);
	}

 return;
}

/*終了時*/
void Soft_kbd_CleanupScreen(void)
{
	SDL_FreeSurface(keydraw_buffer);
	SDL_DestroyTexture(sft_kbd_texture);
	SDL_DestroyRenderer(sft_kbd_render);
	SDL_DestroyWindow(sft_kbd_window);

  return;
}

/*表示制御*/
void Soft_kbd_Show(uint32_t flg)
{
extern void draw_soft_kbd(uint32_t ms_x,uint32_t ms_y, uint8_t keyboardLED);
  if(flg == 0) SDL_HideWindow(sft_kbd_window);
  if(flg == 1){
	SDL_ShowWindow(sft_kbd_window);
	draw_soft_kbd(1,1,keyLED);//Redraw
  }
  if(flg == 2){
	SDL_SetWindowPosition(sft_kbd_window, winx+16, winy+430);
  }
  sleep_counter = 0;
  return;
}

/*描画する*/
void draw_soft_kbd(uint32_t ms_x,uint32_t ms_y, uint8_t keyboardLED)
{
	int32_t i, x, y,Bpp;
	uint32_t *p;
	uint32_t keycolor;
	SDL_Rect keyrect;
	uint32_t skb_backcolor;
	uint32_t skb_keycolor;

	Bpp = keydraw_buffer->format->BytesPerPixel;

	//sleep判定
	if((keyboardLED == 0xff) && (LED_X68 == 0x80)){
	  if(sleep_counter<180) sleep_counter++;
	}
	else{
	  if(keyboardLED != 0x80){ sleep_counter = 0; }
	}

	// LED変化なしの場合はret
	if((ms_x == 0)&&(ms_y == 0)){
		if(!(keyboardLED & 0x80) && (keyboardLED != 0)) return;
		if((keyboardLED & 0x7f) == (LED_X68 & 0x7f)) return;
	}

	//文字色設定
	set_mbcolor(0);
	set_mcolor(0);

	//描画色設定
	if(sleep_counter > 20){
		skb_backcolor= ~(sleep_counter<<24 | sleep_counter<<16 | sleep_counter<<8);
		skb_keycolor = ~((sleep_counter-10)<<24 | (sleep_counter-10)<<16 | (sleep_counter-10)<<8);
	}
	else{
		skb_backcolor= (0xd8000000 | 0x00d80000 | 0x0000d800);
		skb_keycolor = (0xf3000000 | 0x00f30000 | 0x0000f300);
	}

	// キーボードの背景(全画面塗りつぶし)
	SDL_FillRect(keydraw_buffer, NULL, skb_backcolor);

	// 全キーの描画
	for (i = 0; kbd_key[i].x != -1; i++) {

	  if((ms_x == 0)&&(ms_y == 0)){// key更新なし(LEDのみ)
		if(Key_X68 == kbd_key[i].c) keycolor = 0x33333300;
		else keycolor = skb_keycolor;
		LED_X68 = keyboardLED;
	  }
	  else{//key押下判定
		// KEY match
		if((kbd_key[i].x < ms_x) && (kbd_key[i].y < ms_y) && 
			(kbd_key[i].x+kbd_key[i].w > ms_x) && (kbd_key[i].y+kbd_key[i].h > ms_y)){
			keycolor = 0x33333300;
		}
		else{
			keycolor = skb_keycolor;
		}

		// KEY eventをX68000に送る
		if((Key_X68 != kbd_key[i].c) && (keycolor == 0x33333300)){
		  send_keycode((kbd_key[i].c & 0x7f), 2);// send keydown event
		  Key_X68 = kbd_key[i].c;
		}
		if((Key_X68 == kbd_key[i].c) && (keycolor == skb_keycolor)){
		  send_keycode((kbd_key[i].c & 0x7f), 1);// send keyup event
		  Key_X68 = 0;
		}
	  }

		// Key Rect Drawing
		keyrect.x=kbd_key[i].x; keyrect.y=kbd_key[i].y;
		keyrect.w=kbd_key[i].w; keyrect.h=kbd_key[i].h;
		SDL_FillRect(keydraw_buffer, &keyrect, 0x00000000);//shadow
		keyrect.w -=2; keyrect.h -=2;
		SDL_FillRect(keydraw_buffer, &keyrect, keycolor);

		// KeyTop Charactor
		x=kbd_key[i].x + (kbd_key[i].w-softkey_fontsize)/2 - ((strlen(kbd_key[i].s)-1)*2);
		y=kbd_key[i].y + (kbd_key[i].h-softkey_fontsize)/2 - 1;
		set_sbp(keydraw_buffer->pixels, keydraw_buffer->w);//描画準備
		set_mfs(softkey_fontsize);
		if(strlen(kbd_key[i].s2) == 0){
		 set_mlocate(x,y);
		 draw_str(kbd_key[i].s,1);
		}
		else{
		 set_mlocate(x-3,y-2);
		 draw_str(kbd_key[i].s,1);
		 set_mlocate(x+5,y+5);
		 draw_str(kbd_key[i].s2,1);
		}
		set_sbp((uint32_t *)(menu_surface->pixels), menu_surface->w);//元に戻す
		set_mfs(24);
	}

	// LED drawing
	keyrect.w=16; keyrect.h=6;//共通
	uint32_t KeyLED[] = {12,13,14,15,35,101,108};
	y = LED_X68;
	for(i=0; i<7; i++){
	 if(!(y & 0x01)){//かな
	   keyrect.x=kbd_key[KeyLED[i]].x +8; keyrect.y=kbd_key[KeyLED[i]].y +24;
	   SDL_FillRect(keydraw_buffer, &keyrect, (0x00000000 | 0x00ff0000 | 0x00000000));//緑
	 }
	 y >>= 1;
	}

	// Drawbuff to Texture
	void *dst_pixels;
	int   pitch;
	if (0 == SDL_LockTexture(sft_kbd_texture, NULL, &dst_pixels, &pitch)) {// lock texture
	  memcpy(dst_pixels, keydraw_buffer->pixels, pitch * softkey_hight );
	  SDL_UnlockTexture(sft_kbd_texture);// unlock texture
	}

	// 描画結果を画面に表示(SDL2)
	SDL_RenderCopy(sft_kbd_render, sft_kbd_texture, NULL, NULL);
	SDL_RenderPresent(sft_kbd_render);

	 // 元に戻す(念のため)
	set_mfs(24);
	set_sbp((uint32_t *)(menu_surface->pixels), menu_surface->w);

  return;
}

