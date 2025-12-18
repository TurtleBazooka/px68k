#include "common.h"

extern BOOL playing;

int32_t DSound_Init(uint32_t rate);
int32_t DSound_Cleanup(void);
void DSound_RateChange(uint32_t rate);

void DSound_Pause(void);
void DSound_Resume(void);

