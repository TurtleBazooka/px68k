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

#include "bg.h"
#include "crtc.h"
#include "gvram.h"
#include "palette.h"
#include "prop.h"
#include "status.h"
#include "tvram.h"

#if 0
#include "../icons/keropi.xpm"
#endif

extern int32_t Debug_Text, Debug_Grp, Debug_Sp;

uint32_t *ScrBuf = 0;

int32_t JoyDirection;
int32_t Draw_Opaque;
int32_t FullScreenFlag = 0;
extern uint8_t Draw_RedrawAllFlag;
uint8_t Draw_DrawFlag = 1;
uint8_t Draw_ClrMenu = 0;


int32_t  winx = 0, winy = 0; /*window position*/
uint32_t winh = 0, winw = 0; /*window width hight*/
uint32_t root_width, root_height; /*primary screen size*/
uint32_t FrameCount = 0;
int32_t  ScreenClearFlg = 0;

uint32_t WinDraw_Pal32B, WinDraw_Pal32R, WinDraw_Pal32G;

uint32_t WindowX = 0;
uint32_t WindowY = 0;

#ifdef USE_OGLES20
uint16_t *menu_buffer;
uint16_t *kbd_buffer;

static GLuint texid[11];

GLint attr_pos, attr_uv, texture;
GLuint shader_prog, v_shader, f_shader;
extern SDL_DisplayMode sdl_dispmode;
#else
SDL_Surface *menu_surface;
#endif

/* Drawing SDL3 buffer */
extern SDL_Window *sdl_window;
extern SDL_Renderer *sdl_render;
extern SDL_Texture *sdl_texture;
extern SDL_Surface *sdl_x68screen;

extern int32_t VID_MODE, CHANGEAV_TIMING;

#define	APPNAME	"Keropi"

static int32_t drawW=0,drawH=0; /*start X ,Y*/
static uint32_t surfaceW=800,surfaceH=600; /*width Hight*/
static uint32_t HLINE_TOTAL_1,CRTC_HSTART_1,CRTC_HEND_1;/*Store*/
static uint32_t VLINE_TOTAL_1,CRTC_VSTART_1,CRTC_VEND_1,CRTC_VStep_1;

/* X68 change screen size (Call from CRTC) */
/* Clear -> ChangeScreen -> Clear  */
int32_t WinDraw_ChangeSize(void)
{
	ScreenClearFlg=1;
	return TRUE;
}
/* X68 change screen size (Delayed Change) */
/* jadgment change or not.  */
int32_t WinDraw_ChangeSizeGO(void)
{

	if((HLINE_TOTAL<100) || (HLINE_TOTAL>1200)) return FALSE; 	/*H-DrawArea check*/
	if((VLINE_TOTAL<100) || (VLINE_TOTAL>700)) return FALSE; 	/*V-DrawArea check*/

	/* No change check */
	if((HLINE_TOTAL==HLINE_TOTAL_1) && (CRTC_HSTART==CRTC_HSTART_1) && (CRTC_HEND==CRTC_HEND_1) &&
		(VLINE_TOTAL==VLINE_TOTAL_1) && (CRTC_VSTART==CRTC_VSTART_1) && (CRTC_VEND==CRTC_VEND_1)&&(CRTC_VStep==CRTC_VStep_1)){
	return FALSE;
	}

	int32_t VLINE_TOTAL2,VSTART2,HSTART2=CRTC_HSTART*8;

	//垂直解像度補正(2Line描画)
	switch(CRTC_VStep){
		case 1:		VLINE_TOTAL2 = VLINE_TOTAL/2; VSTART2 = CRTC_VSTART / 2;
		  break;
		case 4:		VLINE_TOTAL2 = VLINE_TOTAL*2; VSTART2 = CRTC_VSTART * 2;
		  break;
		default:	VLINE_TOTAL2 = VLINE_TOTAL;  VSTART2 = CRTC_VSTART;
		  break;
	}

	// Screen Size は総数から非表示部分を取り除いて求める(TextDotX/Yは実際のCRTの表示範囲ではない)
	uint32_t ScreenX = HLINE_TOTAL  * 0.69; ScreenX /=32; ScreenX++; ScreenX *=32;
	uint32_t ScreenY = VLINE_TOTAL2 * 0.90; ScreenY /=32; ScreenY++; ScreenY *=32;
	
	// 表示範囲がオーバースキャンの場合
	if(ScreenX<TextDotX){ScreenX=TextDotX;HSTART2=0;}
	if(ScreenY<TextDotY){ScreenY=TextDotY;VSTART2=0;}

	//printf("TOTAL %d:%d ScrenXY %d:%d TdotXY %d:%d StartXY %d:%d\n",HLINE_TOTAL,VLINE_TOTAL2,ScreenX,ScreenY,
	//	TextDotX,TextDotY,HSTART2, VSTART2);

	/*=== Do Change X68000 Screen Size ===*/
	WinDraw_InitWindowSize(ScreenX, ScreenY, HSTART2, VSTART2);

	// 保存
	HLINE_TOTAL_1=HLINE_TOTAL;
	CRTC_HSTART_1=CRTC_HSTART;
	CRTC_HEND_1=CRTC_HEND;
	VLINE_TOTAL_1=VLINE_TOTAL;
	CRTC_VSTART_1=CRTC_VSTART;
	CRTC_VEND_1=CRTC_VEND;
	CRTC_VStep_1=CRTC_VStep;


	return TRUE;
}

/*Do Change Screen Size!*/
void
WinDraw_InitWindowSize(uint32_t ScreenX, uint32_t ScreenY, uint32_t StartX, uint32_t StartY)
{
	SDL_SetRenderDrawColor(sdl_render, 0, 0, 0, 0);	/* select color (black) */
	SDL_RenderClear(sdl_render);/*screen buffer clear*/

	switch(ScreenX){
	  case 513 ... 900: //====768x512/256   640x480/400/240/200====
		 switch(ScreenY){
		 case 257 ... 600:
		   surfaceW=ScreenX; drawW=StartX-224;
		   surfaceH=ScreenY; drawH=StartY-40;
		  break;
		 case 000 ... 256:
		   surfaceW=ScreenX; drawW=StartX-224;
		   surfaceH=ScreenY; drawH=StartY-20;
		  break;
		 }
		if(TextDotX==768){ surfaceW=800; drawW=((800-TextDotX)/2);}/*H-Dot2Dot*/
		if(TextDotY==512){ surfaceH=600; drawH=((600-TextDotY)/2);}/*V-Dot2Dot*/
	   break;
	  case 420 ... 512: //====512x512/256====
	   surfaceW=ScreenX; surfaceH=ScreenY; drawW=StartX-136;
		switch(ScreenY){
		 case 257 ... 600:
		   drawH=StartY-40;
		  break;
		 case 000 ... 256:
		   drawH=StartY-20;
		  break;
		}
	   break;
	  case 257 ... 419: //====416x256 384x256====
	   surfaceW=ScreenX; surfaceH=ScreenY; drawW=StartX-88;
		switch(ScreenY){
		 case 257 ... 600:
		   drawH=StartY-40;
		  break;
		 case 000 ... 256:
		   drawH=StartY-20;
		  break;
		}
	   break;
	  case 000 ... 256: //====256x256以下====
	   surfaceW=256; surfaceH=256; drawW=StartX-48;
		switch(ScreenY){
		 case 257 ... 600:
		   drawH=StartY-40;
		  break;
		 case 000 ... 256:
		   drawH=StartY-20;
		   break;
		}
	   break;
	}

	//printf("XY %d:%d %d:%d\n",surfaceW,surfaceH,drawW,drawH);

	//clipping...
	if(drawW<0){ drawW=0; }
	if(drawH<0){ drawH=0; }

	//nealy full screen...
	if((TextDotX!=768)||(TextDotY!=512)){//dot2dot以外
	  if((surfaceW - TextDotX)<(surfaceW*0.2)){surfaceW=TextDotX;drawW=0;}
	  if((surfaceH - TextDotY)<(surfaceH*0.2)){surfaceH=TextDotY;drawH=0;}
	}

	return;
}

/*プライマリ・Diplayの大きさをGET*/
void WinGetRootSize(void)
{
	/*==マルチ・ディスプレイ　プライマリ(0)の現在の解像度を調査==*/
	int num_modes = 0;
	SDL_DisplayMode **modes;
	const SDL_DisplayMode *mode;
	SDL_DisplayID display = SDL_GetPrimaryDisplay();

	modes = SDL_GetFullscreenDisplayModes(display, &num_modes);
	if (modes) {
		mode = modes[0];// No:0(Pri. Display)
		root_width   = mode->w;/*PrimaryDisplayの最大解像度*/
		root_height  = mode->h;
		//sdl_byte_per_pixel = mode->pixel_density; /*bit深度*/
		//SDL_free(modes);
		printf("メイン画面の最大解像度:%d x %d \n",root_width,root_height);
	}
	else{
		root_width   = 1820;/*ダミー解像度*/
		root_height  = 800;
	}

return;
}

//=== Clear Screen ===
void WinDraw_StartupScreen(void)
{
	/*clear screen:一度全画面描画更新しておく*/
	ScreenClearFlg=1;
	return;
}

// === Power OFF===
void WinDraw_CleanupScreen(void)
{
	SDL_DestroySurface(sdl_x68screen);
	SDL_DestroySurface(menu_surface);
	SDL_DestroyTexture(sdl_texture);
	SDL_DestroyRenderer(sdl_render);
	SDL_DestroyWindow(sdl_window);
}

/* Window Size set 
  0: 800x600(default) or Resizable
  1: full-screen  */
void WinDraw_ChangeMode(int32_t flg)
{
	uint32_t w_flags = 0;

	/* clear renderer */
	SDL_SetRenderDrawColor(sdl_render, 0, 0, 0, 0);	/* select color (black) */
	SDL_RenderClear(sdl_render);

	if (flg == 1) {
	  SDL_SetWindowFullscreen(sdl_window,TRUE);
	  SDL_HideCursor();
	}
	else{
	  SDL_SetWindowFullscreen(sdl_window,FALSE);
	  SDL_ShowCursor();
	}

	//if (Config.WinStrech == 1){w_flags |= SDL_WINDOW_RESIZABLE;}

	/* clear screen */
	ScreenClearFlg=1;

 return ;
}

static void draw_kbd_to_tex(void);

int32_t WinDraw_Init(void)
{
	int32_t i, j, k;

	WindowX = 768;
	WindowY = 512;

	/* SDL3 create Texture */
	sdl_texture = SDL_CreateTexture(sdl_render,SDL_PIXELFORMAT_RGBX8888,
		SDL_TEXTUREACCESS_STREAMING, 800, 600);
	if (sdl_texture == NULL) {
		fprintf(stderr, "can't create texture.\n");
		return FALSE;
	}

	// SDL2/3 Color Table
	WinDraw_Pal32R = 0xff000000;
	WinDraw_Pal32G = 0x00ff0000;
	WinDraw_Pal32B = 0x0000ff00;

#if defined(USE_OGLES20)

	SDL_GLContext glcontext = SDL_GL_CreateContext(sdl_window);

	// shader initialize
	//GLint attr_pos, attr_uv, texture;
	GLint gl_ret;

	const GLchar *v_shader_src =
		"attribute mediump vec4 attr_pos;"
		"attribute mediump vec2 attr_uv;"
		"varying mediump vec2 vary_uv;"
		"void main() {"
		"  gl_Position = attr_pos;"
		"  vary_uv = attr_uv;"
		"}";

	v_shader = glCreateShader(GL_VERTEX_SHADER);
	if (v_shader == 0) {
		p6logd("can't create vertex shader\n");
		return 1;
	}

	glShaderSource(v_shader, 1, &v_shader_src, NULL);

	glCompileShader(v_shader);

	gl_ret = 0;
	glGetShaderiv(v_shader, GL_COMPILE_STATUS, &gl_ret);
	if (gl_ret == GL_FALSE) {
		p6logd("vertex shader compile failed\n");
		return 1;
	}

	const GLchar *f_shader_src =
		"uniform sampler2D texture;"
		"varying mediump vec2 vary_uv;"
		"void main() {"
		"  gl_FragColor = texture2D(texture, vary_uv);"
		"}";

	f_shader = glCreateShader(GL_FRAGMENT_SHADER);
	if (f_shader == 0) {
		p6logd("can't create vertex shader\n");
		return 1;
	}

	glShaderSource(f_shader, 1, &f_shader_src, NULL);

	glCompileShader(f_shader);

	gl_ret = 0;
	glGetShaderiv(f_shader, GL_COMPILE_STATUS, &gl_ret);
	if (gl_ret == GL_FALSE) {
		p6logd("fragment shader compile failed\n");
		return 1;
	}

	shader_prog = glCreateProgram();
	if (shader_prog == 0) {
		p6logd("glCreateProgram failed\n");
	}


	glAttachShader(shader_prog, v_shader);
	glAttachShader(shader_prog, f_shader);

	glLinkProgram(shader_prog);

	gl_ret = 0;
	glGetProgramiv(shader_prog, GL_LINK_STATUS, &gl_ret);
	if (gl_ret == GL_FALSE) {
		p6logd("fragment shader compile failed\n");
		return 1;
	}

	attr_pos = glGetAttribLocation(shader_prog, "attr_pos");
	attr_uv = glGetAttribLocation(shader_prog, "attr_uv");
	texture = glGetUniformLocation(shader_prog, "texture");

	glUseProgram(shader_prog);


	glClearColor( 0, 0, 0, 0 );

	glViewport(0, 0, sdl_dispmode.w, sdl_dispmode.h);

	ScrBuf = malloc(1024*1024*2+2000); // OpenGL ES 1.1 needs 2^x pixels
	if (ScrBuf == NULL) {
		return FALSE;
	}

	kbd_buffer = malloc(1024*1024*2+2000); // OpenGL ES 1.1 needs 2^x pixels
	if (kbd_buffer == NULL) {
		return FALSE;
	}

	p6logd("kbd_buffer 0x%x", kbd_buffer);

	memset(texid, 0, sizeof(texid));
	glGenTextures(11, &texid[0]);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// texid[0] for the main screen
	glBindTexture(GL_TEXTURE_2D, texid[0]);
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
//	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
//	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 1024, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, ScrBuf);

	uint16_t BtnTex[32*32];
	//とりあえず薄めの緑で。
	for (i = 0; i < 32*32; i++) {
		BtnTex[i] = 0x03e0;
	}

	// ボタン用テクスチャ。とりあえず全部同じ色。
	for (i = 1; i < 9; i++) {
		glBindTexture(GL_TEXTURE_2D, texid[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		if (i == 7) {
			// とりあえずキーボードonボタンは薄めの黄色で。
			for (j = 0; j < 32*32; j++) {
				BtnTex[j] = (0x7800 | 0x03e0);
			}
		}
		if (i == 8) {
			// とりあえずmenu onボタンは薄めの白色で。
			for (j = 0; j < 32*32; j++) {
				BtnTex[j] = (0x7800 | 0x03e0 | 0x0f);
			}
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 32, 32, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, BtnTex);
	}

	// メニュー描画用テクスチャ。
	glBindTexture(GL_TEXTURE_2D, texid[9]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 1024, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, menu_buffer);

	draw_kbd_to_tex();

	// ソフトウェアキーボード描画用テクスチャ。
	glBindTexture(GL_TEXTURE_2D, texid[10]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 1024, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, kbd_buffer);

#else // OpenGL ES end

	/* X68000 Drawing Area for SDL3 */
	sdl_x68screen = SDL_CreateSurface(800, 600,SDL_PIXELFORMAT_RGBX8888 );

	if (sdl_x68screen == NULL) return FALSE;
	ScrBuf = sdl_x68screen->pixels;

#endif

	return TRUE;
}

void
WinDraw_Message(uint32_t Err_Mess_No)
{
  static const char *err_message[] = {
	"Boot up X68000 system.",
	"Low Memorry. (not enought RAM)",
	"No found iplrom.dat/cgrom.dat in .keropi folder.",
	"SDL window initialize Error.",
	"There is No-Sound Mode."
  };

  if(Err_Mess_No<=4){
   SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"WARNING!",err_message[Err_Mess_No],NULL);
  }

 return;
}

void
WinDraw_Cleanup(void)
{
#ifdef USE_OGLES20
	glUseProgram(0);
	glDeleteProgram(shader_prog);
	glDeleteShader(f_shader);
	glDeleteShader(v_shader);
#endif
}

void
WinDraw_Redraw(void)
{
	TVRAM_SetAllDirty();
}

#ifdef USE_OGLES20
#define SET_GLFLOATS(dst, a, b, c, d, e, f, g, h)		\
{								\
	dst[0] = (a); dst[1] = (b); dst[2] = (c); dst[3] = (d);	\
	dst[4] = (e); dst[5] = (f); dst[6] = (g); dst[7] = (h);	\
}

#define CHG_GLFLOATS_800_600(dst)				\
{								\
	int i;							\
	for (i = 0; i < 8; i += 2) {				\
		dst[i] = (dst[i] - 400.0f) / 400.0f;		\
		dst[i + 1] = -(dst[i + 1] - 300.0f) / 300.0f;	\
	}							\
}

void draw_button(GLuint texid, GLfloat x, GLfloat y, GLfloat s, GLfloat *tex, GLfloat *ver)
{
	glBindTexture(GL_TEXTURE_2D, texid);
	// Texture から必要な部分を抜き出す(32x32を全部使う)
	SET_GLFLOATS(tex, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	// s倍にして貼り付ける
	SET_GLFLOATS(ver, x, y + 32.0f * s, x, y, x + 32.0f * s, y + 32.0f * s, x + 32.0f * s, y + 0.0f);
	draw_texture(tex, ver);
	CHG_GLFLOATS_800_600(ver);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void draw_all_buttons(GLfloat *tex, GLfloat *ver, GLfloat scale, int32_t is_menu)
{
	int32_t i;
	VBTN_POINTS *p;

	p = Joystick_get_btn_points(scale);

	// 仮想キーはtexid: 1から6まで、キーボードonボタンが7、menuボタンが8
	for (i = 1; i < 9; i++) {
		if (need_Vpad() || i >= 7 || (Config.JoyOrMouse && i >= 5)) {
			draw_button(texid[i], p->x, p->y, scale, tex, ver);
		}
		p++;
	}
}
#endif // USE_OGLES20

/*==SDL3 Update Screen===*/
void
Update_Screen(uint32_t menu)
{
	SDL_FRect x68rect, CRTrect;

	/*surface(Main mem.) → texture(Frame buff.) */
	if(menu){
	  SDL_UpdateTexture(sdl_texture, NULL, menu_surface->pixels, menu_surface->pitch);//32bit depth
	  x68rect.x = CRTrect.x = 0;
	  x68rect.y = CRTrect.y = 0;
	  x68rect.w = CRTrect.w = 800;
	  x68rect.h = CRTrect.h = 600;
	}
	else{
	  SDL_UpdateTexture(sdl_texture, NULL, sdl_x68screen->pixels, sdl_x68screen->pitch);//32bit depth
	  /*texture → renderer copy rect */
	  x68rect.x = 0;
	  x68rect.y = 0;
	  x68rect.w = TextDotX;
	  x68rect.h = TextDotY;
	  CRTrect.x = (800*drawW/surfaceW);
	  CRTrect.y = (600*drawH/surfaceH);
	  CRTrect.w = (800*TextDotX/surfaceW);
	  CRTrect.h = (600*TextDotY/surfaceH);
	}

	// Render Clear every time
	  SDL_SetRenderDrawColor(sdl_render, 0, 0, 0, 0);	/* select color (black) */
	  SDL_RenderClear(sdl_render);		/*renderer buffer clear*/

	if(ScreenClearFlg != 0){ 			/*change screen(after clear screen)*/
	  WinDraw_ChangeSizeGO();
	  ScreenClearFlg=0;
	}

	SDL_RenderTexture(sdl_render, sdl_texture, &x68rect, &CRTrect);/*texture → renderer*/
	SDL_RenderPresent(sdl_render);								/* update screen */

 return;
}

/*描画バッファから表示バッファへの転送*/
/* X68K VRAM to Screen buffer   */
void FASTCALL
WinDraw_Draw(void)
{

#if defined(USE_OGLES20)
	GLfloat texture_coordinates[8];
	GLfloat vertices[8];
	GLfloat w, h;

	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_BLEND);

	// アルファブレンドしない(上のテクスチャが下のテクスチャを隠す)
	glBlendFunc(GL_ONE, GL_ZERO);

	//glDisable(GL_BLEND);

	glBindTexture(GL_TEXTURE_2D, texid[0]);
	//ScrBufから800x600の領域を切り出してテクスチャに転送
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 800, 600, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, ScrBuf);

	// X68K 画面描画

	glEnableVertexAttribArray(attr_pos);
	glEnableVertexAttribArray(attr_uv);
	glUniform1i(texture, 0);

	if (realdisp_w >= realdisp_h * 1.33333) {
		w = (realdisp_h * 1.33333f) / realdisp_w;
		h = 1.0f;
	} else {
		w = 1.0f;
		h = realdisp_w / 1.333333f / realdisp_h;
	}
	SET_GLFLOATS(vertices,
		     -w, h,
		     -w, -h,
		     w, h,
		     w, -h);

	// Texture から必要な部分を抜き出す
	// Texutre座標は0.0fから1.0fの間
	SET_GLFLOATS(texture_coordinates,
		     0, 0,
		     0, TextDotY/1024.0f,
		     TextDotX/1024.0f, 0, 
		     TextDotX/1024.0f, TextDotY/1024.0f);

	glVertexAttribPointer(attr_pos, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)vertices);
	glVertexAttribPointer(attr_uv, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)texture_coordinates);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	// ソフトウェアキーボード描画

	if (Keyboard_IsSwKeyboard()) {
		glBindTexture(GL_TEXTURE_2D, texid[10]);
		//kbd_bufferから800x600の領域を切り出してテクスチャに転送
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 800, 600, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, kbd_buffer);

		// Texture から必要な部分を抜き出す
		// Texutre座標は0.0fから1.0fの間
		SET_GLFLOATS(texture_coordinates,
			     0.0f, (GLfloat)kbd_h/1024,
			     0.0f, 0.0f,
			     (GLfloat)kbd_w/1024, (GLfloat)kbd_h/1024,
			     (GLfloat)kbd_w/1024, 0.0f);

		// 実機の解像度(realdisp_w x realdisp_h)に関係なく、
		// 座標は800x600

		float kbd_scale = 0.8;
		SET_GLFLOATS(vertices,
			     (GLfloat)kbd_x, (GLfloat)(kbd_h * kbd_scale + kbd_y),
			     (GLfloat)kbd_x, (GLfloat)kbd_y,
			     (GLfloat)(kbd_w * kbd_scale + kbd_x), (GLfloat)(kbd_h * kbd_scale + kbd_y),
			     (GLfloat)(kbd_w * kbd_scale + kbd_x), (GLfloat)kbd_y);

		CHG_GLFLOATS_800_600(vertices);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	// 仮想パッド/ボタン描画

	// アルファブレンドする(スケスケいやん)
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	draw_all_buttons(texture_coordinates, vertices, (GLfloat)WinUI_get_vkscale(), 0);

	SDL_GL_SwapWindow(sdl_window);

#else // end of OpenGLES

	Update_Screen(0); /* Draw Screen for X68000 (SDL2) */

#endif	// OpenGLES END

	FrameCount++;
	if (!Draw_DrawFlag/* && is_installed_idle_process()*/)
		return;
	Draw_DrawFlag = 0;

}

// 32bit depth Copy
#define WD_MEMCPY(src) memcpy(&ScrBuf[adr], (src), TextDotX * 4)


#define WD_LOOP(startA, end, sub)			\
{ 							\
	for (i = (startA); i < (end); i++, adr++) {	\
		sub();					\
	}						\
}

// 通常重ね合わせ
#define WD_SUB(SUFFIX, src)		\
{								\
	w = (src);					\
	if (w != 0)					\
		ScrBuf##SUFFIX[adr] = w;	\
}

// 半透明の重ね合わせ
#define WD_SUB2(SUFFIX, src)	\
{							\
	w = (src);				\
	if (w != 0)				\
		w >>= 8;			\
		v = ScrBuf##SUFFIX[adr];	\
		v >>= 8;		\
		v = (((v&0x00ff0000)+(w&0x00ff0000))>>1) & 0x00ff0000 |		\
			(((v&0x0000ff00)+(w&0x0000ff00))>>1) & 0x0000ff00 |		\
			(((v&0x000000ff)+(w&0x000000ff))>>1) & 0x000000ff; 		\
		v <<= 8;		\
		v &= Pal32_FullMask;		\
		ScrBuf##SUFFIX[adr] = v;	\
}

INLINE void WinDraw_DrawGrpLine(int32_t opaq)
{
#define _DGL_SUB(SUFFIX) WD_SUB(SUFFIX, Grp_LineBuf32[i])

	uint32_t adr = VLINE*FULLSCREEN_WIDTH;
	uint32_t w;
	uint32_t i;

	if (opaq) {
		WD_MEMCPY(Grp_LineBuf32);
	} else {
		WD_LOOP(0,  TextDotX, _DGL_SUB);
	}
}

INLINE void WinDraw_DrawGrpLineNonSP(int32_t opaq)
{
#define _DGL_NSP_SUB(SUFFIX) WD_SUB(SUFFIX, Grp_LineBuf32SP2[i])

	uint32_t adr = VLINE*FULLSCREEN_WIDTH;
	uint32_t w;
	uint32_t i;

	if (opaq) {
		WD_MEMCPY(Grp_LineBuf32SP2);
	} else {
		WD_LOOP(0,  TextDotX, _DGL_NSP_SUB);
	}
}

INLINE void WinDraw_DrawTextLine(int32_t opaq, int32_t td)
{
#define _DTL_SUB2(SUFFIX) WD_SUB(SUFFIX, BG_LineBuf32[i])
#define _DTL_SUB(SUFFIX)		\
{					\
	if (Text_TrFlag[i] & 1) {	\
		_DTL_SUB2(SUFFIX);	\
	}				\
}	

	uint32_t adr = VLINE*FULLSCREEN_WIDTH;
	uint32_t w;
	uint32_t i;

	if (opaq) {
		WD_MEMCPY(&BG_LineBuf32[16]);
	} else {
		if (td) {
			WD_LOOP(16, TextDotX + 16, _DTL_SUB);
		} else {
			WD_LOOP(16, TextDotX + 16, _DTL_SUB2);
		}
	}
}

INLINE void WinDraw_DrawTextLineTR(int32_t opaq)
{
#define _DTL_TR_SUB(SUFFIX)			   \
{						   \
	w = Grp_LineBuf32SP[i - 16];		   \
	if (w != 0) {				   \
		w >>= 8;	   \
		v = BG_LineBuf32[i];	   \
		v >>= 8;	   \
		v = (((v&0x00ff0000)+(w&0x00ff0000))>>1) & 0x00ff0000 |	   \
			(((v&0x0000ff00)+(w&0x0000ff00))>>1) & 0x0000ff00 |    \
			(((v&0x000000ff)+(w&0x000000ff))>>1) & 0x000000ff;	   \
		v <<= 8;	   \
		v &= Pal32_FullMask;		   \
	} else {				   \
		if (Text_TrFlag[i] & 1){		   \
			v = BG_LineBuf32[i];	   \
		}else{				   \
			v = 0;		   \
		}	   \
	}					   \
	ScrBuf##SUFFIX[adr] = (uint32_t)v;		   \
}

#define _DTL_TR_SUB2(SUFFIX)			   \
{						   \
	if (Text_TrFlag[i] & 1) {		   \
		w = Grp_LineBuf32SP[i - 16];	   \
		v = BG_LineBuf32[i];		   \
						   \
		if (v != 0) {			   \
			if (w != 0) {			\
				w >>= 8;	   \
				v >>= 8;	   \
				v = (((v&0x00ff0000)+(w&0x00ff0000))>>1) & 0x00ff0000 |	   \
					(((v&0x0000ff00)+(w&0x0000ff00))>>1) & 0x0000ff00 |    \
					(((v&0x000000ff)+(w&0x000000ff))>>1) & 0x000000ff;	   \
				v <<= 8;	   \
				v &= Pal32_FullMask;		   \
			}				\
			ScrBuf##SUFFIX[adr] = (uint32_t)v;	\
		}					\
	}						\
}

	uint32_t adr = VLINE*FULLSCREEN_WIDTH;
	uint32_t v;
	uint32_t w;
	uint32_t i;

	if (opaq) {
		WD_LOOP(16, TextDotX + 16, _DTL_TR_SUB);
	} else {
		WD_LOOP(16, TextDotX + 16, _DTL_TR_SUB2);
	}
}

INLINE void WinDraw_DrawBGLine(int32_t opaq, int32_t td)
{
#define _DBL_SUB2(SUFFIX) WD_SUB(SUFFIX, BG_LineBuf32[i])
#define _DBL_SUB(SUFFIX)			 \
{						 \
	if (Text_TrFlag[i] & 2) {		 \
		_DBL_SUB2(SUFFIX); \
	} \
}

	uint32_t adr = VLINE*FULLSCREEN_WIDTH;
	uint32_t w;
	uint32_t i;

#if 0 // debug for segv
	static int32_t log_start = 0;

	if (TextDotX == 128 && TextDotY == 128) {
		log_start = 1;
	}
	if (log_start) {
		printf("opaq/td: %d/%d VLINE: %d, TextDotX: %d\n", opaq, td, VLINE, TextDotX);
	}
#endif

	if (opaq) {
		WD_MEMCPY(&BG_LineBuf32[16]);
	} else {
		if (td) {
			WD_LOOP(16, TextDotX + 16, _DBL_SUB);
		} else {
			WD_LOOP(16, TextDotX + 16, _DBL_SUB2);
		}
	}
}

INLINE void WinDraw_DrawBGLineTR(int32_t opaq)
{

#define _DBL_TR_SUB3()			\
{					\
	if (w != 0) {			\
		w >>= 8;	   \
		v >>= 8;	   \
		v = (((v&0x00ff0000)+(w&0x00ff0000))>>1) & 0x00ff0000 |	   \
			(((v&0x0000ff00)+(w&0x0000ff00))>>1) & 0x0000ff00 |    \
			(((v&0x000000ff)+(w&0x000000ff))>>1) & 0x000000ff;	   \
		v <<= 8;	   \
		v &= Pal32_FullMask;		   \
	}				\
}

#define _DBL_TR_SUB(SUFFIX) \
{					\
	w = Grp_LineBuf32SP[i - 16];	\
	v = BG_LineBuf32[i];		\
					\
	_DBL_TR_SUB3()			\
	ScrBuf##SUFFIX[adr] = (uint32_t)v;	\
}

#define _DBL_TR_SUB2(SUFFIX) \
{							\
	if (Text_TrFlag[i] & 2) {  			\
		w = Grp_LineBuf32SP[i - 16];		\
		v = BG_LineBuf32[i];			\
							\
		if (v != 0) {				\
			_DBL_TR_SUB3()			\
			ScrBuf##SUFFIX[adr] = (uint32_t)v;	\
		}					\
	}						\
}

	uint32_t adr = VLINE*FULLSCREEN_WIDTH;
	uint32_t v;
	uint32_t w;
	uint32_t i;

	if (opaq) {
		WD_LOOP(16, TextDotX + 16, _DBL_TR_SUB);
	} else {
		WD_LOOP(16, TextDotX + 16, _DBL_TR_SUB2);
	}

}

// GR0 + GR1 (半透明重ね合わせ)
INLINE void WinDraw_DrawGrpLineTR(int32_t opaq)
{

#define _DGL_SUB2(SUFFIX) WD_SUB2(SUFFIX, Grp_LineBuf32[i])

	uint32_t adr = VLINE*FULLSCREEN_WIDTH;
	uint32_t v;
	uint32_t w;
	uint32_t i;

	if (opaq) {
		WD_MEMCPY(Grp_LineBuf32);//not use
	} else {
		WD_LOOP(0,  TextDotX, _DGL_SUB2);// G + G (half)
	}

}

INLINE void WinDraw_DrawPriLine(void)
{
#define _DPL_SUB(SUFFIX) WD_SUB(SUFFIX, Grp_LineBuf32SP[i])

	uint32_t adr = VLINE*FULLSCREEN_WIDTH;
	uint32_t w;
	uint32_t i;

	WD_LOOP(0, TextDotX, _DPL_SUB);
}

void WinDraw_DrawLine(void)
{
	int32_t opaq= 0; // Opaque Mode
	int32_t ton = 0; // TX ON
	int32_t gon = 0; // GR ON
	int32_t bgon= 0; // BacGround ON
	int32_t tron= 0; // Transparent ON
	int32_t pron= 0; // 特殊Priority ON

	if((VLINE < 0) || (VLINE >= 1024)){ return; } /* Area check */
	if (!TextDirtyLine[VLINE]) return;

	TextDirtyLine[VLINE] = 0;
	Draw_DrawFlag = 1;

	//  VCR2[0]:特殊BIT    [YS][AH][VHT][EXON][H/P][B/P][G/G][G/T]
	//  VCR2[1]:画面ON/OFF [ 0][SON][TON][GS4][GS3][GS2][GS1][GS0]
	if (Debug_Grp)
	{
	switch(VCReg0[1]&3)
	{
	case 0:					// 16 colors
		if (VCReg0[1]&4)		// 1024dot
		{
			if (VCReg2[1]&0x10)//GS4
			{
				if ( (VCReg2[0]&0x14)==0x14 )//EXON B/P
				{
					Grp_DrawLine4hSP();
					pron = tron = 1;
				}
				else
				{
					Grp_DrawLine4h();
					gon=1;
				}
			}
		}
		else				// 512dot
		{
			if ( (VCReg2[0]&0x10)&&(VCReg2[1]&1) )//EXON GS0
			{
				Grp_DrawLine4SP((VCReg1[1]   )&3/*, 1*/);			// 半透明の下準備
				pron = tron = 1;
			}
			opaq = 1;
			if (VCReg2[1]&8)
			{
				Grp_DrawLine4((VCReg1[1]>>6)&3, 1);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&4)
			{
				Grp_DrawLine4((VCReg1[1]>>4)&3, opaq);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&2)
			{
				if ( ((VCReg2[0]&0x1e)==0x1e)&&(tron) )
					Grp_DrawLine4TR((VCReg1[1]>>2)&3, opaq);
				else
					Grp_DrawLine4((VCReg1[1]>>2)&3, opaq);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&1)
			{
//				if ( (VCReg2[0]&0x1e)==0x1e )
//				{
//					Grp_DrawLine4SP((VCReg1[1]   )&3, opaq);
//					tron = pron = 1;
//				}
//				else
				if ( (VCReg2[0]&0x14)!=0x14 )
				{
					Grp_DrawLine4((VCReg1[1]   )&3, opaq);
					gon=1;
				}
			}
		}
		break;
	case 1:	
	case 2:	
		opaq = 1;		// 256 colors
		if ( (VCReg1[1]&3) <= ((VCReg1[1]>>4)&3) )	// 同じ値の時は、GRP0が優先（ドラスピ）
		{
			if ( (VCReg2[0]&0x10)&&(VCReg2[1]&1) )//EXON GS0
			{
				Grp_DrawLine8SP(0);			// 半透明の下準備
				tron = pron = 1;
			}
			if (VCReg2[1]&4)//GS2
			{
				if ( ((VCReg2[0]&0x1e)==0x1e)&&(tron) )
					Grp_DrawLine8TR(1, 1);
				else if ( ((VCReg2[0]&0x1d)==0x1d)&&(tron) )
					Grp_DrawLine8TR_GT(1, 1);
				else
					Grp_DrawLine8(1, 1);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&1)//GS0
			{
				if ( (VCReg2[0]&0x14)!=0x14 )//EXON B/P
				{
					Grp_DrawLine8(0, opaq);
					gon=1;
				}
			}
		}
		else
		{
			if ( (VCReg2[0]&0x10)&&(VCReg2[1]&1) )//EXON GS0
			{
				Grp_DrawLine8SP(1);			// 半透明の下準備
				tron = pron = 1;
			}
			if (VCReg2[1]&4)//GS2
			{
				if ( ((VCReg2[0]&0x1e)==0x1e)&&(tron) )
					Grp_DrawLine8TR(0, 1);
				else if ( ((VCReg2[0]&0x1d)==0x1d)&&(tron) )
					Grp_DrawLine8TR_GT(0, 1);
				else
					Grp_DrawLine8(0, 1);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&1)//GS0
			{
				if ( (VCReg2[0]&0x14)!=0x14 )//EXON B/P
				{
					Grp_DrawLine8(1, opaq);
					gon=1;
				}
			}
		}
		break;
	case 3:					// 65536 colors
		if (VCReg2[1]&15)//GS4 GS2 GS0
		{
			if ( (VCReg2[0]&0x14)==0x14 )//EXON B/P
			{
				Grp_DrawLine16SP();
				tron = pron = 1;
			}
			else
			{
				Grp_DrawLine16();
				gon=1;
			}
		}
		break;
	}
	}

	//  VCR1[0]: [xx][SP][TX][GR] 優先順描画前処理
	//  VCR2[1]:画面ON/OFF [ 0][SON][TON][GS4][GS3][GS2][GS1][GS0]
	if ( ((VCReg1[0]&0x30)>>2) < (VCReg1[0]&0x0c) )//SP < TX ?
	{						// SP<TX  SPの方が上
		if ((VCReg2[1]&0x20)&&(Debug_Text))//TEXT-ON
		{
			Text_DrawLine(1);
			ton = 1;
		}
		else
			memset(Text_TrFlag, 0, TextDotX+16);//TEXT-OFF

		if ((VCReg2[1]&0x40)&&(BG_Regs[8]&2)&&(!(BG_Regs[0x11]&2))&&(Debug_Sp))//SP-ON
		{
			int32_t s1, s2;
			s1 = (((BG_Regs[0x11]  &4)?2:1)-((BG_Regs[0x11]  &16)?1:0));
			s2 = (((CRTC_Regs[0x29]&4)?2:1)-((CRTC_Regs[0x29]&16)?1:0));
			VLINEBG = VLINE;
			VLINEBG <<= s1;
			VLINEBG >>= s2;
			if ( !(BG_Regs[0x11]&16) ) VLINEBG -= ((BG_Regs[0x0f]>>s1)-(CRTC_Regs[0x0d]>>s2));
			BG_DrawLine(!ton, 0);
			bgon = 1;
		}
	}
	else
	{						// SPよりTXの方が上
		if ((VCReg2[1]&0x40)&&(BG_Regs[8]&2)&&(!(BG_Regs[0x11]&2))&&(Debug_Sp))//SP-ON
		{
			int32_t s1, s2;
			s1 = (((BG_Regs[0x11]  &4)?2:1)-((BG_Regs[0x11]  &16)?1:0));
			s2 = (((CRTC_Regs[0x29]&4)?2:1)-((CRTC_Regs[0x29]&16)?1:0));
			VLINEBG = VLINE;
			VLINEBG <<= s1;
			VLINEBG >>= s2;
			if ( !(BG_Regs[0x11]&16) ) VLINEBG -= ((BG_Regs[0x0f]>>s1)-(CRTC_Regs[0x0d]>>s2));
			memset(Text_TrFlag, 0, TextDotX+16);//Clear
			BG_DrawLine(1, 1);
			bgon = 1;
		}
		else
		{
			if ((VCReg2[1]&0x20)&&(Debug_Text))//TX-ON
			{
				int_fast32_t i;
				for (i = 16; i < TextDotX + 16; ++i)
					BG_LineBuf32[i] = TextPal32[0];
			} else {		// 20010120 （琥珀色）
				memset(&BG_LineBuf32[16], 0, TextDotX * 4);// 32bit depth
			}
			memset(Text_TrFlag, 0, TextDotX+16);
			bgon = 1;
		}

		if ((VCReg2[1]&0x20)&&(Debug_Text))//TX-ON
		{
			Text_DrawLine(!bgon);
			ton = 1;
		}
	}

	//優先順描画 [xx][SP][TX][GR]  (00 > 01 > 10) 異常系
	// プライオリティが同じ場合は、GRP<SP<TEXT？（ドラスピ、桃伝、YsIII等）
	// GrpよりTextが上にある場合にTextとの半透明を行うと、SPのプライオリティも
	// Textに引きずられる？（つまり、Grpより下にあってもSPが表示される？）
	uint8_t priK = VCReg1[0];

	switch(VCReg1[0])
	{
	  case 0b00000110://SP0 TX1 GR2 正常系
	  case 0b00001001://SP0 TX2 GR1 正常系
	  case 0b00010010://SP1 TX0 GR2 正常系
	  case 0b00011000://SP1 TX2 GR0 正常系
	  case 0b00100001://SP2 TX0 GR1 正常系
	  case 0b00100100://SP2 TX1 GR0 正常系
		break;
	  case 0b00010001://SP1 TX0 GR1
	  case 0b00100010://SP2 TX0 GR2
	  case 0b00110011://SP3 TX0 GR3
	  case 0b00110010://SP3 TX0 GR2 (ThunderForce2)
		priK = 0b00010010;//SP1 TX0 GR2
		break;
	  case 0b00010100://SP1 TX1 GR0
	  case 0b00101000://SP2 TX2 GR0
	  case 0b00111100://SP3 TX3 GR0
		priK = 0b00011000;//SP1 TX2 GR0
		break;
	  case 0b00010000://SP1 TX0 GR0
	  case 0b00100000://SP2 TX0 GR0
	  case 0b00110000://SP3 TX0 GR0
	  case 0b00110001://SP3 TX0 GR1(信長)(GUNDAM)(Augasta)
		priK = 0b00100001;//SP2 TX0 GR1
		break;
	  case 0b00000100://SP0 TX1 GR0
	  case 0b00001000://SP0 TX2 GR0
	  case 0b00001100://SP0 TX3 GR0
	  case 0b00001101://SP0 TX3 GR1 (NOVELWARE)
		priK = 0b00001001;//SP0 TX2 GR1
		break;
	  case 0b00110100:// SP3 TX1 GR0 (R-TYPE)(GUNDAM)(Augasta)
		priK = 0b00100100; // SP2 TX1 GR0 
		break;
	  case 0b00011110://SP1 TX3 GR2 (ArcusOdyssey)
	  default:
		priK = 0b00000110;//SP0 TX1 GR2 基本形
		break;
	}

  static uint16_t VCstore;
  if(VCstore != (VCReg1[0]<<8 | VCReg2[0])){
    p6logd("Pri:%02X→%02X %02X  gon:%d tron:%d ton:%d bgon:%d\n",VCReg1[0],priK,VCReg2[0],gon,tron,ton,bgon);
    VCstore = (VCReg1[0]<<8 | VCReg2[0]);
  }

	opaq = 1;//1:初回描画 0:重ね合わせ(td 1:Text_TrFla含む 0:通常の重ね合わせ)
	int32_t tdrawed=0;//TX Drawed flg
	//(Text_TrFlag 0x01:TX重ね合わせ 0x02:BG重ね合わせ)

	//== 優先順描画 [xx][SP][TX][GR]  (00 > 01 > 10) 正常系6パターン ==
	switch(priK)
	{
	  case 0b00000110://SP0 TX1 GR2 (基本系)
		if (gon){//GR2
			WinDraw_DrawGrpLine(opaq);//GR2(初回BASE)
			opaq = 0;
		}
		if (tron) {//GR2 SPなし
			WinDraw_DrawGrpLineNonSP(opaq);
			opaq = 0;
		}
		if (ton){//TX1
			WinDraw_DrawTextLine(opaq, gon);// TXをBGに重ね合わせ
			opaq = 0;
			tdrawed = 1;
		}
		if (bgon) {//SP0 (TX1/2、BGの上)
			WinDraw_DrawBGLine(opaq,1);
			opaq = 0;
			tdrawed = 1;
		}
	   break;
	  case 0b00001001://SP0 TX2 GR1 (アルゴスの戦士)(中華大戦)(NewseaLandStory)(ああっお姫様)(FULLTHROTTLE)(STARLUSTERop)(悪魔状ドラキュラ)
		if (ton){//TX2
			if( (tron) && ((VCReg2[0]&0x5d)==0x1d) )//TX2 GR0/1 半透明(Ibit)
				WinDraw_DrawTextLineTR(opaq);
			else
				WinDraw_DrawTextLine(opaq, tdrawed);// (非透過)
			opaq = 0;
			tdrawed = 1;
		}
		if (gon){//GR1
			WinDraw_DrawGrpLine(opaq);//GR(初回BASE)
			opaq = 0;
			if( (tron) && (VCReg2[0]&0x10) ){//GR1 EXON
				WinDraw_DrawGrpLineNonSP(opaq);
				opaq = 0;
			}
		}
		if (bgon) {//SP0
			WinDraw_DrawBGLine(opaq,1);// (TX1/2) (BG透過)
			opaq = 0;
			tdrawed = 1;
		}
	   break;
	  case 0b00010010://SP1 TX0 GR2 (AlienSyndrome)(Phalanx)(Gradius)(DynamiteDuke)(Garou)(A-JAX)(CodeZero)(大魔界村)他多数
		if (gon){//GR2
			WinDraw_DrawGrpLine(opaq);//GR2(初回BASE)
			opaq = 0;
		}
		if (tron) {//GR2 SPなし
			WinDraw_DrawGrpLineNonSP(opaq);
			opaq = 0;
		}
		if (bgon) {//SP1 GR2
			WinDraw_DrawBGLine(opaq, 0); //（TX0だから)
			opaq = 0;
			tdrawed = 1;
		}
		if (ton) {//TX0
			WinDraw_DrawTextLine(opaq, 1);// (透過)
			opaq = 0;
			tdrawed = 1;
		}
	   break;
	  case 0b00011000://SP1 TX2 GR0(SP1見直すこと) (STARLUSTER)(SYNTHIAop)
		if (ton){//TX2
			if( (tron) && ((VCReg2[0]&0x5d)==0x1d))//GR+TX透過
				WinDraw_DrawTextLineTR(opaq);
			else
				WinDraw_DrawTextLine(opaq, tdrawed);//(非透過)
			opaq = 0;
			tdrawed = 1;
		}
		if (bgon) {//SP1
			if ( (tron) && ((VCReg2[0]&0x5d)==0x1d) ) //G/T透過
				WinDraw_DrawBGLineTR(opaq);
			else
				WinDraw_DrawBGLine(opaq, 1);//（TX2だから透過?) TX2だからtdは1？
			opaq = 0;
			tdrawed = 1;
		}
		if (gon) {
			WinDraw_DrawGrpLine(opaq);// GR0 (あんまりありえない)
			opaq = 0;
		}
		if ( (tron) && (VCReg2[0]&0x10) ){//GR0 EXON
			WinDraw_DrawGrpLineNonSP(opaq);
			opaq = 0;
		}
	   break;
	  case 0b00100001://SP2 TX0 GR1 (Phalanx) (KnightArms)(Y2)(FULLTHROTTLE)(MetalOrangeEX)
		if (bgon) {//SP2
			if ( (tron) && ((VCReg2[0]&0x5d)==0x1d) ){//G/T透過
				//G/T透過:GR描画後に優先度変更
			}
			else {
				WinDraw_DrawBGLine(opaq, 0);//GR描画(BASE)
				opaq = 0;
				tdrawed = 1;
			}
		}
		if (gon) {//GR1
			WinDraw_DrawGrpLine(opaq); // GR1 BG重ね合わせ
			opaq = 0;
			if ( (tron) && (VCReg2[0]&0x10) ){//GR1 EXON
				WinDraw_DrawGrpLineNonSP(opaq);
				opaq = 0;
			}
		}
		if (bgon) {//GR1 & SP2 重ね合わせ
			if( (tron) && ((VCReg2[0]&0x5d)==0x1d) ){//TX0 SP>GR G/X
			 WinDraw_DrawBGLineTR(opaq);//GR描画(BASE)
			 opaq = 0;
			 tdrawed = 1;
			 WinDraw_DrawGrpLineNonSP(opaq);
			}
			if( (tron) && ((VCReg2[0]&0x5e)==0x1e) ){//TX0 SP>GR G/G
			 if ( (VCReg1[1]&3) <= ((VCReg1[1]>>4)&3) ) {//GP0<=GP2 優先画面はどっち？
			   Grp_DrawLine8TR(1,1);}//GR画面1 再Load
			 else
			   Grp_DrawLine8TR(0,1);//GR画面0 再Load
			 WinDraw_DrawGrpLineTR(opaq);//GR描画（半透明重ね合わせ）
			 opaq = 0;
			 WinDraw_DrawGrpLineNonSP(opaq);//SP上書きカモだけどOK
			}
		}
		if (ton)  {//TX0
			WinDraw_DrawTextLine(opaq, 1 );// TX0 GR透過
			opaq = 0;
			tdrawed = 1;
		}
	   break;
	  case 0b00100100://SP2 TX1 GR0 (KnightArms)(MetalOrangeEX)(ChaseHQ)
		if (bgon ) {//SP2
			if ( (tron) && ((VCReg2[0]&0x5d)==0x1d) )//GR0  GR<TX
				WinDraw_DrawBGLineTR(opaq); //BASE(透過はありえない？)
			else
				WinDraw_DrawBGLine(opaq,tdrawed);// (0)
			opaq = 0;
			tdrawed = 1;
		}
		if (ton){//TX1
			if ( (tron) && ((VCReg2[0]&0x5d)==0x1d) )//G/T
				WinDraw_DrawTextLineTR(opaq); //BASE(透過はありえない？)
			else
				WinDraw_DrawTextLine(opaq,1);// SP2透過
			opaq = 0;
			tdrawed = 1;
		}
		if (gon) {//GR0
			WinDraw_DrawGrpLine(opaq);
			opaq = 0;
		}
		if ( (tron) && (VCReg2[0]&0x10) ){//GR0 EXON SP重ね合わせ
			WinDraw_DrawGrpLineNonSP(opaq);
			opaq = 0;
		}
	   break;
	  default:
		p6logd("No Draw:Priority Fail:%2X\n", VCReg1[0]);
	   break;
	}

#define _DL_SUB(SUFFIX) 		\
{								\
	w = Grp_LineBuf32SP[i];		\
	if (w != 0 && (ScrBuf##SUFFIX[adr] & Pal32_FullMask) == 0)	\
		ScrBuf##SUFFIX[adr] = (w & Pal32_HalfMask) >> 1;		\
}

	// ==特殊プライオリティ時のグラフィック==
	if ( ((VCReg2[0]&0x5c)==0x14)&&(pron) )	// 特殊Pri時は、対象プレーンビットは意味が無いらしい（ついんびー）
	{
		WinDraw_DrawPriLine();
	}
	else if ( ((VCReg2[0]&0x5d)==0x1c)&&(tron) )	// 半透明時に全てが透明なドットをハーフカラーで埋める
	{						// （AQUALES）
		uint32_t adr = VLINE*FULLSCREEN_WIDTH;
		uint32_t w;
		uint32_t i;

		WD_LOOP(0, TextDotX, _DL_SUB);
	}

	// ==後片付け==
	if (opaq)
	{
		uint32_t adr = VLINE*FULLSCREEN_WIDTH;

		memset(&ScrBuf[adr], 0, TextDotX * 4);//32bit depth clear
	}
}
