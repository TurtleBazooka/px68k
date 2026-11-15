#ifndef _winx68k_fdd
#define _winx68k_fdd

#include "common.h"

typedef struct {
	uint8_t c;
	uint8_t h;
	uint8_t r;
	uint8_t n;
} FDCID;

enum {
	FD_Non = 0,
	FD_XDF,
	FD_D88,
	FD_DIM,
};

int32_t FDD_Int(uint8_t irq);
void FDD_SetFD(int32_t drive, char* filename, int32_t readonly);
void FDD_EjectFD(int32_t drive);
void FDD_Init(void);
void FDD_Cleanup(void);
void FDD_Reset(void);
void FDD_SetFDInt(void);
int32_t FDD_Seek(int32_t drv, int32_t trk, FDCID* id);
int32_t FDD_ReadID(int32_t drv, FDCID* id);
int32_t FDD_WriteID(int32_t drv, int32_t trk, uint8_t* buf, int32_t num);
int32_t FDD_Read(int32_t drv, FDCID* id, uint8_t* buf);
int32_t FDD_ReadDiag(int32_t drv, FDCID* id, FDCID* retid, uint8_t* buf);
int32_t FDD_Write(int32_t drv, FDCID* id, uint8_t* buf, int32_t del);
int32_t FDD_IsReady(int32_t drv);
int32_t FDD_IsReadOnly(int32_t drv);
int32_t FDD_GetCurrentID(int32_t drv, FDCID* id);
void FDD_SetReadOnly(int32_t drv);
void FDD_SetEMask(int32_t drive, int32_t emask);
void FDD_SetAccess(int32_t drive);
void FDD_SetBlink(int32_t drive, int32_t blink);

/* Misc: Used to trigger rumble when FDD is reading data.
 * Reset at every frame */
extern int32_t FDD_IsReading;

#endif


