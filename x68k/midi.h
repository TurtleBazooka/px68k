
void MIDI_Init(void);
void MIDI_Reset(void);
void MIDI_SetModule(void);
void MIDI_Cleanup(void);

int32_t MIDI_Int(uint8_t irq);
void MIDI_Timer(int32_t clk);
void MIDI_DelayOut(uint32_t delay);
uint8_t MIDI_Read(uint32_t adr);
void MIDI_Write(uint32_t adr, uint8_t data);

extern uint32_t mid_outDevList();
extern uint32_t mid_inDevList();
extern void midOutChg(uint32_t mid_out_port, uint32_t bank);
extern void midInChg(uint32_t mid_in_port);

