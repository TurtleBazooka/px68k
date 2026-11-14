#ifndef _win68_opm_fmgen
#define _win68_opm_fmgen

int32_t OPM_Init(int32_t clock, int32_t rate);
void OPM_Cleanup(void);
void OPM_Reset(void);
void OPM_Update(int16_t *buffer, int32_t length, int16_t *pbsp, int16_t *pbep);
void OPM_Write(uint32_t r, uint8_t v);
uint8_t OPM_Read(void);
void OPM_Timer(uint32_t step);
void OPM_SetVolume(uint8_t vol);
void OPM_SetRate(int32_t clock, int32_t rate);
void OPM_RomeoOut(uint32_t delay);

int32_t M288_Init(int32_t clock, int32_t rate, const char* path);
void M288_Cleanup(void);
void M288_Reset(void);
void M288_Update(int16_t *buffer, int32_t length);
void M288_Write(uint8_t r, uint8_t v);
uint8_t M288_Read(uint8_t a);
void M288_Timer(uint32_t step);
void M288_SetVolume(uint8_t vol);
void M288_SetRate(int32_t clock, int32_t rate);
void M288_RomeoOut(uint32_t delay);

#endif //_win68_opm_fmgen
