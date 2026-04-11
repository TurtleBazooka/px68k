// ---------------------------------------------------------------------------------------
//  FDD.C - 内蔵FDD Unit（イメージファイルの管理とFD挿抜割り込みの発生）
// ---------------------------------------------------------------------------------------

#include "common.h"
#include "status.h"
#include "irqh.h"
#include "ioc.h"
#include "fdc.h"
#include "fdd.h"
#include "disk_d88.h"
#include "disk_xdf.h"
#include "disk_dim.h"
#include <string.h>
#include <unistd.h>

typedef struct {
	int32_t SetDelay[4];
	int32_t Types[4];
	int32_t ROnly[4];
	int32_t EMask[4];
	int32_t Blink[4];
	int32_t Access;
} FDDINFO;

static FDDINFO fdd;
static int32_t (*SetFD[4])(int32_t, char*)                             = { 0, XDF_SetFD,        D88_SetFD,        DIM_SetFD };
static int32_t (*Eject[4])(int32_t)                                    = { 0, XDF_Eject,        D88_Eject,        DIM_Eject };
static int32_t (*Seek[4])(int32_t, int32_t, FDCID*)                        = { 0, XDF_Seek,         D88_Seek,         DIM_Seek };
static int32_t (*ReadID[4])(int32_t, FDCID*)                           = { 0, XDF_ReadID,       D88_ReadID,       DIM_ReadID };
static int32_t (*WriteID[4])(int32_t, int32_t, unsigned char*, int32_t)        = { 0, XDF_WriteID,      D88_WriteID,      DIM_WriteID };
static int32_t (*Read[4])(int32_t, FDCID*, unsigned char*)             = { 0, XDF_Read,         D88_Read,         DIM_Read };
static int32_t (*ReadDiag[4])(int32_t, FDCID*, FDCID*, unsigned char*) = { 0, XDF_ReadDiag,     D88_ReadDiag,     DIM_ReadDiag };
static int32_t (*Write[4])(int32_t, FDCID*, unsigned char*, int32_t)       = { 0, XDF_Write,        D88_Write,        DIM_Write };
static int32_t (*GetCurrentID[4])(int32_t, FDCID*)                     = { 0, XDF_GetCurrentID, D88_GetCurrentID, DIM_GetCurrentID };

int32_t FDD_IsReading                                              = 0;

// -----------------------------------------------------------------------
//   イメージタイプ判別
// -----------------------------------------------------------------------
static void ConvertCapital(char* buf)
{
	for ( ; *buf; buf++) {
		if ( (((uint8_t)*buf>=0x80)&&((uint8_t)*buf<=0x9f))||((uint8_t)*buf>=0xe0) ) {
			buf++;
		} else if ( (*buf>='a')&&(*buf<='z') ) *buf -= 0x20;
	}
}

static int32_t GetDiskType(char* file)
{
	char tmp[8], *p;
	int32_t ret = FD_XDF;
	p = strrchr(file, '.');
	if ( p ) {
		memset(tmp, 0, sizeof(tmp));
		strncpy(tmp, p+1, 3);
		ConvertCapital((char*)tmp);
		if ( (!strncmp(tmp, "D88", 3))||(!strncmp(tmp, "88D", 3)) )
			ret = FD_D88;
		else if ( !strncmp(tmp, "DIM", 3) )
			ret = FD_DIM;
	}
	return ret;
}



// -----------------------------------------------------------------------
//   挿抜割り込み
// -----------------------------------------------------------------------
int32_t FASTCALL FDD_Int(uint8_t irq)
{
	IRQH_IRQCallBack(irq);
	if ( irq==1 ){
		return (int32_t)(IOC_IntVect+1);
	}

	return -1;
}


// -----------------------------------------------------------------------
//   FDせっと
//     すぐには割り込み上げないです
// -----------------------------------------------------------------------
void FDD_SetFD(int32_t drive, char* filename, int32_t readonly)
{
	int32_t type = GetDiskType(filename);
	if ( (drive<0)||(drive>3) ) return;
	FDD_EjectFD(drive);
	if ( SetFD[type] ) {
		if ( SetFD[type](drive, filename) ) {
			fdd.Types[drive]  = type;
			fdd.ROnly[drive] |= readonly;
			fdd.SetDelay[drive] = 3;
			fdd.EMask[drive] = 0;
			fdd.Blink[drive] = 0;
			StatBar_SetFDD(drive, filename);
			StatBar_ParamFDD(drive, (fdd.Types[drive]!=FD_Non)?((fdd.Access==drive)?2:1):0, ((fdd.Types[drive]!=FD_Non)&&(!fdd.EMask[drive]))?1:0, (fdd.Blink[drive])?1:0);
		}
	}
}


// -----------------------------------------------------------------------
//   いじぇくと
// -----------------------------------------------------------------------
void FDD_EjectFD(int32_t drive)
{
	int32_t type;
	if ( (drive<0)||(drive>3) ) return;
	type = fdd.Types[drive];
	if ( Eject[type] ) {
		Eject[type](drive);
		if ( IOC_IntStat&2 ) IRQH_Int(1, &FDD_Int);
	}
	fdd.Types[drive] = FD_Non;
	fdd.ROnly[drive] = 0;
	fdd.EMask[drive] = 0;
	fdd.Blink[drive] = 0;
	StatBar_SetFDD(drive, "");
	StatBar_ParamFDD(drive, (fdd.Types[drive]!=FD_Non)?((fdd.Access==drive)?2:1):0, ((fdd.Types[drive]!=FD_Non)&&(!fdd.EMask[drive]))?1:0, (fdd.Blink[drive])?1:0);
}


// -----------------------------------------------------------------------
//   Eject Mask / Blink / AccessDrive
// -----------------------------------------------------------------------
void FDD_SetEMask(int32_t drive, int32_t emask)
{
	if ( (drive<0)||(drive>3) ) return;
	if ( fdd.EMask[drive]==emask ) return;
	fdd.EMask[drive] = emask;
	StatBar_ParamFDD(drive, (fdd.Types[drive]!=FD_Non)?((fdd.Access==drive)?2:1):0, ((fdd.Types[drive]!=FD_Non)&&(!fdd.EMask[drive]))?1:0, (fdd.Blink[drive])?1:0);
}

void FDD_SetAccess(int32_t drive)
{
	if ( fdd.Access==drive ) return;
	fdd.Access = drive;
	StatBar_ParamFDD(0, (fdd.Types[0]!=FD_Non)?((fdd.Access==0)?2:1):0, ((fdd.Types[0]!=FD_Non)&&(!fdd.EMask[0]))?1:0, (fdd.Blink[0])?1:0);
	StatBar_ParamFDD(1, (fdd.Types[1]!=FD_Non)?((fdd.Access==1)?2:1):0, ((fdd.Types[1]!=FD_Non)&&(!fdd.EMask[1]))?1:0, (fdd.Blink[1])?1:0);
}

void FDD_SetBlink(int32_t drive, int32_t blink)
{
	if ( (drive<0)||(drive>3) ) return;
	if ( fdd.Blink[drive]==blink ) return;
	fdd.Blink[drive] = blink;
	StatBar_ParamFDD(drive, (fdd.Types[drive]!=FD_Non)?((fdd.Access==drive)?2:1):0, ((fdd.Types[drive]!=FD_Non)&&(!fdd.EMask[drive]))?1:0, (fdd.Blink[drive])?1:0);
}


// -----------------------------------------------------------------------
//   初期化
// -----------------------------------------------------------------------
void FDD_Init(void)
{
	memset(&fdd,0 , sizeof(FDDINFO));
	fdd.Access = -1;
	D88_Init();
	XDF_Init();
	DIM_Init();
}


// -----------------------------------------------------------------------
//   終了
// -----------------------------------------------------------------------
void FDD_Cleanup(void)
{
	D88_Cleanup();
	XDF_Cleanup();
	DIM_Cleanup();
}


// -----------------------------------------------------------------------
//   りせっと
// -----------------------------------------------------------------------
void FDD_Reset(void)
{
	int_fast16_t i;
	FDD_SetAccess(-1);
	for (i=0; i<4; i++) {
		FDD_SetEMask(i, 0);
		FDD_SetBlink(i, 0);
	}
}


// -----------------------------------------------------------------------
//   FD入れ替えが起こっていたら割り込み発生
// -----------------------------------------------------------------------
void FDD_SetFDInt(void)
{
	int_fast16_t i;
	for (i=0; i<4; i++) {
		if ( fdd.SetDelay[i] ) {
			fdd.SetDelay[i]--;
			if ( fdd.SetDelay[i]<=0 ) {
				if ( IOC_IntStat&2 ) IRQH_Int(1, &FDD_Int);
				fdd.SetDelay[i] = 0;
			}
		}
	}
}


int32_t FDD_Seek(int32_t drv, int32_t trk, FDCID* id)
{
	static int32_t trk_B4=0;
	int32_t type;

	if ( (drv<0)||(drv>3) ) return FALSE;

	type = fdd.Types[drv];
	if ( Seek[type] ){
		uint32_t movetrack = abs(trk_B4 - trk);
		usleep(movetrack * 200 + 150);// wait...
		trk_B4 = trk;
		return Seek[type](drv, trk, id);
	}

	return FALSE;
}

int32_t FDD_ReadID(int32_t drv, FDCID* id)
{
	int32_t type;

	if ( (drv<0)||(drv>3) ) return FALSE;

	type = fdd.Types[drv];
	if ( ReadID[type] ){
		return ReadID[type](drv, id);
	}

	return FALSE;
}

int32_t FDD_WriteID(int32_t drv, int32_t trk,uint8_t* buf, int32_t num)
{
	int32_t type;

	if ( (drv<0)||(drv>3) ) return FALSE;

	type = fdd.Types[drv];
	if ( WriteID[type] ){
		return WriteID[type](drv, trk, buf, num);
	}

	return FALSE;
}


int32_t FDD_Read(int32_t drv, FDCID* id, uint8_t* buf)
{
	int32_t type;

	if ( (drv<0)||(drv>3) ) return FALSE;

	type = fdd.Types[drv];
	if ( Read[type] )
	{
		usleep(300);// wait...
		FDD_IsReading = 1;
		return Read[type](drv, id, buf);
	}

	return FALSE;
}


int32_t FDD_ReadDiag(int32_t drv, FDCID* id, FDCID* retid, uint8_t* buf)
{
	int32_t type;

	if ( (drv<0)||(drv>3) ) return FALSE;

	type = fdd.Types[drv];
	if ( ReadDiag[type] ){
		return ReadDiag[type](drv, id, retid, buf);
	}

	return FALSE;
}


int32_t FDD_Write(int32_t drv, FDCID* id, uint8_t* buf, int32_t del)
{
	int32_t type;

	if ( (drv<0)||(drv>3) ) return FALSE;

	type = fdd.Types[drv];
	if ( Write[type] ){
		usleep(300);// wait...
		return Write[type](drv, id, buf, del);
	}

	return FALSE;
}


int32_t FDD_GetCurrentID(int32_t drv, FDCID* id)
{
	int32_t type;

	if ( (drv<0)||(drv>3) ) return FALSE;

	type = fdd.Types[drv];
	if ( GetCurrentID[type] ){
		return GetCurrentID[type](drv, id);
	}

	return FALSE;
}


int32_t FDD_IsReady(int32_t drv)
{
	if ( (drv<0)||(drv>3) ) return FALSE;

	if ( (fdd.Types[drv]!=FD_Non)&&(!fdd.SetDelay[drv]) ){
		return TRUE;
	}

	return FALSE;
}


int32_t FDD_IsReadOnly(int32_t drv)
{
	if ( (drv<0)||(drv>3) ) return FALSE;

	return fdd.ROnly[drv];
}


void FDD_SetReadOnly(int32_t drv)
{
	fdd.ROnly[drv] |= 1;
}
