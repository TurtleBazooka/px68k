#ifndef _winx68k_mercury_h
#define _winx68k_mercury_h

extern uint8_t Mcry_LRTiming;

void Mcry_Update(int16_t *buffer, int32_t length);
void Mcry_PreUpdate(int32_t clock);

void    Mcry_Write(uint32_t adr, uint8_t data);
uint8_t Mcry_Read(uint32_t adr);

void Mcry_SetClock(void);
void Mcry_SetVolume(uint8_t vol);

void Mcry_Init(int32_t samplerate, const char* path);
void Mcry_Cleanup(void);
int32_t Mcry_IsReady(void);

void Mcry_Int(void);

#endif

