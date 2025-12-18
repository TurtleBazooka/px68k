#ifdef  __cplusplus
extern "C" {
#endif 

#include <sys/stat.h>
#include <errno.h>

#include "SDL3/SDL.h"
#include "common.h"
#include "dosio.h"
#include "timer.h"
#include "keyboard.h"
#include "prop.h"
#include "status.h"
#include "GamePad.h"
#include "mkcgrom.h"
#include "winx68k.h"
#include "windraw.h"
#include "winui.h"
//#include "../x68k/m68000.h" // xxx これはいずれいらなくなるはず
#include "../m68000/m68000.h"
//#include "../m68000/c68k/c68k.h"
#include "../x68k/x68kmemory.h"
#include "mfp.h"
#include "bg.h"
#include "adpcm.h"
#include "mercury.h"
#include "crtc.h"
#include "fdc.h"
#include "fdd.h"
#include "dmac.h"
#include "irqh.h"
#include "ioc.h"
#include "rtc.h"
#include "sasi.h"
#include "scsi.h"
#include "sysport.h"
#include "bg.h"
#include "palette.h"
#include "crtc.h"
#include "ppi.h"
#include "scc.h"
#include "midi.h"
#include "sram.h"
#include "gvram.h"
#include "tvram.h"
#include "mouse.h"

#include "SoundCtrl.h"
#ifdef	YMFM
#include "ymfm_wrap.h"
#else
#include "fmg_wrap.h"
#endif


#ifdef USE_OGLES20
SDL_DisplayMode sdl_dispmode;
#endif

#ifdef RFMDRV
int32_t rfd_sock;
#endif



//#include "../icons/keropi_mono.xbm"

#define	APPNAME	"Keropi"

extern	uint16_t	BG_CHREND;
extern	uint16_t	BG_BGTOP;
extern	uint16_t	BG_BGEND;
extern	uint8_t		BG_CHRSIZE;

extern int32_t ICount;
extern void m68000_init(void);

#if defined(ANDROID) || TARGET_OS_IPHONE
extern SDL_TouchID touchId;
#endif

const	char	PrgName[] = "Keropi";
const	char	PrgTitle[] = APPNAME;

char	winx68k_dir[MAX_PATH];
char	winx68k_ini[MAX_PATH];

// *.HDS、*.HDFはHDD、他はFDD MOSはMO
char fdimg[] = "D8888DHDMDUP2HDDIMXDFIMG";
char saimg[] = "HDF";
char scimg[] = "HDS";
char moimg[] = "MOS";

int32_t		HLINE_TOTAL = 1026;
int32_t		VLINE_TOTAL = 567;
int32_t		VLINE = 0;
int32_t		vline = 0;

extern	int32_t	SplashFlag;
struct chdata {
    uint8_t *addr;
    uint32_t num;
};

char window_title[100];

uint8_t DispFrame = 0;
uint32_t SoundSampleRate;

uint32_t hTimerID = 0;
uint32_t TimerICount = 0;
extern uint32_t timertick;
uint8_t traceflag = 0;

uint8_t ForceDebugMode = 0;
//uint32_t skippedframes = 0;

static int32_t ClkUsed = 0;
static int32_t FrameSkipCount = 0;
static int32_t FrameSkipQueue = 0;

#ifdef __cplusplus
};
#endif


/*SDL3関係の宣言*/
SDL_Window *sdl_window;   /*SDL screen*/
SDL_Renderer *sdl_render; /*SDL GPU frame buf*/
SDL_Texture *sdl_texture; /*SDL GPU transfer buf*/
SDL_Surface *sdl_x68screen; /*X68K drawing buf*/
int32_t realdisp_w, realdisp_h; /*SDLGetScreenSize*/
extern SDL_Surface *menu_surface; /*for Menu Drawing*/
extern SDL_Window *sft_kbd_window;// softkeyboard

void
WinX68k_SCSICheck()
{
// オリジナルのCZ6BS1のSCSI ROMの代替
// 動作：SCSI IOCSが呼ばれると、SCSI IOCS番号を $e9f800 に出力します。
// SCSIデバイスからの起動は不可、初期化ルーチンはSCSI IOCS($F5)のベクタ設定のみを行います。
	static	const uint8_t	EX_SCSIIMG[] = {
		0x00, 0xea, 0x00, 0x34,			// $ea0020 SCSI起動用のエントリアドレス
		0x00, 0xea, 0x00, 0x52,			// $ea0024 Human-Loader(必ず"Human68k"の8バイト前)
		0x00, 0xea, 0x00, 0x4a,			// $ea0028 SCSI IOCSエントリアドレス
		0x48, 0x75, 0x6d, 0x61,0x6e, 0x36, 0x38, 0x6b,	// $ea002c ↓"Human68k"	(必ず起動エントリポイントの直前)
		0x13, 0xc1, 0x00, 0xea, 0x02, 0x00,	// $ea0034 "move.b d1, $ea0200"	 (SCSI 起動エントリポイント)
		0x0c, 0x11, 0x00, 0x60,				// $ea003a "cmp.b #$60,(a1)"
		0x66, 0x02,							// $ea003e "bne $(pc+2)"
		0x4e, 0x91,							// $ea0040 "jsr(a1)"
		0x4e, 0x75,							// $ea0042 "rts"
		0x53, 0x43, 0x53, 0x49, 0x45, 0x58,	// $ea0044 ID "SCSIEX"		(SCSIカードのID)
		0x13, 0xc1, 0x00, 0xea, 0x01, 0x00,	// $ea004a "move.b d1, $ea0100"	(SCSI IOCSコールエントリポイント)
		0x4e, 0x75,							// $ea0050 "rts"
		0x13, 0xc1, 0x00, 0xea, 0x03, 0x00,	// $ea0052 "move.b d1, $ea0300"	(Human 起動エントリポイント)
		0x4e, 0x75,							// $ea0058 "rts"
	};

// XVI/Compact/030の内蔵SCSI ROM
	static const uint8_t IN_SCSIIMG[] = {
		0x00, 0xfc, 0x00, 0x14,			// $fc0000 SCSI起動用のエントリアドレス
		0x00, 0xfc, 0x00, 0x32,			// $fc0004 Human-Loader(必ず"Human68k"の8バイト前)
		0x00, 0xfc, 0x00, 0x2a,			// $fc0008 SCSI IOCSエントリアドレス
		0x48, 0x75, 0x6d, 0x61,0x6e, 0x36, 0x38, 0x6b,	// $fc000c ↓"Human68k"	(必ず起動エントリポイントの直前)
		0x13, 0xc1, 0x00, 0xea, 0x02, 0x00,	// $fc0014 "move.b d1, $ea0200"	 (SCSI 起動エントリポイント)
		0x0c, 0x11, 0x00, 0x60,				// $fc001a "cmp.b #$60,(a1)"
		0x66, 0x02,							// $fc001e "bne $(pc+2)"
		0x4e, 0x91,							// $fc0020 "jsr(a1)"
		0x4e, 0x75,							// $fc0022 "rts"
//		0x53, 0x43, 0x53, 0x49, 0x49, 0x4e,	// $fc0024 ID "SCSIIN"		(SCSIカードのID)
// 内蔵SCSIをONにすると、SASIは自動的にOFFになっちゃうらしい…
// よって、IDはマッチしないようにしておく…
		0x44, 0x55, 0x4d, 0x4d, 0x59, 0x20,	// $fc0024 ID "DUMMY "
		0x13, 0xc1, 0x00, 0xea, 0x01, 0x00,	// $fc002a "move.b d1, $ea0100"	(SCSI IOCSコールエントリポイント)
		0x4e, 0x75,							// $fc0030 "rts"
		0x13, 0xc1, 0x00, 0xea, 0x03, 0x00,	// $fc0032 "move.b d1, $ea0300"	(Human 起動エントリポイント)
		0x4e, 0x75,							// $fc0038 "rts"
	};

	static const uint8_t EX_SCSIIOCS[] = {
			0x13, 0xc1, 0x00, 0xea, 0x01, 0x00,	// $ "move.b d1, $ea0100"	(SCSI IOCSコールエントリポイント)
			0x4e, 0x75,	0x00					// $ "rts"
	};

	uint16_t *p1, *p2;
	int32_t scsi;
	int32_t i;
	uint16_t tmp;
	FILEH fp;
	static const char CZ6BS1IPLFILE[] = "scsiexrom.dat";
	static const char SCSIINIPLFILE[] = "scsiinrom.dat";


	/*check IPL scsi=0:ExSCSI 1:InSCSI*/
	scsi = 0;
	for (i = 0x30600; i < 0x30c00; i += 2) {
		p1 = (uint16_t *)(&IPL[i]);
		p2 = p1 + 1;
		// xxx: works only for little endian guys
		if (*p1 == 0xfc00 && *p2 == 0x0000) {
			scsi = 1;
			break;
		}
	}

	// InSCSI(XVI/Compact/030) Append SCSI-IPL
	if (scsi) {
		fp = File_OpenCurDir((char *)SCSIINIPLFILE);/*InSCSI-IPL*/
		if (fp == 0) {
			//p6logd("NO-SCSI-IPL for built-in.\n"); // No InSCSI-IPL
			memset(IPL, 0, 0x10000);		// clear
			memcpy(IPL, IN_SCSIIMG, sizeof(IN_SCSIIMG));	// Dummy-SCSI BIOS Load
		}
		else{
			strcat(window_title," SCSIin");
			//p6logd("SCSI-IPL for built-in.\n"); // Yes InSCSI-IPL
			File_Read(fp, IPL, 0x02000);/*0xfc0000~8KB*/
			File_Close(fp);
			memcpy( &IPL[0x00041A], EX_SCSIIOCS, sizeof(EX_SCSIIOCS));//IOCS Patch
		}
	}
	else{
		//Memory_SetSCSIMode();
	}

	// ExSCSI(origin X68000) CZ-6BS1を常に有効にする
	fp = File_OpenCurDir((char *)CZ6BS1IPLFILE);/*ExSCSI-IPL*/
	if (fp == 0) {
		//p6logd("NO-SCSI-IPL for CZ-6BS1.\n");// No CZ-6BS1-IPL
		memset(SCSIIPL, 0, 0x02000);		// clear
		//memcpy(&SCSIIPL[0x20], EX_SCSIIMG, sizeof(EX_SCSIIMG));	// Dummy-SCSI BIOS Load(FDD起動を阻害するので注意！)
	}
	else{
		//p6logd("SCSI-IPL for CZ-6BS1.\n");// Yes CZ-6BS1-IPL
		File_Read(fp, &SCSIIPL[0x20], 0x01FD0);/*0xea0000~8KB*/
		File_Close(fp);
		/*ea0044からSCSIEXが格納されてることをIPLがチェックしている*/
		if(memcmp(&SCSIIPL[0x000044],"SCSIEX",6) != 0){// dump from 0xea0020
		  if(memcmp(&SCSIIPL[0x000064],"SCSIEX",6) == 0){ //dump from 0xea0000
		    memcpy(&SCSIIPL[0x000020],&SCSIIPL[0x000040], 0x02000-0x40);
		  }
		  else{
		   p6logd("SCSI-IPL not found.\n");
		  }
		}

		/* IOCS patch */
		for(i=0x20; i<0x8f; i++){
		 if(memcmp(&SCSIIPL[i],"NuSCSI",6) == 0){
		   uint32_t IOCS_adr = SCSIIPL[i+10]<<24 | SCSIIPL[i+11]<<16 | SCSIIPL[i+12]<<8 | SCSIIPL[i+13];
		   //memset(&SCSIIPL[IOCS_adr - 0xea0000], 0, 0x2000-(IOCS_adr - 0xea0000));
		   memcpy( &SCSIIPL[IOCS_adr - 0xea0000], EX_SCSIIOCS, sizeof(EX_SCSIIOCS));//IOCS Patch
		   strcat(window_title," SCSIex");
		   break;
		 }
		}
	}

	// for little endian 
#ifndef __BIG_ENDIAN__
	for (i = 0; i < 0x02000; i += 2) {
	 tmp = *(uint16_t *)&SCSIIPL[i];
	 *(uint16_t *)&SCSIIPL[i] = ((tmp >> 8)&0x00ff) | ((tmp << 8)&0xff00);
	}
#endif

 return;
}

/* Load IPL,cg-rom and SCSI-IPL */
int32_t
WinX68k_LoadROMs(void)
{
	static const char *BIOSFILE[] = {
		"iplrom.dat", "iplromxv.dat", "iplromco.dat", "iplrom30.dat"
	};
	static const char *FONTFILE[] = {
		"cgrom30.dat", "cgrom30.tmp", "cgrom.dat", "cgrom.tmp"
	};
	FILEH fp;
	int32_t i,x68030=0;
	uint16_t tmp;
	int32_t flg = TRUE;

	for (fp = 0, i = 0; fp == 0 && i < NELEMENTS(BIOSFILE); i++) {
		fp = File_OpenCurDir((char *)BIOSFILE[i]);
	}
	if (fp == 0) {
		Error("BIOS ROM イメージが見つかりません.");
		memset(IPL, 0x00, 0x40000);/*IPL clear*/
		strcat(window_title," NO-IPL");
		flg = FALSE;
	}
	else {
	File_Read(fp, &IPL[0x20000], 0x20000);/*128K*/
	File_Close(fp);
	if(i==1) strcat(window_title," EXPERT");//ver 1.0
	if(i==2) strcat(window_title," XVI");   //ver 1.1
	if(i==3) strcat(window_title," XVIcpt");//ver 1.2
	if(i==4){strcat(window_title," X68030");//ver 1.3
	  x68030 = 1;
	}
	WinX68k_SCSICheck();	// Load SCSI IPL in:$fc0000～ ex:ea0000
	}

// for little endian 
#ifndef __BIG_ENDIAN__
	for (i = 0; i < 0x40000; i += 2) {
	  tmp = *(uint16_t *)&IPL[i];
	  *(uint16_t *)&IPL[i] = ((tmp >> 8) & 0x00ff) | ((tmp << 8) & 0xff00);
	}
#endif

	for (fp = 0, i = 0; fp == 0 && i < NELEMENTS(FONTFILE); i++) {
		fp = File_OpenCurDir((char *)FONTFILE[i]);
	}
	if (fp == 0) {
		//if (make_cgromdat(FONT, NULL, NULL, x68030 )==0){/* 暫定フォント生成 */
		 memset(IPL, 0, 0x40000);/*IPL clear*/
		 flg = FALSE;
		//}
		Error("フォントROMイメージが見つかりません.\n");
		strcat(window_title," NO-FONT");
	}
	else{
	 File_Read(fp, FONT, 0xc0000);
	 File_Close(fp);
	}

// for little endian 
#ifndef __BIG_ENDIAN__
	for (i = 0; i < 0xc0000; i += 2) {
	  tmp = *(uint16_t *)&FONT[i];
	  *(uint16_t *)&FONT[i] = ((tmp >> 8) & 0x00ff) | ((tmp << 8) & 0xff00);
	}
#endif

	SDL_SetWindowTitle(sdl_window, window_title); /*SDL2/3*/

	return flg;
}

/*==  CPU-Reset ==*/
int32_t
WinX68k_Reset(void)
{
	OPM_Reset();

	m68000_reset();
	m68000_set_reg(M68K_A7,Memory_ReadD(0xff0000));
	m68000_set_reg(M68K_PC,Memory_ReadD(0xff0004));

	Memory_Init();
	CRTC_Init();
	DMA_Init();
	MFP_Init();
	FDC_Init();
	FDD_Reset();
	SASI_Init();
	SCSI_Init();
	IOC_Init();
	SCC_Init();
	Keyboard_Init();
	PPI_Init();
	//GamePad_Init();
	RTC_Init();
	TVRAM_Init();
	GVRAM_Init();
	BG_Init();
	Pal_Init();
	IRQH_Init();
	MIDI_Init();

	ICount = 0;
	ScreenClearFlg = 1;  /* clear screen く*/

	DSound_Pause();
	SRAM_VirusCheck();
	DSound_Resume();

	return TRUE;
}


int32_t
WinX68k_Init(void)
{

	IPL = (uint8_t*)malloc(0x40000 + 100);
	MEM = (uint8_t*)malloc(0xc00000 + 100);
	FONT = (uint8_t*)malloc(0xc0000 + 100);

	if (MEM){
		memset(MEM, 0, 0xc00000);
	}

	if (MEM && FONT && IPL) {
		m68000_init();
		return TRUE;
	}

	return FALSE;
}

void
WinX68k_Cleanup(void)
{

	if (IPL) {
		free(IPL);
		IPL = 0;
	}
	if (MEM) {
		free(MEM);
		MEM = 0;
	}
	if (FONT) {
		free(FONT);
		FONT = 0;
	}
}

#define CLOCK_SLICE 200
// ----------------------------------------------------
//  コアのめいんるーぷ
// ----------------------------------------------------
void WinX68k_Exec(void)
{
	//char *test = NULL;
	int32_t clk_total, clkdiv, usedclk, hsync, clk_next, clk_count, clk_line=0;
	int32_t KeyIntCnt = 0, MouseIntCnt = 0;
	uint32_t t_start = timeGetTime(), t_end;

	if ( Config.FrameRate != 7 ) {
		DispFrame = (DispFrame+1)%Config.FrameRate;
	} else {				// Auto Frame Skip
		if ( FrameSkipQueue ) {
			if ( FrameSkipCount>15 ) {
				FrameSkipCount = 0;
				FrameSkipQueue++;
				DispFrame = 0;
			} else {
				FrameSkipCount++;
				FrameSkipQueue--;
				DispFrame = 1;
			}
		} else {
			FrameSkipCount = 0;
			DispFrame = 0;
		}
	}

	vline = 0;
	clk_count = -ICount;
	clk_total = (CRTC_Regs[0x29] & 0x10) ? VSYNC_HIGH : VSYNC_NORM;
	if (Config.XVIMode == 1) {
		clk_total = (clk_total*16)/10; // ===16MHz (XVI)
		clkdiv = 16;
	} else if (Config.XVIMode == 2) {
		clk_total = (clk_total*24)/10; // ===24MHz (RedZONE)
		clkdiv = 24;
	} else {
		clkdiv = 10;  // ===10MHz (EXPERT)
	}
	ICount += clk_total;
	clk_next = (clk_total/VLINE_TOTAL);
	hsync = 1;

	do {
		int32_t m, n = (ICount>CLOCK_SLICE)?CLOCK_SLICE:ICount;
		//C68K.ICount = m68000_ICountBk = 0;			// 割り込み発生前に与えておかないとダメ（CARAT）

		if ( hsync ) {
			hsync = 0;
			clk_line = 0;
			MFP_Int(0);
			if ( (vline>=CRTC_VSTART)&&(vline<CRTC_VEND) )
				VLINE = ((vline-CRTC_VSTART)*CRTC_VStep)/2;
			else
				VLINE = -1; /* error */
			if ( (!(MFP[MFP_AER]&0x40))&&(vline==CRTC_IntLine) )
				MFP_Int(1);
			if ( MFP[MFP_AER]&0x10 ) {
				if ( vline==CRTC_VSTART )
					MFP_Int(9);
			} else {
				if ( CRTC_VEND>=VLINE_TOTAL ) {
					if ( (int32_t)vline==(CRTC_VEND-VLINE_TOTAL) ) MFP_Int(9);		// エキサイティングアワーとか（TOTAL<VEND）
				} else {
					if ( (int32_t)vline==(VLINE_TOTAL-1) ) MFP_Int(9);			// クレイジークライマーはコレでないとダメ？
				}
			}
		}

		{
			//C68K.ICount = n;
			//C68k_Exec(&C68K, C68K.ICount);
			m68000_execute(n);
			m = (n-m68000_ICountBk);			// 経過クロック数
			ClkUsed += m*10;
			usedclk = ClkUsed/clkdiv;
			clk_line += usedclk;
			ClkUsed -= usedclk*clkdiv;
			ICount -= m;
			clk_count += m;
		}

		MFP_Timer(usedclk);
		RTC_Timer(usedclk);
		DMA_Exec(0);
		DMA_Exec(1);
		DMA_Exec(2);

		if ( clk_count>=clk_next ) {
			//OPM_RomeoOut(Config.BufferSize*5);
			MIDI_DelayOut((Config.MIDIAutoDelay)?(Config.BufferSize*5):Config.MIDIDelay);
			MFP_TimerA();
			if ( (MFP[MFP_AER]&0x40)&&(vline==CRTC_IntLine) )
				MFP_Int(1);
			if ( (!DispFrame)&&(vline>=CRTC_VSTART)&&(vline<CRTC_VEND) ) {
				if ( CRTC_VStep==1 ) {				// HighReso 256dot（2度読み）
					if ( vline%2 )
						WinDraw_DrawLine();
				} else if ( CRTC_VStep==4 ) {		// LowReso 512dot
					WinDraw_DrawLine();				// 1走査線で2回描く（インターレース）
					VLINE++;
					WinDraw_DrawLine();
				} else {							// High 512dot / Low 256dot
					WinDraw_DrawLine();
				}
			}

			ADPCM_PreUpdate(clk_line);
			OPM_Timer(clk_line);
			MIDI_Timer(clk_line);
#ifndef	NO_MERCURY
			Mcry_PreUpdate(clk_line);
#endif

			KeyIntCnt++;
			if ( KeyIntCnt>(VLINE_TOTAL/4) ) {
				KeyIntCnt = 0;
				Keyboard_Int();
			}
			MouseIntCnt++;
			if ( MouseIntCnt>(VLINE_TOTAL/8) ) {
				MouseIntCnt = 0;
				SCC_IntCheck();
			}
			//DSound_Send0(clk_line);

			vline++;
			clk_next  = (clk_total*(vline+1))/VLINE_TOTAL;
			hsync = 1;
		}
	} while ( vline<VLINE_TOTAL );

	if ( CRTC_Mode&2 ) {		// FastClrビットの調整（PITAPAT）
		if ( CRTC_FastClr ) {	// FastClr=1 且つ CRTC_Mode&2 なら 終了
			CRTC_FastClr--;
			if ( !CRTC_FastClr )
				CRTC_Mode &= 0xfd;
		} else {				// FastClr開始
			if ( CRTC_Regs[0x29]&0x10 )
				CRTC_FastClr = 1;
			else
				CRTC_FastClr = 2;
			TVRAM_SetAllDirty();
			GVRAM_FastClear();
		}
	}


	FDD_SetFDInt();
	if ( !DispFrame )
		WinDraw_Draw();
	TimerICount += clk_total;

	t_end = timeGetTime();
	if ( (int32_t)(t_end-t_start)>((CRTC_Regs[0x29]&0x10)?14:16) ) {
		FrameSkipQueue += ((t_end-t_start)/((CRTC_Regs[0x29]&0x10)?14:16))+1;
		if ( FrameSkipQueue>100 )
			FrameSkipQueue = 100;
	}
}

/*command lineから実行 (引数あり)*/
void
get_cmd_line(int32_t argc, char *argv[])
{
 char strwork[20];
 char *p;
 uint32_t i,len,f1=0,h1=0,s1=0;

	for(i=1; i<argc; i++){
	  if(argv[i][0]=='-' && argv[i][1]=='h'){
	   p6logd("for useage.\n$ px68k.sdl2 hoge0.xdf hoge1.xdf scsi0.hds\n");
	  }
	  else{
	    len = (uint32_t)strlen(argv[i]);
	    strcpy(strwork, argv[i] + len - 3);//拡張子GET
	    for(len=0; len<3; len++){
	     if (strwork[len] >= 'a' && strwork[len] <= 'z') {//大文字に変換
	      strwork[len] = 'A' + strwork[len] - 'a';
	     }
	    }
	    p = strstr(fdimg, strwork);// FDD image check
	    if(p != NULL){
	      strcpy((char *)Config.FDDImage[f1], argv[i]);
	      if(f1 < 1){f1++;}
	    }
	    p = strstr(saimg, strwork);// SASI image check
	    if(p != NULL){
	      strcpy((char *)Config.HDImage[h1], argv[i]);
	      if(h1 < 15){h1++;}
	    }
	    p = strstr(scimg, strwork);// SCSI image check
	    if(p != NULL){
	      strcpy((char *)Config.SCSIEXHDImage[s1], argv[i]);
	      s1++;
	      if(s1 < 6){s1++;}
	    }
	    p = strstr(moimg, strwork);// MO image check
	    if(p != NULL){
	      strcpy((char *)Config.SCSIEXHDImage[s1], argv[i]);
	      s1++;
	      if(s1 < 6){s1++;}
	    }
	  }
	}

 return;
}

	int32_t sdlaudio = -1;
	enum {menu_out, menu_enter, menu_in};
	int32_t menu_mode = menu_out;

/* Drag & Drop file */
void
drop_file(const char* dropped_fileurl)
{
  char strwork[20];
  char *p;
  uint32_t len = (uint32_t)strlen(dropped_fileurl);

	strcpy(strwork, dropped_fileurl + len - 3);//拡張子GET
	for(len=0; len<3; len++){
	  if (strwork[len] >= 'a' && strwork[len] <= 'z') {//大文字に変換
	    strwork[len] = 'A' + strwork[len] - 'a';
	  }
	}
	int drv = 0;//A Drive
	p = strstr(fdimg, strwork);// FDD image check
	if(p != NULL){
	   //if A not empty? then B ?
	   if(strcmp((char *)Config.FDDImage[0],"\0") != 0 ) drv = 1;//B Drive
	   if(strcmp((char *)Config.FDDImage[drv],dropped_fileurl) != 0 ){//一応一致チェック
	    strcpy((char *)Config.FDDImage[drv], dropped_fileurl);
	    FDD_SetFD(drv, Config.FDDImage[drv], 0);
	   }
	}
	p = strstr(saimg, strwork);// SASI image check
	if(p != NULL){
	   strcpy((char *)Config.HDImage[0], dropped_fileurl);
	}
	p = strstr(scimg, strwork);// SCSI image check
	if(p != NULL){
	   strcpy((char *)Config.SCSIEXHDImage[0], dropped_fileurl);
	}
	p = strstr(moimg, strwork);// MO image check(SCSI-ID=5)
	if(p != NULL){
	   strcpy((char *)Config.SCSIEXHDImage[5], dropped_fileurl);
	}

	if(menu_mode == menu_in) WinUI_Menu(1);//menu redraw
	p6logd("DropFile:%s set %d\n",dropped_fileurl,drv);

  return;
}

//
// main
//
int main(int argc, char *argv[])
{
	SDL_Event ev;
	SDL_Keycode menu_key_down;

#if defined(ANDROID) || TARGET_OS_IPHONE
	int32_t vk_cnt = -1;
	int32_t menu_cnt = -1;
	uint8_t state;
#endif

	p6logd("PX68K Ver.%s\n", PX68KVERSTR);

#ifdef RFMDRV
	struct sockaddr_in dest;

	memset(&dest, 0, sizeof(dest));
	dest.sin_port = htons(2151);
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = inet_addr("127.0.0.1");

	rfd_sock = socket(AF_INET, SOCK_STREAM, 0);
	connect (rfd_sock, (struct sockaddr *)&dest, sizeof(dest));
#endif

	if (set_modulepath((char *)winx68k_dir, sizeof(winx68k_dir)))
		return 1;

	dosio_init();
	File_Setcd(winx68k_dir);
    p6logd("%s\n",winx68k_dir);

	SRAM_Init();
	LoadConfig();

#ifdef NOSOUND
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		return 1;
	}
#else
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
		p6logd("SDL_Init error\n");
		if (!SDL_Init(SDL_INIT_VIDEO)) {
			return 1;
		}
	} else {
		sdlaudio = 0;//SDL init OK
	}
#endif

	strcat(window_title,APPNAME);
	strcat(window_title," SDL3");/*SDL3*/

#ifdef USE_OGLES20
	SDL_GetCurrentDisplayMode(0, &sdl_dispmode);
	p6logd("width: %d height: %d", sdl_dispmode.w, sdl_dispmode.h);
	// ナビゲーションバーを除くアプリが触れる画面
	realdisp_w = sdl_dispmode.w, realdisp_h = sdl_dispmode.h;

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );

	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 ); 

#if TARGET_OS_IPHONE
	/* for iPhone */
	sdl_window = SDL_CreateWindow(window_title, FULLSCREEN_WIDTH, FULLSCREEN_HEIGHT,
			SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_BORDERLESS);
#else
	/* for Android: window sizeの指定は関係なくフルスクリーンになるみたい*/
	sdl_window = SDL_CreateWindow(window_title, FULLSCREEN_WIDTH, FULLSCREEN_HEIGHT,
			SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
#endif
#else
	/* SDL3 for GPU */
	SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
	if (Config.WinStrech == 1){
	  flags |= SDL_WINDOW_RESIZABLE;
	}
	sdl_window = SDL_CreateWindow(window_title,FULLSCREEN_WIDTH,FULLSCREEN_HEIGHT,flags);
#endif

	if (sdl_window == NULL) {
		p6logd("Cαn`t Create sdl_window: %ld", sdl_window);
	}

	SDL_SetWindowPosition(sdl_window, winx, winy);

	/*Create Renderer */
	sdl_render = SDL_CreateRenderer( sdl_window ,NULL );
	SDL_SetRenderLogicalPresentation( sdl_render ,FULLSCREEN_WIDTH,
								FULLSCREEN_HEIGHT,SDL_LOGICAL_PRESENTATION_LETTERBOX);

	Soft_kbd_CreateScreen();// Soft Keyboard Window init.

	if (!WinDraw_MenuInit()) {
		WinX68k_Cleanup();
		WinDraw_Cleanup();
		return 1;
	}

	SplashFlag = 20;

	StatBar_Show(Config.WindowFDDStat);
	WinGetRootSize();
	WinDraw_ChangeSize();
	WinDraw_ChangeMode(FALSE);
	WinUI_Init();
	WinDraw_StartupScreen();

	uint32_t err_msg_no = 0;
	if (!WinX68k_Init()) {
		//WinX68k_Cleanup();
		//WinDraw_Cleanup();
		err_msg_no = 1;
		//return 1;
	}

	if (!WinX68k_LoadROMs()) {
		//WinX68k_Cleanup();
		//WinDraw_Cleanup();
		err_msg_no = 2;
		//exit (1);
	}
	memcpy(&MEM[0xbffffc], &SCSIIPL[0x20], 4); /*RST Vect patch for ROM v1.0*/

	Keyboard_Init(); //WinDraw_Init()前に移動
	Keymap_Init(); //Load Key Map file

	if (!WinDraw_Init()) {
		WinDraw_Cleanup();
		Error("Error: Can't init screen.\n");
		err_msg_no = 3;
	}

	if(err_msg_no != 0) WinDraw_Message(err_msg_no);

	FDD_Init();
	SysPort_Init();
	Mouse_Init();
	WinX68k_Reset();
	GamePad_Init();
	Timer_Init();

	// MIDI_Init();
	MIDI_SetMimpiMap((char *)Config.ToneMapFile);	// 音色設定ファイル使用反映
	MIDI_EnableMimpiDef(Config.ToneMap);

	//Dummy SampleRate 
	if(Config.SampleRate == 0) SoundSampleRate = 11025;
	else SoundSampleRate = Config.SampleRate;

	// SDL Create Sound stream 
	if (sdlaudio == 0 && !DSound_Init(SoundSampleRate)) {
		if (Config.DSAlert)
		fprintf(stderr, "Can't init sound.\n");
	}

	//波形生成初期化(ADPCM,OPM,Mercury)
	ADPCM_Init(SoundSampleRate);
	OPM_Init(4000000/*3579545*/, SoundSampleRate);
#ifndef	NO_MERCURY
	Mcry_Init(SoundSampleRate, winx68k_dir);
#endif

	// Set Sound Volume
	ADPCM_SetVolume((uint8_t)Config.PCM_VOL);
	OPM_SetVolume((uint8_t)Config.OPM_VOL);
#ifndef	NO_MERCURY
	Mcry_SetVolume((uint8_t)Config.MCR_VOL);
#endif

	// No Sound set
	if(Config.SampleRate == 0){
		playing = FALSE;
	}

	DSound_Resume();// Start Sound

	// command lineから実行 引数あり
	if(argc > 1){
	  get_cmd_line(argc, (char **)argv);
	}

	FDD_SetFD(0, (char *)Config.FDDImage[0], 0);
	FDD_SetFD(1, (char *)Config.FDDImage[1], 0);

	//SDL_StartTextInput();

	while (1) {
		// OPM_RomeoOut(Config.BufferSize * 5);
		if (menu_mode == menu_out
		    && (Config.NoWaitMode || Timer_GetCount()) && err_msg_no == 0) {
			WinX68k_Exec();
#if defined(ANDROID) || TARGET_OS_IPHONE
			if (vk_cnt > 0) {
				vk_cnt--;
				if (vk_cnt == 0) {
					p6logd("vk_cnt 0");
				}
			}
			if  (menu_cnt > 0) {
				menu_cnt--;
				if (menu_cnt == 0) {
					p6logd("menu_cnt 0");
				}
			}
#endif
			if (SplashFlag) {
				SplashFlag--;
				if (SplashFlag == 0)
					WinDraw_HideSplash();
			}

			Pal_TrackContrast();//コントラスト調整

		}

		menu_key_down = SDLK_UNKNOWN;

		/* menu clickable area */
		uint32_t menu_mouse_area_xr =  30, menu_mouse_area_xl = 720;
		uint32_t menu_mouse_area_yu = 110, menu_mouse_area_yd = 300;

		while (SDL_PollEvent(&ev)) {
			switch (ev.type) {
			case SDL_EVENT_DISPLAY_FIRST...SDL_EVENT_DISPLAY_LAST:
				break;
			case SDL_EVENT_QUIT:
				goto end_loop;
			case SDL_EVENT_WINDOW_RESIZED:
				ScreenClearFlg = 1;
			break;
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			break;
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
					if(ev.window.windowID == SDL_GetWindowID(sdl_window)){ goto end_loop; }
					if(ev.window.windowID == SDL_GetWindowID(sft_kbd_window)){ Soft_kbd_Show(0); }//消す
			break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				if(ev.button.button == SDL_BUTTON_LEFT){//左ボタンを押した
				 if(ev.window.windowID == SDL_GetWindowID(sft_kbd_window)){//SoftKey Window
				   draw_soft_kbd(ev.button.x,ev.button.y, 0);
				   if(menu_mode == menu_in){//menu mode
				    extern uint8_t Key_X68;
				    if(Key_X68 == 0x3c) menu_key_down = 0x40000052; //  up
				    if(Key_X68 == 0x3e) menu_key_down = 0x40000051; //  dawn
				    if(Key_X68 == 0x1d) menu_key_down = 0x0d; //  return
				    if(Key_X68 == 0x01) menu_key_down = 0x1b; //  escape
				    if(Key_X68 == 0x2b) menu_key_down = 0x78; //  x
				    if(Key_X68 == 0x2a) menu_key_down = 0x7a; //  z
				   Keyboard_Init();
				   }
				 }
				 else if(ev.window.windowID == SDL_GetWindowID(sdl_window)){// SDL Window
				  if(menu_mode == menu_in){//menu mode
					 if((menu_state!=ms_file)&&(ev.button.x > menu_mouse_area_xr)&&(ev.button.x < menu_mouse_area_xl)&&
					    (ev.button.y > menu_mouse_area_yu)&&(ev.button.y < menu_mouse_area_yd)){
				     menu_key_down = 0x0d; // click Left button = return
					 }
					 if(menu_state==ms_file) menu_key_down = 0x0d; // click Left button = return
				  }
				  else{//X68000 mode
				    if(ev.window.windowID == SDL_GetWindowID(sdl_window)){ Mouse_Event((int)1, 1, 0); }
				  }
				 }
				}
				else if(ev.button.button == SDL_BUTTON_RIGHT){//右ボタン押した
				 if(ev.window.windowID == SDL_GetWindowID(sft_kbd_window)){//SoftKey Window
				   // none.
				 }
				 else if(ev.window.windowID == SDL_GetWindowID(sdl_window)){// SDL Window
					if(menu_mode == menu_in){//menu mode
					  if(menu_state==ms_file) menu_mouse_area_yd += 90;//file選択モードは範囲拡大
					  if((ev.button.x > menu_mouse_area_xr)&&(ev.button.x < menu_mouse_area_xl)&&
					     (ev.button.y > menu_mouse_area_yu)&&(ev.button.y < menu_mouse_area_yd)){
					   menu_key_down = 0x1b; // click Right button = esc
					  }
					  else{
					   int x, y;
					   SDL_GetWindowPosition(sdl_window, &x, &y);
					   SDL_SetWindowPosition(sft_kbd_window, ev.button.x + x, ev.button.y + y);
					   Soft_kbd_Show(1);// SoftKey Window ON	
					  }
					}
					else{// X68000 mode
					 Mouse_Event((int)2, 1, 0);
					}
				 }
				}
			break;
			case SDL_EVENT_MOUSE_BUTTON_UP:
				if(ev.button.button == SDL_BUTTON_LEFT){//Mouse L-button release
					if(ev.window.windowID == SDL_GetWindowID(sdl_window)){ Mouse_Event((int)1, 0, 0); }
					if(ev.window.windowID == SDL_GetWindowID(sft_kbd_window)){ draw_soft_kbd(1,1,0); }// ReDrawSoftKey
					//p6logd("UP/LEFT:x=%d,y=%d\n", ev.button.x,ev.button.y);
				}
				else if(ev.button.button == SDL_BUTTON_RIGHT){//Mouse R-button release
					if(ev.window.windowID == SDL_GetWindowID(sdl_window)){ Mouse_Event((int)2, 0, 0); }
					//p6logd("UP/RIGHT:x=%d,y=%d\n", ev.button.x,ev.button.y);
				}
			break;
			case SDL_EVENT_MOUSE_MOTION:
				if(ev.window.windowID == SDL_GetWindowID(sdl_window)){//on main-window
				if(menu_mode != menu_out){//menu mode
					if((menu_state!=ms_file)&&(ev.button.x>25)&&(ev.button.x<190) &&(ev.button.y>121)&&(ev.button.y<289)){
					uint32_t m_locate=(ev.button.y-121)/24;
					extern int32_t mkey_pos,mkey_y;
					if( m_locate + mkey_pos > mkey_y) menu_key_down = 0x40000051; // Down Wheel
					if( m_locate + mkey_pos < mkey_y) menu_key_down = 0x40000052; // Up   Wheel
					}
					if((menu_state==ms_file)&&(ev.button.x>25)&&(ev.button.x<700) &&(ev.button.y>50)&&(ev.button.y<385)){
					uint32_t m_locate=(ev.button.y-50)/24;
					if( m_locate > mfl.y) menu_key_down = 0x40000051; // Down menu
					if( m_locate < mfl.y) menu_key_down = 0x40000052; // Up   menu
					}
				}
				else{
				Mouse_Event((int)0,	(float)ev.motion.xrel * Config.MouseSpeed /10,
									(float)ev.motion.yrel * Config.MouseSpeed /10);/*mouse support*/
				//p6logd("x:%d y:%d xrel:%d yrel:%d\n", ev.motion.x, ev.motion.y, ev.motion.xrel, ev.motion.yrel);
				}
				}
				break;
			case SDL_EVENT_MOUSE_WHEEL:
				if(ev.window.windowID == SDL_GetWindowID(sdl_window)){//Main window
				 if(menu_mode != menu_out){//menu mode
				  if(ev.wheel.y > 0) menu_key_down = 0x40000052; // Up   Wheel
				  if(ev.wheel.y < 0) menu_key_down = 0x40000051; // Down Wheel
				  if(ev.wheel.x > 0) menu_key_down = 0x4000004f; // Right Wheel
				  if(ev.wheel.x < 0) menu_key_down = 0x40000050; // Left Wheel
				 }
				}
				break;
#if defined(ANDROID) || TARGET_OS_IPHONE
			case SDL_APP_WILLENTERBACKGROUND:
				DSound_Pause();
				break;
			case SDL_APP_WILLENTERFOREGROUND:
				DSound_Resume();
				break;
			case SDL_FINGERDOWN:
				//p6logd("FINGERDOWN: tid: %lld,,, x:%f y:%f", ev.tfinger.touchId, ev.tfinger.x, ev.tfinger.y);
				if (touchId == -1) {
					touchId = ev.tfinger.touchId;
				}
				break;
			case SDL_FINGERMOTION:
				float kx, ky, dx, dy;
				//p6logd("FM: x:%f y:%f dx:%f dy:%f\n", ev.tfinger.x, ev.tfinger.y, ev.tfinger.dx, ev.tfinger.dy);
				if (vk_cnt == 0) {
					kx = ev.tfinger.x * 800;
					ky = ev.tfinger.y * 600;
					dx = ev.tfinger.dx * 800;
					dy = ev.tfinger.dy * 600;
					if (kbd_x < kx && kbd_x + kbd_w > kx &&
					    kbd_y < ky && kbd_y + kbd_h > ky) {
						kbd_x += dx;
						kbd_y += dy;
						if (kbd_x < 0) kbd_x = 0;
						if (kbd_y < 0) kbd_y = 0;
						if (kbd_x > 700) {
							vk_cnt = -1;
						}
						if (kbd_y > 550) kbd_y = 550;
					}
				} else if (Config.JoyOrMouse) { // Mouse mode is off when the keyboard is active
					Mouse_Event(0, ev.tfinger.dx * 50 * Config.MouseSpeed, ev.tfinger.dy * 50 * Config.MouseSpeed);
				}
				break;
#endif
			case SDL_EVENT_KEY_DOWN:
#if defined(ANDROID) || TARGET_OS_IPHONE
				static uint32_t bef = 0;
				uint32_t now;
				switch (ev.key.keysym.sym) {
				case SDLK_AC_BACK:
					now = timeGetTime();
					if (now - bef < 1000) {
						goto end_loop;
					}
					bef = now;
					break;
				case SDLK_MENU:
					if (menu_mode == menu_out) {
						menu_mode = menu_enter;
						DSound_Pause();
					} else {
						DSound_Resume();
						menu_mode = menu_out;
					}
					break;
				}
#endif
				//p6logd("keydown: 0x%x\n", ev.key.key);
				// F12 だけじゃなくescでもmenu抜ける
				if ((ev.key.key == SDLK_ESCAPE) &&
					(menu_mode != menu_out) && (menu_state == ms_key)){
						ev.key.key = SDLK_F12;
				}
				if (ev.key.key == SDLK_F12) {
					if (menu_mode == menu_out) {// Enter MenuMode
						menu_mode = menu_enter;
						DSound_Pause();
					} else {// Return from MenuMode
						if (Config.SampleRate != SoundSampleRate){
						  SoundSampleRate = Config.SampleRate;
						  if(SoundSampleRate==0){ playing = FALSE;}
						  else{
						   playing = TRUE;
						   ADPCM_Init(SoundSampleRate);
						   OPM_SetRate(4000000, SoundSampleRate);
#ifndef	NO_MERCURY
						   Mcry_Init(SoundSampleRate, winx68k_dir);
#endif
						   //SDL Sound stream freq. change
						   DSound_RateChange(SoundSampleRate);
						  }
						}
						DSound_Resume();
						menu_mode = menu_out;
						ScreenClearFlg = 1;
						break;
					}
				}
				if (ev.key.key == SDLK_F11) { /*toggle Full-Screen mode*/
					if(FullScreenFlag == 0){ FullScreenFlag = 1; }
					else{FullScreenFlag = 0;}
					WinDraw_ChangeMode(FullScreenFlag);
				}
#ifdef WIN68DEBUG
				if (ev.key.key == SDLK_F10) {
					traceflag ^= 1;
					p6logd("trace %s\n", (traceflag)?"on":"off");
				}
#endif
				if (menu_mode != menu_out) {
					menu_key_down = ev.key.key;
				} else {
					Keyboard_KeyDown(ev.key.key,ev.key.scancode);//phisical code + α
				}
				break;
			case SDL_EVENT_KEY_UP:
				//p6logd("keyup: 0x%x 0x%x\n", ev.key.keysym.sym,ev.key.keysym.scancode);
				Keyboard_KeyUp(ev.key.key,ev.key.scancode);//phisical code + α
				break;
			case SDL_EVENT_GAMEPAD_AXIS_MOTION:
				GamePadAxis_Update(ev.gaxis.which, ev.gaxis.axis, ev.gaxis.value);
				break;
			case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
				GamePadButton_Update(ev.gbutton.which, ev.gbutton.button, 1);
				if((menu_mode == menu_out) && (get_joy_downstate() == (JOY_HOME ^ 0xff)) ){// HOME(Menu in)
				  menu_mode = menu_enter;
				  reset_joy_downstate();
				  DSound_Pause();
				}
				break;
			case SDL_EVENT_GAMEPAD_BUTTON_UP:
				GamePadButton_Update(ev.gbutton.which, ev.gbutton.button, 0);
				break;
			case SDL_EVENT_GAMEPAD_ADDED:
				GamePad_Add(ev.gdevice.which);
				break;
			case SDL_EVENT_GAMEPAD_REMOVED:
				GamePad_Removed(ev.gdevice.which);
				break;
			case SDL_EVENT_GAMEPAD_REMAPPED:
				GamePad_Remapped(ev.gdevice.which);
				break;
			case SDL_EVENT_DROP_BEGIN:
			case SDL_EVENT_DROP_COMPLETE:
				break;
			case SDL_EVENT_DROP_FILE:
				drop_file(ev.drop.data);
				break;
			default:
				break;
			}
		}

#if defined(ANDROID) || TARGET_OS_IPHONE

		state = Joystick_get_vbtn_state(7);

		if (menu_mode == menu_in) {
			if (state == VBTN_OFF && menu_cnt == 0) {
				menu_cnt = -2;
			}
			if (state == VBTN_ON && menu_cnt == -2) {
				DSound_Resume();
				menu_mode = menu_out;
			}
		} else if (menu_mode == menu_out) {
			if (state == VBTN_OFF && menu_cnt == -2) {
				menu_cnt = -1;
			}
			if (menu_cnt == -1 && state == VBTN_ON) {
				p6logd("menu_cnt start");
				menu_cnt = 20;
			} else if (menu_cnt > 0 && state == VBTN_OFF) {
				menu_cnt = -1;
			}
			if (menu_cnt == 0) {
				p6logd("menu mode on");
				menu_mode = menu_enter;
				DSound_Pause();
			}
		}

#endif
		if (menu_mode != menu_out) {
			int32_t ret; 

			Menu_GamePad_Update(menu_key_down); // XBOX like GamePad and KeyPad

			if(ScreenClearFlg == 1){/*Resizable Window support in MENU*/
			 Update_Screen(1);
			}

			ret = WinUI_Menu(menu_mode == menu_enter);
			menu_mode = menu_in;
			if (ret == WUM_MENU_END) {
				DSound_Resume();
				menu_mode = menu_out;
			} else if (ret == WUM_EMU_QUIT) {
				goto end_loop;
			}
		}

#if defined(ANDROID) || TARGET_OS_IPHONE

		if (menu_mode == menu_out) {
			state = Joystick_get_vbtn_state(6);
			if (vk_cnt == -1 && state == VBTN_ON) {
				p6logd("vk_cnt start");
				vk_cnt = 20;
			} else if (vk_cnt > 0 && state == VBTN_OFF) {
				vk_cnt = -1;
			}
			if (kbd_x > 700 && vk_cnt == 0) {
				kbd_x = 0, kbd_y = 0;
				p6logd("do_kbd");
			}
			if (kbd_x < 700) {
#ifdef USE_OGLES20
				Keyboard_skbd();
#endif
			}
		}
#endif

	}
end_loop:

	DSound_Pause();

	WinX68k_Reset();
	for(uint_fast32_t i=0; i<130; i++){WinX68k_Exec();}// Reset and run(few step)

	Memory_WriteB(0xe8e00d, 0x31);	// SRAM書き込み許可
	Memory_WriteD(0xed0040, Memory_ReadD(0xed0040)+1); // 積算稼働時間(min.)
	Memory_WriteD(0xed0044, Memory_ReadD(0xed0044)+1); // 積算起動回数

	OPM_Cleanup();
#ifndef	NO_MERCURY
	Mcry_Cleanup();
#endif

	Config.DisplayNo=SDL_GetDisplayForWindow(sdl_window);
	SDL_GetWindowPosition(sdl_window,&Config.WinPosX,&Config.WinPosY);

	GamePad_Cleanup();
	SRAM_Cleanup();
	FDD_Cleanup();
	//CDROM_Cleanup();
	MIDI_Cleanup();
	DSound_Cleanup();
	WinX68k_Cleanup();
	WinDraw_Cleanup();
	Soft_kbd_CleanupScreen();
	WinDraw_CleanupScreen();

	SaveConfig();
	SDL_Quit();

#if defined(ANDROID) || TARGET_OS_IPHONE
	exit(0);
#endif
	return 0;
}