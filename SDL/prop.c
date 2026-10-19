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

/* -------------------------------------------------------------------------- *
 *  PROP.C - 各種設定用プロパティシートと設定値管理                           *
 * -------------------------------------------------------------------------- */

#include <sys/stat.h>
#include "common.h"
#include "winx68k.h"
#include "keyboard.h"
#include "prop.h"
#include "x68kmemory.h"

#ifndef SDL2
#include "SDL3/SDL.h"
#else
#include "SDL2/SDL.h"
#endif

char ini_title[] = "WinX68k";

static const char MIDI_TYPE_NAME[4][3] = {
	"LA", "GM", "GS", "XG"
};

Win68Conf Config;
Win68Conf ConfBk;

#ifndef MAX_BUTTON
#define MAX_BUTTON 32
#endif

extern char filepath[MAX_PATH];
extern char winx68k_ini[MAX_PATH];
extern char joyname[2][MAX_PATH];
extern char joybtnname[2][MAX_BUTTON][MAX_PATH];
extern uint8_t joybtnnum[2];

#define CFGLEN MAX_PATH

#ifdef _WIN32
char slash = '\\';
#else
char slash = '/';
#endif

static char *makeBOOL(uint8_t value) {

	if (value) {
		return("true");
	}
	return("false");
}

static int32_t Aacmp(char *cmp, char *str) {

	char	p;

	while(*str) {
		p = *cmp++;
		if (!p) {
			break;
		}
		if (p >= 'a' && p <= 'z') {
			p -= 0x20;
		}
		if (p != *str++) {
			return(-1);
		}
	}
	return(0);
}

static int32_t solveBOOL(char *str) {

	if ((!Aacmp(str, "TRUE")) || (!Aacmp(str, "ON")) ||
		(!Aacmp(str, "+")) || (!Aacmp(str, "1")) ||
		(!Aacmp(str, "ENABLE"))) {
		return(1);
	}
	return(0);
}

int32_t
set_modulepath(char *path, size_t len)
{
	struct stat sb;
	char *homepath;

#ifdef ANDROID
	const char *extpath;
	char *p, ep_buf[MAX_PATH], p6dir_buf[MAX_PATH];
	FILE *fp;

	p6dir_buf[0] = '\0';
#ifndef SDL2
 	extpath = SDL_GetAndroidExternalStoragePath();//for SDL3
#else
	extpath = SDL_AndroidGetExternalStoragePath();//for SDL2
#endif
	p6logd("extpath:%s", extpath);

	// get the Android external path (.ex /storage/emulated/0)
	strcpy(p6dir_buf, extpath);
	p = strstr(p6dir_buf, "/Android/data/com.fc2.blog45.hissorii/files");
	if (p != NULL) {
		*p = '\0';
		p6logd("extpath2:%s", p6dir_buf);

		strcat(p6dir_buf, "/px68k");
		p6logd("p6dir_buf:%s", p6dir_buf);
		// if there is the px68k directory ...
		if (stat(p6dir_buf, &sb) == 0 && S_ISDIR(sb.st_mode)) {
			goto set_dir;
		}
	}

	if (extpath != NULL) {
		// read Android/data/com.fc2.blog45.hissorii/files/dir.txt
		sprintf(ep_buf, "%s/dir.txt", extpath);
		if ((fp = fopen(ep_buf, "r")) != NULL) {
			fgets(p6dir_buf, MAX_PATH - 1, fp);
			p6logd("p6dir:%s", p6dir_buf);
			fclose(fp);
		}
	}

	// if everything failed, try /sdcard/px68k directory
	if (p6dir_buf[0] == '\0') {
		strcpy(p6dir_buf, "/sdcard/px68k");
	}

set_dir:
	strcpy(path, p6dir_buf);
	p6logd("path:%s", path);

	sprintf(winx68k_ini, "%s/config", p6dir_buf);
	p6logd("config:%s", winx68k_ini);

	return 0;
#endif
#if TARGET_OS_IPHONE && TARGET_IPHONE_SIMULATOR == 0
        puts("Iphone...");
        sprintf(path, "/var/mobile/px68k");
        sprintf(winx68k_ini, "/var/mobile/px68k/config");
        return 0;
#endif

	homepath = (char *)getenv("HOME");
	if (homepath == 0)
		homepath = (char *)".";

	snprintf((char *)path, len, "%s%c%s", homepath, slash, ".keropi");
	if (stat((char *)path, &sb) < 0) {
#ifdef _WIN32
		if (mkdir((char *)path) < 0) {
#else
		if (mkdir((char *)path, 0700) < 0) {
#endif
			perror((char *)path);
			return 1;
		}
	} else {
		//if ((sb.st_mode & S_IFDIR) == 0) {
		if(S_ISDIR(sb.st_mode)){
			fprintf(stderr, "%s is directory.\n", path);
		}
		else{
			fprintf(stderr, "%s isn't directory.\n", path);
			return 1;
		}
	}
	snprintf(winx68k_ini, sizeof(winx68k_ini), "%s%c%s", path, slash, "config");
	if (stat(winx68k_ini, &sb) >= 0) {
		//if (sb.st_mode & S_IFDIR) {
		if(S_ISDIR(sb.st_mode)){
			fprintf(stderr, "%s is directory.\n", winx68k_ini);
			return 1;
		}
	}

	return 0;
}

// Mem serch String → return:buf
uint32_t
GetPrivateProfileString(const char* title, const char* key, const char* defvalue, 
						char* buf, uint32_t len, const char* buff)
{
	if (title == NULL || key == NULL || buff == NULL){
	  strncpy(buf,defvalue,len);
	  return(uint32_t)strlen(buf);
	}

	uint32_t value;
	char keyword[100];

	strncpy(keyword, key ,sizeof(keyword));

	if(memcmp(&buff[1],title,strlen(title)) != 0){//第1チェック
	 strncpy(buf, defvalue ,len);// store default
	}

	char *result = strstr(buff,keyword);//Key一致検索
	if(result){
	  uint32_t posi = (result - buff + strlen(keyword) );
	  if(buff[posi] == (char)'=' ){
	    char *dst;
	    dst = buf;
	    int src = posi +1;
	    while(buff[src] != '\r' && buff[src] != '\n' && buff[src] != '\0'){//転送
	     *dst = buff[src];
	     dst++;
	     src++;
	    }
	    *dst = '\0';
	  }
	}
	return(uint32_t)strlen(buf);
}
// Mem serch Int.
uint32_t
GetPrivateProfileInt(const char* title, const char* key, int32_t defvalue, const char* buff)
{
	char	buf[CFGLEN];
	char	defv[20];

	snprintf(defv, sizeof(defv), "%d\n", defvalue);// default value
	uint32_t ret = GetPrivateProfileString(title,key,defv,buf,sizeof(buf),buff);//文字列で取り出す

	if(ret!=0){
	  int32_t value;
	  sscanf(buf, "%d", &value);// 数字に変換
	  return value;
	}

	return defvalue;
}

// Load config File 
// on memory serch ver.
void LoadConfig(void)
{
	int		i, j;
	char	buf[CFGLEN];
	char	*buff;
	size_t	dt;

	buff = File_Load(winx68k_ini, &dt);// 一気読み
	if(buff == NULL){
	 p6logd("Load config Error.\n");
	}

	Config.MenuLanguage = (int32_t)GetPrivateProfileInt((const char*)ini_title, "MenuLanguage", 0, buff);
	Config.DisplayNo = (int32_t)GetPrivateProfileInt((const char*)ini_title, "DisplayNo", 0, buff);
	Config.WinPosX = (int32_t)GetPrivateProfileInt((const char*)ini_title, "WinPosX", 20, buff);
	Config.WinPosY = (int32_t)GetPrivateProfileInt((const char*)ini_title, "WinPosY", 20, buff);
	Config.FrameRate = (uint8_t)GetPrivateProfileInt((const char*)ini_title, "FrameRate", 7, buff);
	if (!Config.FrameRate) Config.FrameRate = 7;
	Config.ram_size = (int32_t)(Memory_ReadB(0xed0009) & 0xf0)>>4; // from SRAM
	GetPrivateProfileString((const char*)ini_title, "StartDir", "", buf, MAX_PATH, buff);
	if (buf[0] != 0)
		strncpy(filepath, buf, sizeof(filepath));
	else
		filepath[0] = 0;

	Config.OPM_VOL = GetPrivateProfileInt((const char*)ini_title, "OPM_Volume", 12, buff);
	Config.PCM_VOL = GetPrivateProfileInt((const char*)ini_title, "PCM_Volume", 15, buff);
	Config.MCR_VOL = GetPrivateProfileInt((const char*)ini_title, "MCR_Volume", 13, buff);
	Config.SampleRate = GetPrivateProfileInt((const char*)ini_title, "SampleRate", 44100, buff);
	Config.BufferSize = GetPrivateProfileInt((const char*)ini_title, "BufferSize", 50, buff);

	Config.MouseSpeed = GetPrivateProfileInt((const char*)ini_title, "MouseSpeed", 10, buff);

	GetPrivateProfileString((const char*)ini_title, "FDDStatWin", "1", buf, sizeof(buf), buff);
	Config.WindowFDDStat = solveBOOL(buf);
	GetPrivateProfileString((const char*)ini_title, "FDDStatFullScr", "1", buf, sizeof(buf), buff);
	Config.FullScrFDDStat = solveBOOL(buf);

	GetPrivateProfileString((const char*)ini_title, "DSAlert", "1", buf, sizeof(buf), buff);
	Config.DSAlert = solveBOOL(buf);
	GetPrivateProfileString((const char*)ini_title, "SoundLPF", "1", buf, sizeof(buf), buff);
	Config.Sound_LPF = solveBOOL(buf);
	Config.CPU_Emu = GetPrivateProfileInt((const char*)ini_title, "M68000_Emu", 0, buff);
	GetPrivateProfileString((const char*)ini_title, "MIDI_SW", "1", buf, sizeof(buf), buff);
	Config.MIDI_SW = solveBOOL(buf);
	GetPrivateProfileString((const char*)ini_title, "MIDI_Reset", "0", buf, sizeof(buf), buff);
	Config.MIDI_Reset = solveBOOL(buf);
	Config.MIDI_Type = GetPrivateProfileInt((const char*)ini_title, "MIDI_Type", 1, buff);
	Config.MIDI_Bank = GetPrivateProfileInt((const char*)ini_title, "MIDI_Bank", 0, buff);
	GetPrivateProfileString((const char*)ini_title, "JoySwap", "0", buf, sizeof(buf), buff);
	Config.JoySwap = solveBOOL(buf);

	GetPrivateProfileString((const char*)ini_title, "JoyKey", "0", buf, sizeof(buf), buff);
	Config.JoyKey = solveBOOL(buf);
	GetPrivateProfileString((const char*)ini_title, "JoyKeyReverse", "0", buf, sizeof(buf), buff);
	Config.JoyKeyReverse = solveBOOL(buf);
	GetPrivateProfileString((const char*)ini_title, "JoyKeyJoy2", "0", buf, sizeof(buf), buff);
	Config.JoyKeyJoy2 = solveBOOL(buf);
	GetPrivateProfileString((const char*)ini_title, "SRAMBootWarning", "1", buf, sizeof(buf), buff);
	Config.SRAMWarning = solveBOOL(buf);

	GetPrivateProfileString((const char*)ini_title, "WinDrvLFN", "1", buf, sizeof(buf), buff);
	Config.LongFileName = solveBOOL(buf);
	GetPrivateProfileString((const char*)ini_title, "WinDrvFDD", "1", buf, sizeof(buf), buff);
	Config.WinDrvFD = solveBOOL(buf);

	Config.WinStrech = GetPrivateProfileInt((const char*)ini_title, "WinStretch", 1, buff);

	GetPrivateProfileString((const char*)ini_title, "DSMixing", "0", buf, sizeof(buf), buff);
	Config.DSMixing = solveBOOL(buf);

	Config.XVIMode = (uint8_t)GetPrivateProfileInt((const char*)ini_title, "XVIMode", 0, buff);

	GetPrivateProfileString((const char*)ini_title, "CDROM_ASPI", "1", buf, sizeof(buf), buff);
	Config.CDROM_ASPI = solveBOOL(buf);
	Config.CDROM_SCSIID = (uint8_t)GetPrivateProfileInt((const char*)ini_title, "CDROM_SCSIID", 6, buff);
	Config.CDROM_ASPI_Drive = (uint8_t)GetPrivateProfileInt((const char*)ini_title, "CDROM_ASPIDrv", 0, buff);
	Config.CDROM_IOCTRL_Drive = (uint8_t)GetPrivateProfileInt((const char*)ini_title, "CDROM_CTRLDrv", 16, buff);
	GetPrivateProfileString((const char*)ini_title, "CDROM_Enable", "1", buf, sizeof(buf), buff);
	Config.CDROM_Enable = solveBOOL(buf);

	GetPrivateProfileString((const char*)ini_title, "SSTP_Enable", "0", buf, sizeof(buf), buff);
	Config.SSTP_Enable = solveBOOL(buf);
	Config.SSTP_Port = GetPrivateProfileInt((const char*)ini_title, "SSTP_Port", 11000, buff);

	GetPrivateProfileString((const char*)ini_title, "ToneMapping", "0", buf, sizeof(buf), buff);
	Config.ToneMap = solveBOOL(buf);
	GetPrivateProfileString((const char*)ini_title, "ToneMapFile", "", buf, MAX_PATH, buff);
	if (buf[0] != 0)
		strcpy((char *)Config.ToneMapFile, buf);
	else
		Config.ToneMapFile[0] = 0;

	GetPrivateProfileString((const char*)ini_title, "SoundFontFile", "", buf, MAX_PATH, buff);
	if (buf[0] != 0)
		strcpy((char *)Config.SoundFontFile, buf);
	else
		Config.SoundFontFile[0] = 0;

	Config.MIDIDelay = GetPrivateProfileInt((const char*)ini_title, "MIDIDelay", Config.BufferSize*5, buff);
	Config.MIDIAutoDelay = GetPrivateProfileInt((const char*)ini_title, "MIDIAutoDelay", 1, buff);

	Config.VkeyScale = GetPrivateProfileInt((const char*)ini_title, "VkeyScale", 4, buff);

	Config.VbtnSwap = GetPrivateProfileInt((const char*)ini_title, "VbtnSwap", 0, buff);

	Config.JoyOrMouse = GetPrivateProfileInt((const char*)ini_title, "JoyOrMouse", 0, buff);

	Config.Joymode = GetPrivateProfileInt((const char*)ini_title, "JoyMode", 0, buff);

	Config.HwJoyAxis[0] = GetPrivateProfileInt((const char*)ini_title, "HwJoyAxis0", 0, buff);

	Config.HwJoyAxis[1] = GetPrivateProfileInt((const char*)ini_title, "HwJoyAxis1", 1, buff);

	Config.HwJoyAxis[2] = GetPrivateProfileInt((const char*)ini_title, "HwJoyAxis2", 2, buff);

	Config.HwJoyHat = GetPrivateProfileInt((const char*)ini_title, "HwJoyHat", 0, buff);

	for (i = 0; i < 8; i++) {
		sprintf(buf, "HwJoyBtn%d", i);
		Config.HwJoyBtn[i] = GetPrivateProfileInt((const char*)ini_title, buf, i, buff);
	}

	Config.NoWaitMode = GetPrivateProfileInt((const char*)ini_title, "NoWaitMode", 0, buff);

	for (i=0; i<2; i++)
	{
		for (j=0; j<8; j++)
		{
			sprintf(buf, "Joy%dButton%d", i+1, j+1);
			Config.JOY_BTN[i][j] = GetPrivateProfileInt((const char*)ini_title, buf, j, buff);
		}
	}

	for (i = 0; i < 2; i++) {
		sprintf(buf, "FDD%d", i);
		GetPrivateProfileString((const char*)ini_title, buf, "", (char *)Config.FDDImage[i], MAX_PATH, buff);
	}

	for (i=0; i<16; i++)
	{
		sprintf(buf, "HDD%d", i);
		GetPrivateProfileString((const char*)ini_title, buf, "", (char *)Config.HDImage[i], MAX_PATH, buff);
	}
	for (i=0; i<8; i++)
	{
		sprintf(buf, "SCSIEXHDD%02d", i);
		GetPrivateProfileString((const char*)ini_title, buf, "", (char *)Config.SCSIEXHDImage[i], MAX_PATH, buff);
	}

	if(buff != NULL){
	  File_Free(buff);// メモリ解放
	}

  return;
}

// Save config File 
// Store mem to File
void SaveConfig(void)
{
	int		i, j;
	char	buf[CFGLEN], buf2[CFGLEN];

	char config[2048];//MAXこのくらい？

	strcpy(config, "[");
	strcat(config, ini_title);
	strcat(config, "]\n");

	snprintf(buf, sizeof(buf), "%s=%d\n", "MenuLanguage", Config.MenuLanguage);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "DisplayNo", Config.DisplayNo);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "WinPosX", Config.WinPosX);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "WinPosY", Config.WinPosY);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "FrameRate", Config.FrameRate);
	strcat(config,buf);
	//snprintf(buf, sizeof(buf), "%s=%d\n", "RAM(MB)", Config.ram_size);  SRAMに保存してあるのでconfigには保存しない
	//strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "StartDir", filepath );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "OPM_Volume", Config.OPM_VOL);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "PCM_Volume", Config.PCM_VOL);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "MCR_Volume", Config.MCR_VOL);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "SampleRate", Config.SampleRate);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "BufferSize", Config.BufferSize);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "MouseSpeed", Config.MouseSpeed);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "FDDStatWin",makeBOOL((uint8_t)Config.WindowFDDStat) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "FDDStatFullScr", makeBOOL((uint8_t)Config.FullScrFDDStat) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "DSAlert", makeBOOL((uint8_t)Config.DSAlert) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "SoundLPF", makeBOOL((uint8_t)Config.Sound_LPF) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "M68000_Emu", Config.CPU_Emu);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "MIDI_SW", makeBOOL((uint8_t)Config.MIDI_SW) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "MIDI_Reset", makeBOOL((uint8_t)Config.MIDI_Reset) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "MIDI_Type", Config.MIDI_Type);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "JoySwap", makeBOOL((uint8_t)Config.JoySwap) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "JoyKey", makeBOOL((uint8_t)Config.JoyKey) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "JoyKeyReverse", makeBOOL((uint8_t)Config.JoyKeyReverse) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "JoyKeyJoy2", makeBOOL((uint8_t)Config.JoyKeyJoy2) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "SRAMBootWarning", makeBOOL((uint8_t)Config.SRAMWarning) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "WinDrvLFN", makeBOOL((uint8_t)Config.LongFileName) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "WinDrvFDD", makeBOOL((uint8_t)Config.WinDrvFD) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "WinStretch", Config.WinStrech);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "DSMixing", makeBOOL((uint8_t)Config.DSMixing) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "XVIMode", Config.XVIMode);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "CDROM_ASPI", makeBOOL((uint8_t)Config.CDROM_ASPI) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "CDROM_SCSIID", Config.CDROM_SCSIID);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "CDROM_ASPIDrv", Config.CDROM_ASPI_Drive);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "CDROM_CTRLDrv", Config.CDROM_IOCTRL_Drive);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "CDROM_Enable", makeBOOL((uint8_t)Config.CDROM_Enable) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "SSTP_Enable", makeBOOL((uint8_t)Config.SSTP_Enable) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "SSTP_Port", Config.SSTP_Port);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "ToneMapping", makeBOOL((uint8_t)Config.ToneMap) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "ToneMapFile", (char *)Config.ToneMapFile );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "SoundFontFile", (char *)Config.SoundFontFile );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "MIDIDelay", Config.MIDIDelay );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%s\n", "MIDIAutoDelay", makeBOOL((uint8_t)Config.MIDIAutoDelay) );
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "VkeyScale", Config.VkeyScale);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "VbtnSwap", Config.VbtnSwap);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "JoyOrMouse", Config.JoyOrMouse);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "HwJoyAxis0", Config.HwJoyAxis[0]);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "HwJoyAxis1", Config.HwJoyAxis[1]);
	strcat(config,buf);
	snprintf(buf, sizeof(buf), "%s=%d\n", "HwJoyHat", Config.HwJoyHat);
	strcat(config,buf);

	for (i = 0; i < 8; i++) {
		snprintf(buf, sizeof(buf), "%s%d=%d\n","HwJoyBtn",i , Config.HwJoyBtn[i] );
		strcat(config,buf);
	}

	snprintf(buf, sizeof(buf), "%s=%d\n", "NoWaitMode", Config.NoWaitMode);
	strcat(config,buf);

	for (i=0; i<2; i++)
	{
		for (j=0; j<8; j++)
		{
			snprintf(buf, sizeof(buf), "%s%d%s%d=%d\n","Joy",i+1,"Button", j+1, Config.JOY_BTN[i][j] );
			strcat(config,buf);
		}
	}

	for (i = 0; i < 2; i++)
	{
		snprintf(buf, sizeof(buf), "%s%d=%s\n", "FDD", i, (char *)Config.FDDImage[i] );
		strcat(config,buf);
	}

	for (i=0; i<16; i++)
	{
		snprintf(buf, sizeof(buf), "%s%d=%s\n","HDD", i, (char *)Config.HDImage[i] );
		strcat(config,buf);
	}
	for (i=0; i<8; i++)
	{
		snprintf(buf, sizeof(buf), "%s%02d=%s\n", "SCSIEXHDD", i, (char *)Config.SCSIEXHDImage[i] );
		strcat(config,buf);
	}

	//一括Write
	if(TRUE != File_Save(winx68k_ini, config, strlen(config))){
		printf("Config Save Error \n");
	}

  return;
}
