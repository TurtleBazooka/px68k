//  2025/1/4  YMFM support by KAMEYA
//  - YM2151をYMFM生成に変更 16bitPCMから32bitPCMで波形生成
//  - SDL3のStreamingでADPCMとFM音源を別々に生成
//  - YMF288の実装は暫定

//C++をCから参照する場合の共通宣言
extern "C" {

#include "prop.h"
#include "mfp.h"
#include "fdc.h"
#include "adpcm.h"
#include "mercury.h"
#include "SoundCtrl.h"
#include "ymfm_wrap.h"

};

// ===== YMFM =====
#include "ymfm_opm.h"
#include "ymfm_opn.h"

// run this many dummy clocks of each chip before generating
#define EXTRA_CLOCKS (0)

// we use an int64_t as emulated time, as a 32.32 fixed point value
using emulated_time = int64_t;
emulated_time output_pos = 0;
emulated_time YM2151_output_step = 0x100000000ull / 44100;
int32_t ymfm_ym2151_vol;
uint8_t YM2151_reg;

// enumeration of the different types of chips we support
enum chip_type
{
	CHIP_YM2151,// OPM X68000
	CHIP_YM2203,// OPN
	CHIP_YM2608,// OPNA
	CHIP_YM2610,// OPNB
	CHIP_YM2612,// OPN2
	CHIP_YMF288,// OPN3L Mercury
	CHIP_TYPES
};

//  YMFM CLASSES
// abstract base class for a Yamaha chip; we keep a list of these for processing
// as new commands come in
class x68_chip_base
{
public:
	// construction
	x68_chip_base(uint32_t clock, chip_type type, char const *name) :
		m_type(type),
		m_name(name)
	{
	}

	// destruction
	virtual ~x68_chip_base()
	{
	}

	// simple getters
	chip_type type() const { return m_type; }
	virtual uint32_t sample_rate() const = 0;

	// required methods for derived classes to implement
	virtual void write(uint32_t reg, uint8_t data) = 0;
	virtual void generate(emulated_time output_start, emulated_time output_step, int32_t *buffer) = 0;

	// write data to the ADPCM-A buffer
	void write_data(ymfm::access_class type, uint32_t base, uint32_t length, uint8_t const *src)
	{
		uint32_t end = base + length;
		if (end > m_data[type].size())
			m_data[type].resize(end);
		memcpy(&m_data[type][base], src, length);
	}

	// seek within the PCM stream
	void seek_pcm(uint32_t pos) { m_pcm_offset = pos; }
	uint8_t read_pcm() { auto &pcm = m_data[ymfm::ACCESS_PCM]; return (m_pcm_offset < pcm.size()) ? pcm[m_pcm_offset++] : 0; }

protected:
	// internal state
	chip_type m_type;
	std::string m_name;
	std::vector<uint8_t> m_data[ymfm::ACCESS_CLASSES];
	uint32_t m_pcm_offset;

};

// ======================> x68_chip
template<typename ChipType>
class x68_chip : public x68_chip_base, public ymfm::ymfm_interface
{
public:
	// construction
	x68_chip(uint32_t clock, chip_type type, char const *name) :
		x68_chip_base(clock, type, name),
		m_chip(*this),
		m_clock(clock),
		m_clocks(0),
		m_step(0x100000000ull / m_chip.sample_rate(clock)),
		m_pos(0)
	{
		m_chip.reset();

		for (int clock = 0; clock < EXTRA_CLOCKS; clock++)
			m_chip.generate(&m_output);

	}

	virtual uint32_t sample_rate() const override
	{
		return m_chip.sample_rate(m_clock);
	}

	// handle a register write: just queue for now
	virtual void write(uint32_t reg, uint8_t data) override
	{
		m_queue.push_back(std::make_pair(reg, data));
	}

	// generate one output sample of output
	virtual void generate(emulated_time output_start, emulated_time output_step, int32_t *buffer) override
	{
		uint32_t addr1 = 0xffff, addr2 = 0xffff;
		uint8_t data1 = 0, data2 = 0;

		// see if there is data to be written; if so, extract it and dequeue
		if (!m_queue.empty())
		{
			auto front = m_queue.front();
			addr1 = 0 + 2 * ((front.first >> 8) & 3);
			data1 = front.first & 0xff;
			//addr2 = addr1 + ((m_type == CHIP_YM2149) ? 2 : 1);
			addr2 = addr1 + 1;
			data2 = front.second;
			m_queue.erase(m_queue.begin());
		}

		// write to the chip
		if (addr1 != 0xffff)
		{
			//p6logd("%10.5f: %s %03X=%02X\n", double(output_start) / double(1LL << 32), m_name.c_str(), data1 + 0x100 * (addr1/2), data2);
			m_chip.write(addr1, data1);
			m_chip.write(addr2, data2);
		}

		// generate at the appropriate sample rate
		for ( ; m_pos <= output_start; m_pos += m_step)
		{
			m_chip.generate(&m_output);
		}

		// add the final result to the buffer
		if (ChipType::OUTPUTS == 1)
		{
			*buffer++ += m_output.data[0];
			*buffer++ += m_output.data[0];
		}
		else
		{
			*buffer++ += m_output.data[0];
			*buffer++ += m_output.data[1 % ChipType::OUTPUTS];
		}
		m_clocks++;
	}

protected:
	// handle a read from the buffer
	virtual uint8_t ymfm_external_read(ymfm::access_class type, uint32_t offset) override
	{
		auto &data = m_data[type];
		return (offset < data.size()) ? data[offset] : 0;
	}

	// internal state
	ChipType m_chip;
	uint32_t m_clock;
	uint64_t m_clocks;
	typename ChipType::output_data m_output;
	emulated_time m_step;
	emulated_time m_pos;
	std::vector<std::pair<uint32_t, uint8_t>> m_queue;
};

// global list of active chips
std::vector<std::unique_ptr<x68_chip_base>> active_chips;

//-------------------------------------------------
//  add_chips - add 1 or 2 instances of the given
//  supported chip type
//-------------------------------------------------

template<typename ChipType>
void add_chips(uint32_t clock, chip_type type, char const *chipname)
{
	uint32_t clockval = clock & 0x3fffffff;
	int numchips = (clock & 0x40000000) ? 2 : 1;
	p6logd("Adding %s%s @ %dHz\n", (numchips == 2) ? "2 x " : "", chipname, clockval);
	for (int index = 0; index < numchips; index++)
	{
		char name[100];
		snprintf(name, sizeof(name) ,"%s #%d", chipname, index);
		active_chips.push_back(std::make_unique<x68_chip<ChipType>>(clockval, type, (numchips == 2) ? name : chipname));
	}

}

//-------------------------------------------------
//  find_chip - find the given chip and index
//-------------------------------------------------

x68_chip_base *find_chip(chip_type type, uint8_t index)
{
	for (auto &chip : active_chips)
		if (chip->type() == type && index-- == 0)
			return chip.get();
	return nullptr;
}

//-------------------------------------------------
//  write_chip - handle a write to the given chip
//  and index
//-------------------------------------------------

void write_chip(chip_type type, uint8_t index, uint32_t reg, uint8_t data)
{
	x68_chip_base *chip = find_chip(type, index);
	if (chip != nullptr){
		chip->write(reg, data);
	}
}


// YM2151 for X68000
// wave gen by YMFM
uint8_t  OPMReg;
uint32_t CurCount;

uint8_t	status;
uint8_t	TimerCntl;

uint8_t	TimerA_Reg[2];
int32_t	TimerA, TimerA_count;
int32_t	TimerB, TimerB_count;
int32_t	TimerWait;
int32_t	timer_step = 0;//初期値

// ====割り込み発生====
void Intr(bool f)
{
	if ( f ) ::MFP_Int(12);
}

// ===ステータスフラグ設定===
void SetStatus(uint8_t bits)
{
	if (!(status & bits))
	{
		status |= bits;
		Intr(true);
	}
}

// ===ステータスフラグ解除===
void ResetStatus(uint8_t bits)
{
	if (status & bits)
	{
		status &= ~bits;
		if (!status)
			Intr(false);
	}
}

// ===タイマー制御===
// [CSM][ - ][RST_B][RST_A][IRQ_B][IRG_A][LD_B][LD_A]
void SetTimerControl(uint8_t data)
{

	if (data & 0x10){//Reset TimerA flag 
		ResetStatus(0x01);
	}

	if (data & 0x20){//Reset TimerB Flag 
		ResetStatus(0x02);
	}


	if ((TimerCntl & 0x01) == 0x00){//0→1 edge TimerA Start
		if (data & 0x01) {
		  TimerA_count = TimerA;
		}
	}

	if ((TimerCntl & 0x02) == 0x00){//0→1 edge TimerB Start
		if (data & 0x02) {
		  TimerB_count = TimerB;
		}
	}

	TimerCntl = uint8_t(data);//保存

  return;
}

// ===タイマー A 発生時イベント (CSM) ===
void TimerA_CSM_event(void)
{

	for (uint8_t i=0; i<8; i++)
	{
		write_chip(CHIP_YM2151, 0, (uint32_t)0x08, (uint8_t)(0x78 | i));//All slot Key-ON
	}

  return;
}

// ===タイマー・カウント処理===
void Count(int32_t us)
{

	if((TimerA != 0) && (TimerCntl & 0x01)){//TimerA Running?
		TimerA_count -= us << 16;
		if (TimerA_count <= 0){//OverFlow check

		  if (TimerCntl & 0x80){ // CSM bit check
			TimerA_CSM_event();
		  }

		  while (TimerA_count < 0){
			TimerA_count += TimerA;
		  }

		  if (TimerCntl & 0x04){// IRQ flg check
				SetStatus(0x01);
		  }
		}
	}

	if((TimerB != 0) && (TimerCntl & 0x02)){//TimerB Running?
		TimerB_count -= us << 12;
		if (TimerB_count <= 0){//OverFlow check

		  while (TimerB_count < 0){
			TimerB_count += TimerB;
		  }

		  if (TimerCntl & 0x08){// IRQ flg check
			  SetStatus(0x02);
		  }
		}
	}

	return;
}

// === OPM YM2151 Initialize ===
// OPM 4MHz駆動 Samplingrate 11025/22050/44100/48000
void OPM_Init(int32_t clock, int32_t rate)
{
	if(timer_step == 0)//2重初期化禁止
	{
	 timer_step = int32_t(1000000. * 65536 / (clock / 64));

	 add_chips<ymfm::ym2151>(clock, CHIP_YM2151, "YM2151");
	 ymfm_ym2151_vol = 0;//volume
	}

	// freq. change.のみ
	YM2151_output_step = 0x100000000ull / rate;

	return;
}

// === OPM YM2151 Quit ===
void OPM_Cleanup(void)
{
	return;
}

// === OPM YM2151 change freq. ===
void OPM_SetRate(int32_t clock, int32_t rate)
{
	// freq. change.のみ
	YM2151_output_step = 0x100000000ull / rate;
}

// === OPM YM2151 Reset ===
void OPM_Reset(void)
{
	//ymfm::opm_registers.reset();

	// YM2151 all reg Reset
	for (uint16_t i=0x0; i<0x100; i++){
	 write_chip(CHIP_YM2151, 0, (uint8_t)i, 0);//YMFM clear
	}
	write_chip(CHIP_YM2151, 0, 0x19, 0x80);//PM LFO depth

	// Timer Reset
	OPMReg = 0;
	CurCount = 0;

	status = 0;
	TimerCntl = 0;

	TimerA_Reg[0] = 0;
	TimerA_Reg[1] = 0;

	TimerA = TimerB = 0;
	TimerA_count = 0;
	TimerB_count = 0;

  return;
}

// === OPM YM2151 Read Status ===
//[busy]-------[TimerB flag][TimerA flag]
uint8_t FASTCALL OPM_Read(void)
{
	uint8_t ret = (status & 0x03);

	// 0x80:Write Busy(Addr:4.25μs/data:20.75μs)
	if (TimerWait > 0)  ret |= 0x80;

	return ret;
}

// === OPM YM2151 Write ===
void FASTCALL OPM_Write(uint32_t adr, uint8_t data)
{
	uint16_t tmp;

	if( (adr & 0x01) == 0x00 ) {
		OPMReg = data;
		TimerWait=(4.24 * 10);//4.24μs
	}

	if( (adr & 0x01) == 0x01 ) {
		write_chip(CHIP_YM2151, 0, (uint32_t)OPMReg, (uint8_t)data);//YMFM
		switch (OPMReg & 0xff)// Timerは自前処理
		{
		  case 0x10:		// CLKA0
		  case 0x11:		// CLKA1
		    TimerA_Reg[OPMReg & 0x01] = uint8_t(data);
			tmp = (uint16_t)((TimerA_Reg[0] << 2) | (TimerA_Reg[1] & 0x03)) & 0x03ff;
			TimerA = (1024-tmp) * timer_step;
		    break;
		  case 0x12:		// CLKB
			TimerB = (256-data) * timer_step;
		    break;
		  case 0x14:		// CSM, TIMER
		    SetTimerControl(data);
		    break;
		  case 0x19:		// PMD, AMD
		    // none
		    break;
		  case 0x1B:		// ADPCM clock & FDC Ready
			::ADPCM_SetClock((data>>5) & 4);
			::FDC_SetForceReady((data>>6) & 1);
		    break;
		  default:
		    break;
		}
		TimerWait=(20.75 * 10);//20.75μs
	}

  return;
}

// === YM2151 Genarate Sound ===
void OPM_Update(int32_t *buffer, int32_t length )
{

	// YMFM callback (Streaming)
	for(int32_t i=0; i<length; i+=2)
	{
		int32_t outputs[2] = { 0 };
		for (auto &chip : active_chips)
			chip->generate(output_pos, YM2151_output_step, outputs);
		output_pos += YM2151_output_step;
		buffer[ i ] = outputs[0] * ymfm_ym2151_vol ;
		buffer[i+1] = outputs[1] * ymfm_ym2151_vol ;
	}

  return;
}

// === OPM YM2151 Timer ===
void FASTCALL OPM_Timer(uint32_t step)
{

	CurCount += step;
	Count(CurCount/10);//μs
	CurCount %= 10;

	if(TimerWait > 0) TimerWait -= (step);

}

// === OPM YM2151 Volume ===
void OPM_SetVolume(uint8_t vol)
{
	if ( vol>16 ) vol=16;// vol:0~16

	ymfm_ym2151_vol = (int32_t)vol * 1400;// YMFMはこのくらいかなぁ？

}

// +++++++++++++++++++++++++++++++++
// ++  YMF288 x2 (満開製 MK-MU1O)
// +++++++++++++++++++++++++++++++++

	int32_t YM288CurReg[2];
	uint32_t YM288CurCount;
	int32_t IntrFlag;


uint8_t YMF288_ReadIO(uint32_t adr)
{
	uint8_t ret = 0;
	if ( adr&1 ) {
		//ret = GetReg(((adr&2)?(CurReg[1]+0x100):CurReg[0]));
	} else {
		//ret = ((adr)?(ReadStatusEx()):(ReadStatus()));
	}
	return ret;
}


void YMF288_Intr(bool f)
{
	if ( (f)&&(IntrFlag) ) ::Mcry_Int();
}


void YMF288_Count2(uint32_t clock)
{
	//YM288CurCount += clock;
	//Count(YM288CurCount/10);
	//YM288CurCount %= 10;
}

void M288_Init(int32_t clock, int32_t rate, const char* path)
{

	add_chips<ymfm::ymf288>(clock, CHIP_YMF288, "YMF288");//ymfm
	add_chips<ymfm::ymf288>(clock, CHIP_YMF288, "YMF288");

	return;
}


void M288_Cleanup(void)
{
// none
}

void M288_Reset(void)
{
// none
}


uint8_t FASTCALL M288_Read(uint8_t adr)
{
uint8_t ret = 0x00;

/*
	if ( adr<=3 ) {
		if ( ymf288a )	ret = ymf288a->ReadIO(adr);
	} else {
		if ( ymf288b )	ret = ymf288b->ReadIO(adr&3);
	}
*/

return ret;
}

uint8_t adr_A288,adr_B288;
void FASTCALL M288_Write(uint8_t adr, uint8_t data)
{
	switch (adr & 0x03)
	{
	case 0x00: // #1 YM288 set Reg
	 adr_A288 = data;
	 break;
	case 0x01: // #1 YM288 set Parm
	 write_chip(CHIP_YMF288, 0, (uint8_t)adr_A288, (uint8_t)data);//ymfm
	 break;
	case 0x02: // #2 YM288 set Reg
	 adr_B288 = data;
	 break;
	case 0x03: // #2 YM288 set Parm.
	 write_chip(CHIP_YMF288, 1, (uint8_t)adr_B288, (uint8_t)data);//ymfm
	 break;
	default:
	 break;
	}

return;
}


void M288_Update(int16_t *buffer, int32_t length)
{
// none(YM2151と共通)
}


void FASTCALL M288_Timer(uint32_t step)
{
//	none(使ってる？)
}


void M288_SetVolume(uint8_t vol)
{
// none
}
