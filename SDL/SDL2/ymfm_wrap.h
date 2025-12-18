// for YM2151 
// and YMF288

void OPM_Init(int32_t clock, int32_t rate);
void OPM_Cleanup(void);
void OPM_SetRate(int32_t clock, int32_t rate);
void OPM_Reset(void);
void OPM_Update(int16_t *buffer, int32_t length, int16_t *pbsp, int16_t *pbep);
void FASTCALL OPM_Write(uint32_t r, uint8_t v);
uint8_t FASTCALL OPM_Read(uint32_t adr);
void FASTCALL OPM_Timer(uint32_t step);
void OPM_SetVolume(uint8_t vol);

void M288_Init(int32_t clock, int32_t rate, const char* path);
void M288_Cleanup(void);
void M288_Reset(void);
void M288_Update(int16_t *buffer, int32_t length);
void FASTCALL M288_Write(uint8_t r, uint8_t v);
uint8_t FASTCALL M288_Read(uint8_t a);
void FASTCALL M288_Timer(uint32_t step);
void M288_SetVolume(uint8_t vol);

