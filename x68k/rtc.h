#ifndef _x68k_rtc
#define _x68k_rtc

void    RTC_Init(void);
uint8_t RTC_Read(uint32_t adr);
void    RTC_Write(uint32_t adr, uint8_t data);
int     RTC_Timer(int32_t clock);

#endif
