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

/* -------------------------------------------------------------------------- *
 *  WINUI.C - UI                                                              *
 * -------------------------------------------------------------------------- */

#include <sys/stat.h>
#include <errno.h>

#include "GamePad.h"
#include "common.h"
#include "keyboard.h"
#include "windraw.h"
#include "prop.h"
#include "status.h"
#include "mouse.h"
#include "winx68k.h"
#include "version.h"
#include "fdd.h"
#include "irqh.h"
#include "../m68000/m68000.h"
#include "crtc.h"
#include "mfp.h"
#include "fdc.h"
#include "disk_d88.h"
#include "dmac.h"
#include "ioc.h"
#include "rtc.h"
#include "sasi.h"
#include "bg.h"
#include "palette.h"
#include "crtc.h"
#include "ppi.h"
#include "scc.h"
#include "midi.h"
#include "adpcm.h"
#include "mercury.h"
#include "tvram.h"
#include "winui.h"
#include "sram.h"

#include <dirent.h>
#include <sys/stat.h>

// 外部参照用
extern	uint8_t		fdctrace;
extern	uint8_t		traceflag;

extern  uint8_t SRAM[];

extern	uint32_t	FrameCount;
extern	uint32_t	TimerICount;
extern	uint32_t	hTimerID;
		uint32_t	timertick=0;
extern	int32_t		FullScreenFlag;
		int32_t		UI_MouseFlag = 0;
		int32_t		UI_MouseX = -1, UI_MouseY = -1;
extern	int16_t		timertrace;

		int32_t		MenuClearFlag = 0;

		int32_t		Debug_Text=1, Debug_Grp=1, Debug_Sp=1;

		char		filepath[MAX_PATH] = ".";
		int32_t		fddblink = 0;
		int32_t		fddblinkcount = 0;

		int32_t		hddtrace = 0;
extern  int32_t		dmatrace;

		int32_t		LastClock[4] = {0, 0, 0, 0};

		char cur_dir_str[MAX_PATH];
		int32_t cur_dir_slen;

struct menu_flist mfl;

/***** menu items *****/

#define MENU_NUM 17
#define MENU_WINDOW 7

int32_t mval_y[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 0, 0, 1, 1, 1}; /*初期値*/

enum menu_id {M_SYS, M_JOM, M_FD0, M_FD1, M_HD0, M_HD1, M_FS, M_SR, M_MO, M_MI, M_VKS, M_VBS, M_JMD, M_HJS, M_NW, M_JK, M_RAM};

// Max # of characters is 17.
char menu_item_key[][18] = {"SYSTEM", "Joy/Mouse", "FDD0", "FDD1", "HDD0", "HDD1", "Frame Skip", "Sound Rate", "MIDI Out", "MIDI In ", "VKey Size", "VBtn Swap", "GamePad Mode", "USB GamePad", "No Wait Mode", "JoyKey", "RAM", "uhyo", ""};

// Max # of characters is 30.
// Max # of items including terminater `""' in each line is 15.
char menu_items[][18][100] = {
	{"RESET", "NMI RESET", "QUIT", "Eject MO", "SRAM-Clear and RESET", ""},
	{"Joystick", "Mouse", ""},
	{"dummy", "EJECT", ""},
	{"dummy", "EJECT", ""},
	{"dummy", "EJECT", ""},
	{"dummy", "EJECT", ""},
	{"Auto Frame Skip", "Full Frame", "1/2 Frame", "1/3 Frame", "1/4 Frame", "1/5 Frame", "1/6 Frame", "1/8 Frame", "1/16 Frame", "1/32 Frame", "1/60 Frame", ""},
	{"No Sound", "11025Hz", "22050Hz", "44100Hz", "48000Hz", ""},
	{"Port0", "Port1", "Port2", "Port3", "Port4", "Port5", "Port6", "Port7", ""},
	{"Port0", "Port1", "Port2", "Port3", "Port4", "Port5", "Port6", "Port7", ""},
	{"Ultra Huge", "Super Huge", "Huge", "Large", "Medium", "Small", ""},
	{"TRG-B TRG-A", "TRG-A TRG-B", ""},
	{"Digital", "Analog","Auto detect", ""},
	{"Axis0: X", "Axis1: Y", "Throttle: Z", "TRG-A: ", "TRG-B: ", "TRG-C: ", "TRG-D(X): ", "Th-UP(Y): ", "Th-Down(Z): ", "TRG-E1: ", "TRG-E2: ",  ""},
	{"Off", "On", ""},
	{"Off", "On", ""},
	{"1MB", "2MB", "4MB", "8MB", "12MB", ""}
};

static void menu_system(int32_t v);
static void menu_joy_or_mouse(int32_t v);
static void menu_create_flist(int32_t v);
static void menu_frame_skip(int32_t v);
static void menu_sound_rate(int32_t v);
static void menu_midout_rate(int32_t v);
static void menu_midin_rate(int32_t v);
static void menu_vkey_size(int32_t v);
static void menu_vbtn_swap(int32_t v);
static void menu_joymode_setting(int32_t v);
static void menu_hwjoy_setting(int32_t v);
static void menu_nowait(int32_t v);
static void menu_joykey(int32_t v);
static void menu_ram_size(int32_t v);

struct _menu_func {
	void (*func)(int32_t v);
	int32_t imm;
};

struct _menu_func menu_func[] = {
	{menu_system, 0}, 
	{menu_joy_or_mouse, 1},
	{menu_create_flist, 0},
	{menu_create_flist, 0},
	{menu_create_flist, 0},
	{menu_create_flist, 0},
	{menu_frame_skip, 1},
	{menu_sound_rate, 1},
	{menu_midout_rate, 1},
	{menu_midin_rate, 1},
	{menu_vkey_size, 1},
	{menu_vbtn_swap, 1},
	{menu_joymode_setting, 1},
	{menu_hwjoy_setting, 0},
	{menu_nowait, 1},
	{menu_joykey, 1},
	{menu_ram_size, 1}
};

int32_t WinUI_get_drv_num(int32_t key)
{
	char *s = menu_item_key[key];

	if (!strncmp("FDD", s, 3)) {
		return strcmp("FDD0", s)?
			(strcmp("FDD1", s)? -1 : 1) : 0;
	} else {
		return strcmp("HDD0", s)?
			(strcmp("HDD1", s)? -1: 3) : 2;
	}
}

static void menu_hwjoy_print(int32_t v)
{
	return;/*--if debug then delete*/

	if (v <= 2) {
		sprintf(menu_items[M_HJS][v], "Axis%d(%s): %d",
			v,
			(v == 0)? "Left/Right" : "Up/Down",
			Config.HwJoyAxis[v]);
	} else {
		sprintf(menu_items[M_HJS][v], "Button%d: %d",
			v - 3,
			Config.HwJoyBtn[v - 3]);
	}
}

/******************************************************************************
 * init
 ******************************************************************************/
void
WinUI_Init(void)
{
	int32_t i;

	mval_y[M_JOM] = Config.JoyOrMouse;
	if (Config.FrameRate == 7) {
		mval_y[M_FS] = 0;
	} else if (Config.FrameRate == 8) {
		mval_y[M_FS] = 7;
	} else if (Config.FrameRate == 16) {
		mval_y[M_FS] = 8;
	} else if (Config.FrameRate == 32) {
		mval_y[M_FS] = 9;
	} else if (Config.FrameRate == 60) {
		mval_y[M_FS] = 10;
	} else {
		mval_y[M_FS] = Config.FrameRate;
	}

	if (Config.SampleRate == 0) {
		mval_y[M_SR] = 0;
	} else if (Config.SampleRate == 11025) {
		mval_y[M_SR] = 1;
	} else if (Config.SampleRate == 22050) {
		mval_y[M_SR] = 2;
	} else if (Config.SampleRate == 44100) {
		mval_y[M_SR] = 3;
	} else if (Config.SampleRate == 48000) {
		mval_y[M_SR] = 4;
	} else {
		mval_y[M_SR] = 1;
	}

	mval_y[M_VKS] = Config.VkeyScale;
	mval_y[M_VBS] = Config.VbtnSwap;

	for (i = 0; i < 11; i++) {
		menu_hwjoy_print(i);
	}

	mval_y[M_NW] = Config.NoWaitMode;
	mval_y[M_JK] = Config.JoyKey;
	mval_y[M_JMD] = Config.Joymode;

	if (Config.ram_size == 1) {
		mval_y[M_RAM] = 0;
	} else if (Config.ram_size == 2) {
		mval_y[M_RAM] = 1;
	} else if (Config.ram_size == 4) {
		mval_y[M_RAM] = 2;
	} else if (Config.ram_size == 8) {
		mval_y[M_RAM] = 3;
	} else if (Config.ram_size == 12) {
		mval_y[M_RAM] = 4;
	} else {
		mval_y[M_RAM] = 1;
	}



#if defined(ANDROID)
	char CUR_DIR_STR[] = {"winx68k_dir"};
#elif TARGET_OS_IPHONE && TARGET_IPHONE_SIMULATOR == 0
	char CUR_DIR_STR[] = {"/var/mobile/px68k/"};
#else
	char CUR_DIR_STR[MAX_PATH];
	strcpy(CUR_DIR_STR, getenv("HOME"));
#endif

	if (filepath[0]!='\0') {
		strcpy(cur_dir_str, filepath);
	}
	else{
		strcpy(cur_dir_str, CUR_DIR_STR);
	}

#ifdef _WIN32
		if (cur_dir_str[strlen(cur_dir_str)-1]!='\\')
			strcat(cur_dir_str, "\\");
#else
		if (cur_dir_str[strlen(cur_dir_str)-1]!='/')
			strcat(cur_dir_str, "/");
#endif

	cur_dir_slen = (int32_t)strlen(cur_dir_str);
	p6logd("cur_dir_str %s %d\n", cur_dir_str, cur_dir_slen);

	for (i = 0; i < 4; i++) {
		strcpy(mfl.dir[i], cur_dir_str);
	}
}

#if 0
/*
 * item function
 */
static void
reset(gpointer data, guint action, GtkWidget *w)
{
	WinX68k_Reset();
	if (Config.MIDI_SW && Config.MIDI_Reset)
		MIDI_Reset();
}

static void
stretch(gpointer data, guint action, GtkWidget *w)
{
	UNUSED(data);
	UNUSED(w);

	if (Config.WinStrech != (int32_t)action)
		Config.WinStrech = action;
}

static void
xvimode(gpointer data, guint action, GtkWidget *w)
{
	UNUSED(data);
	UNUSED(w);

	if (Config.XVIMode != (int32_t)action)
		Config.XVIMode = action;
}

static void
videoreg_save(gpointer data, guint action, GtkWidget *w)
{
	char buf[256];

	UNUSED(data);
	UNUSED(action);
	UNUSED(w);

	DSound_Stop();
	g_snprintf(buf, sizeof(buf),
	             "VCReg 0:$%02X%02X 1:$%02x%02X 2:$%02X%02X  "
	             "CRTC00/02/05/06=%02X/%02X/%02X/%02X%02X  "
		     "BGHT/HD/VD=%02X/%02X/%02X   $%02X/$%02X",
	    VCReg0[0], VCReg0[1], VCReg1[0], VCReg1[1], VCReg2[0], VCReg2[1],
	    CRTC_Regs[0x01], CRTC_Regs[0x05], CRTC_Regs[0x0b], CRTC_Regs[0x0c],
	      CRTC_Regs[0x0d],
	    BG_Regs[0x0b], BG_Regs[0x0d], BG_Regs[0x0f],
	    CRTC_Regs[0x29], BG_Regs[0x11]);
	Error(buf);
	DSound_Play();
}

#endif

float VKey_scale[] = {3.0, 2.5, 2.0, 1.5, 1.25, 1.0};

float WinUI_get_vkscale(void)
{
	int32_t n = Config.VkeyScale;

	// failsafe against invalid values
	if (n < 0 || n >= sizeof(VKey_scale)/sizeof(float)) {
		return 1.0;
	}
	return VKey_scale[n];
}

int32_t menu_state = ms_key;
int32_t mkey_y = 0;
int32_t mkey_pos = 0;

static void menu_system(int32_t v)
{
	switch (v) {
	case 1:
		IRQH_Int(7, NULL);
		break;
	case 2:
		return; /*quit for main loop*/
		break;
	case 3:
		Config.SCSIEXHDImage[5][0] = '\0'; /*Eject MO (SCSI-ID=5)*/
		break;
	case 4:
		SRAM_Clear();
	case 0 :
		WinX68k_Reset();
		break;
	}
	ScreenClearFlg=1;
	mval_y[M_SYS] = 0;//メニュー選択をReset
}

static void menu_joy_or_mouse(int32_t v)
{
	Config.JoyOrMouse = v;
	Mouse_StartCapture(v == 1);
}


static void upper(char *s)
{
	while (*s != '\0') {
		if (*s >= 'a' && *s <= 'z') {
			*s = 'A' + *s - 'a';
		}
		s++;
	}
}

/*昇順にソートする比較関数*/
int32_t fnamecmp(const void* p1, const void* p2) {
    const char* str1 = (const char*)p1;
    const char* str2 = (const char*)p2;
    return strcmp(str1, str2);
}

/*------Open File List------*/
static void menu_create_flist(int32_t v)
{
	int32_t drv;
	//file extension of FD image
	char support[] = "D8888DHDMDUP2HDDIMXDFIMG";

	drv = WinUI_get_drv_num(mkey_y);
	//p6logd("*** drv:%d ***** %s \n", drv, mfl.dir[drv]);
	if (drv < 0) {
		return;
	}

	// set current directory when FDD/HDD is ejected
	if (v == 1) {
		if (drv < 2) {
			FDD_EjectFD(drv);
			Config.FDDImage[drv][0] = '\0';
		} else {
			Config.HDImage[drv - 2][0] = '\0';
			Config.SCSIEXHDImage[drv - 2][0] = '\0';
		}
		//strcpy(mfl.dir[drv], cur_dir_str); いちいちカレントに戻さない方がいい！
		return;
	}

	if (drv >= 2) {
		strcpy(support, "HDFHDSMOS");/*SASI-HDD:HDF SCSI-HDD:HDS SCSI-MO:MOS*/
	}

	// This routine gets file lists.
	DIR *dp;
	struct dirent *dent;
	struct stat buf;
	int_fast32_t i,j,k;
	int32_t l, len;
	char *n, ext[4], *p;
	char ent_name[MAX_PATH];
	char srt[100];

	dp = opendir(mfl.dir[drv]);
	if(dp == NULL ) {return;} // xxx check if dp is null...

	// xxx You can get only MFL_MAX files.
	for (i = 0 ; i < MFL_MAX; i++) {
		dent = readdir(dp);
		if (dent == NULL) {
			break;
		}
		n = dent->d_name;
		strcpy(ent_name, mfl.dir[drv]);
		strcat(ent_name, n);
		stat(ent_name, &buf);

		if (!S_ISDIR(buf.st_mode)) {
			// Check extension if this is file.
			len = (int32_t)strlen(n);
			if (len < 4 || *(n + len - 4) != '.') {
				i--;
				continue;
			}
			strcpy(ext, n + len - 3);
			upper(ext);	 /*拡張子判別用大文字変換*/
			p = strstr(support, ext);
			if (p == NULL || (p - support) % 3 != 0) {
				i--;
				continue;
			}
		} else {
			// current directory.
			if (!strcmp(n, ".")) {
				i--;
				continue;
			}

			// You can't go up over current directory.
			if (!strcmp(n, "..") &&
			    !strcmp(mfl.dir[drv], cur_dir_str)) {
				i--;
				continue;
			}
		}

		strcpy(mfl.name[i], n);
		// set 1 if this is directory
		mfl.type[i] = S_ISDIR(buf.st_mode)? 1 : 0;
		printf("%s 0x%x\n", n, buf.st_mode);
	}

	closedir(dp);

	/*==ファイル名を昇順に並び替え==*/
	for(j=0; j<i; j++){
		for(k=j+1; k<i; k++)
		if (fnamecmp(mfl.name[j],mfl.name[k]) > 0){
			strcpy(srt , mfl.name[j]);
			strcpy(mfl.name[j] , mfl.name[k]);
			strcpy(mfl.name[k] , srt);
			l = mfl.type[j];
			mfl.type[j] = mfl.type[k];
			mfl.type[k] = l;
		}
	}

	strcpy(mfl.name[i], "");
	mfl.num = i;
	mfl.ptr = 0;

}


static void menu_frame_skip(int32_t v)
{
	if (v == 0) {
		Config.FrameRate = 7;
	} else if (v == 7) {
		Config.FrameRate = 8;
	} else if (v == 8) {
		Config.FrameRate = 16;
	} else if (v == 9) {
		Config.FrameRate = 32;
	} else if (v == 10) {
		Config.FrameRate = 60;
	} else {
		Config.FrameRate = v;
	}
}

static void menu_sound_rate(int32_t v)
{
	if (v == 0) {
		Config.SampleRate = 0;
	} else if (v == 1) {
		Config.SampleRate = 11025;
	} else if (v == 2) {
		Config.SampleRate = 22050;
	} else if (v == 3) {
		Config.SampleRate = 44100;
	} else if (v == 4) {
		Config.SampleRate = 48000;
	}
}

static void menu_midout_rate(int32_t v)
{
	Config.MIDI_outPort = v;
}

static void menu_midin_rate(int32_t v)
{
	Config.MIDI_inPort = v;
}

static void menu_vkey_size(int32_t v)
{
	Config.VkeyScale = v;
#if defined(ANDROID) || TARGET_OS_IPHONE
	Joystick_Vbtn_Update(WinUI_get_vkscale());
#endif
}

static void menu_vbtn_swap(int32_t v)
{
	Config.VbtnSwap = v;
}

static void menu_joymode_setting(int32_t v)
{
	Config.Joymode = v;
}

static void menu_hwjoy_setting(int32_t v)
{
	GamePad_Change(v);
}

static void menu_nowait(int32_t v)
{
	Config.NoWaitMode = v;
}

static void menu_joykey(int32_t v)
{
	Config.JoyKey = v;
}

static void menu_ram_size(int32_t v)
{

	if (v == 0) {
		Config.ram_size = 1;
	} else if (v == 1) {
		Config.ram_size = 2;
	} else if (v == 2) {
		Config.ram_size = 4;
	} else if (v == 3) {
		Config.ram_size = 8;
	} else if (v == 4) {
		Config.ram_size = 12;
	} else {
		Config.ram_size = 2;
	}

	/*Set SRAM RAM-Size*/
	SRAM_SetRamSize((uint8_t)(Config.ram_size<<4));

}

// 1階層上に移動するんだよ？
// ex. ./hoge/.. -> ./
// ( ./ ---down hoge dir--> ./hoge ---up hoge dir--> ./hoge/.. )
static void shortcut_dir(int32_t drv)
{
	int32_t i, len, found = 0;
	char *p;

	// len is larger than 2
	len = (int32_t)strlen(mfl.dir[drv]);
	p = mfl.dir[drv] + len - 2;
	for (i = len - 2; i >= 0; i--) {
#ifdef _WIN32
		if (*p == '\\') {
#else
		if (*p == '/') {
#endif
			found = 1;
			break;
		}
		p--;
	}

#ifdef _WIN32
	if (found && strcmp(p, "\\..\\")) {
		*(p + 1) = '\0';
	} else {
		strcat(mfl.dir[drv], "..\\");//ここに来たら要注意
	}
#else
	if (found && strcmp(p, "/../")) {
		*(p + 1) = '\0';
	} else {
		strcat(mfl.dir[drv], "../");//ここに来たら要注意
	}
#endif

}

/*=== PF12 Menu ===*/
int32_t WinUI_Menu(int32_t first)
{
	int32_t i, n;
	int32_t cursor0;
	uint8_t joy;
	int32_t menu_redraw = 0;
	int32_t pad_changed = 0;
	int32_t mfile_redraw = 0;
	char *p;
	uint32_t back_color = 0x00000000; //menuの背景色

	if (first) {
		menu_state = ms_key;
		mkey_y = 0;
		mkey_pos = 0;
		menu_redraw = 1;
		first = 0;
		//  The screen is not rewritten without any key actions,
		// so draw screen first.
		WinDraw_ClearMenuBuffer(back_color);
		WinDraw_DrawMenu(menu_state, mkey_pos, mkey_y, mval_y);
	}

	cursor0 = mkey_y;
	joy = get_joy_downstate();
	reset_joy_downstate();

	//HOMEボタンでMenuEnd
	if (!(joy & JOY_HOME)) {
			return WUM_MENU_END;
	}

	//OPM Volume setting
	char *s = menu_item_key[mkey_y];
	if ((menu_state == ms_key)&&(!strncmp("Sound", s, 5))) {
	  if(joy & JOY_RIGHT){
		if((Config.OPM_VOL>0)&&(Config.PCM_VOL>0)/*&&(Config.MCR_VOL>0)*/){
		Config.OPM_VOL --;
		Config.PCM_VOL --;
		Config.MCR_VOL --;
		menu_redraw = 1;
		}
	  }
	  if(joy & JOY_LEFT) {
		if((Config.OPM_VOL<16)&&(Config.PCM_VOL<16)/*&&(Config.MCR_VOL<16)*/){
		Config.OPM_VOL ++;
		Config.PCM_VOL ++;
		Config.MCR_VOL ++;
		menu_redraw = 1;
		}
	  }
	}

	/* JoyPad setting 
	if (menu_state == ms_hwjoy_set && sdl_joy) {
		int32_t y;
		y = mval_y[mkey_y];
		SDL_JoystickUpdate();
		if (y <= 2) { // X,Y,Z
			for (i = 0; i < SDL_JoystickNumAxes(sdl_joy); i++) {
				n = SDL_JoystickGetAxis(sdl_joy, i);
				if (n < -JOYAXISPLAY || n > JOYAXISPLAY) {
					Config.HwJoyAxis[y] = i;
					if(n>0){
						JoyDirection = 0;
					}
					else{
						JoyDirection = 1;
					}
					menu_hwjoy_print(y);//for Debug
					pad_changed = 1;
					break;
				}
			}
		} else {// A,B,X,Y,C,D,E1,E2  Button 
			for (i = 0; i < SDL_JoystickNumButtons(sdl_joy); i++) {
				if (SDL_JoystickGetButton(sdl_joy, i)) {
					Config.HwJoyBtn[y - 3] = i;
					menu_hwjoy_print(y);//for Debug
					pad_changed = 1;
					break;
				}
			}
		}
	}*/

	if (!(joy & JOY_UP)) {
		switch (menu_state) {
		case ms_key:
			if (mkey_y > 0) {
				mkey_y--;
			}
			if (mkey_pos > mkey_y) {
				mkey_pos--;
			}
			break;
		case ms_value:
			if (mval_y[mkey_y] > 0) {
				mval_y[mkey_y]--;

				// do something immediately
				if (menu_func[mkey_y].imm) {
					menu_func[mkey_y].func(mval_y[mkey_y]);
				}

				menu_redraw = 1;
			}
			break;
		case ms_file:
			if (mfl.y == 0) {
				if (mfl.ptr > 0) {
					mfl.ptr--;
				}
			} else {
				mfl.y--;
			}
			mfile_redraw = 1;
			break;
		}
	}

	if (!(joy & JOY_DOWN)) {
		switch (menu_state) {
		case ms_key:
			if (mkey_y < MENU_NUM - 1) {
				mkey_y++;
			}
			if (mkey_y > mkey_pos + MENU_WINDOW - 1) {
				mkey_pos++;
			}
			break;
		case ms_value:
			if (menu_items[mkey_y][mval_y[mkey_y] + 1][0] != '\0') {
				mval_y[mkey_y]++;

				if (menu_func[mkey_y].imm) {
					menu_func[mkey_y].func(mval_y[mkey_y]);
				}

				menu_redraw = 1;
			}
			break;
		case ms_file:
			if (mfl.y == 13) {
				if (mfl.ptr + 14 < mfl.num
				    && mfl.ptr < MFL_MAX - 13) {
					mfl.ptr++;
				}
			} else if (mfl.y + 1 < mfl.num) {
				mfl.y++;
				//printf("mfl.y %d\n", mfl.y);
			}
			mfile_redraw = 1;
			break;
		}
	}

	if (!(joy & JOY_TRGA)) {
		int32_t drv, y;
		switch (menu_state) {
		case ms_key:
			menu_state = ms_value;
			menu_redraw = 1;
			break;
		case ms_value:
			menu_func[mkey_y].func(mval_y[mkey_y]);

			if (menu_state == ms_hwjoy_set) {
				menu_redraw = 1;
				break;
			}

			// get back key_mode if value is set.
			// go file_mode if value is filename.
			menu_state = ms_key;
			menu_redraw = 1;

			drv = WinUI_get_drv_num(mkey_y);
			p6logd("**** drv:%d *****\n", drv);
			if (drv >= 0) {
				if (mval_y[mkey_y] == 0) {
					// go file_mode
					//p6logd("hoge:%d", mval_y[mkey_y]);
					menu_state = ms_file;
					menu_redraw = 0; //reset
					mfile_redraw = 1;
				} else { // mval_y[mkey_y] == 1
					// FDD_EjectFD() is done, so set 0.
					mval_y[mkey_y] = 0;
				}
			} else if (!strcmp("SYSTEM", menu_item_key[mkey_y])) {
				if (mval_y[mkey_y] == 2) {
					return WUM_EMU_QUIT;
				}
				return WUM_MENU_END;
			} else if (!strcmp("MIDI Out", menu_item_key[mkey_y])) {
				midOutChg(Config.MIDI_outPort, Config.MIDI_Bank);
			} else if (!strcmp("MIDI In ", menu_item_key[mkey_y])) {
				midInChg(Config.MIDI_inPort);
			}
			break;
		case ms_file:
			drv = WinUI_get_drv_num(mkey_y);
			//p6logd("***** drv:%d *****\n", drv);
			if (drv < 0) {
				break; 
			}
			y = mfl.ptr + mfl.y;
			// file loaded
			//p6logd("file slect %s\n", mfl.name[y]);
			if (mfl.type[y]) {
				// directory operation
				if (!strcmp(mfl.name[y], "..")) {
					shortcut_dir(drv);
				} else {
					strcat(mfl.dir[drv], mfl.name[y]);
#ifdef _WIN32
					strcat(mfl.dir[drv], "\\");
#else
					strcat(mfl.dir[drv], "/");
#endif
				}
				menu_func[mkey_y].func(0);
				mfile_redraw = 1;
			} else {
				// file operation
				if (strlen(mfl.name[y]) != 0) {
					char tmpstr[MAX_PATH];
					strcpy(tmpstr, mfl.dir[drv]);
					strcat(tmpstr, mfl.name[y]);
					if (drv < 2) {
						FDD_SetFD(drv, tmpstr, 0);
						strcpy((char *)Config.FDDImage[drv], tmpstr);
					} else {
						char strwork[MAX_PATH];
						strcpy(strwork, tmpstr);
						upper(strwork);	 /*拡張子判別用大文字変換*/
						p = strstr(strwork, ".HDS");
						if (p == NULL ) {
						  p = strstr(strwork, ".MOS");
						  if (p == NULL ) {//HDF
						   strcpy((char *)Config.HDImage[drv - 2], tmpstr);
						   SRAM_SetSASIDrive(15);//Set SRAM for SASI boot drive
						   Config.SCSIEXHDImage[drv - 2][0] = '\0';
						  }
						  else{//MOS
						   strcpy((char *)Config.SCSIEXHDImage[5], tmpstr);// SCSI-ID=5
						  }
						}
						else{//HDS
						 Config.HDImage[drv - 2][0] = '\0';
						 strcpy((char *)Config.SCSIEXHDImage[drv - 2], tmpstr);
						 SRAM_SetSCSIMode(1);//Set SRAM for ExSCSI boot!
						}
					}
				}
				menu_state = ms_key;
				menu_redraw = 1;
			}
			mfl.y = 0;
			mfl.ptr = 0;
			break;
		case ms_hwjoy_set:
			// Go back keymode
			// if TRG1 of v-pad or hw keyboard was pushed.
			if (!pad_changed) {
				menu_state = ms_key;
				menu_redraw = 1;
			}
			break;
		}
	}

	if (!(joy & JOY_TRGB)) {
		switch (menu_state) {
		case ms_key:
			return WUM_MENU_END;//GamePadでMenuEnd
			break;
		case ms_file:
			menu_state = ms_value;
			// reset position of file cursor
			mfl.y = 0;
			mfl.ptr = 0;
			menu_redraw = 1;
			break;
		case ms_value:
			menu_state = ms_key;
			menu_redraw = 1;
			break;
		case ms_hwjoy_set:
			// Go back keymode
			// if TRG2 of v-pad or hw keyboard was pushed.
			if (!pad_changed) {
				menu_state = ms_key;
				menu_redraw = 1;
			}
			break;
		}
	}

	if (pad_changed) {
		menu_redraw = 1;
	}

	if (cursor0 != mkey_y) {
		menu_redraw = 1;
	}

	if (mfile_redraw) {
		WinDraw_DrawMenufile(&mfl); /*File選択*/
		mfile_redraw = 0;
	}

	if (menu_redraw) {
		//p6logd("RedrawMenu\n");
		WinDraw_ClearMenuBuffer(back_color);
		WinDraw_DrawMenu(menu_state, mkey_pos, mkey_y, mval_y);/*Menu ReDraw*/
	}

	return 0;
}
