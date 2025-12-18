#ifndef dswin_h__
#define dswin_h__

#include "common.h"

extern int32_t audio_fd;

void DSound_RateChange(uint32_t rate);
int32_t DSound_Init(uint32_t rate, uint32_t length);
int32_t DSound_Cleanup(void);

void DSound_Play(void);
void DSound_Stop(void);
void FASTCALL DSound_Send0(int32_t clock);

void DS_SetVolumeOPM(int32_t vol);
void DS_SetVolumeADPCM(int32_t vol);
void DS_SetVolumeMercury(int32_t vol);

#endif /* dswin_h__ */
