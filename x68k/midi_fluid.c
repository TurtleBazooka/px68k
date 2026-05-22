/*
 fluidsynth利用のMIDI出力ルーチン群
 要 framework版/共有ライブラリ版libfluidsynth
 要 SoundFont2(Configで指定)
 libfluidsynth is under LGPLv2.1 licence なんだけど
 LINK(共有ライブラリ)するだけならその限りではないそうです。

 2022/5/29  by kameya (このソースはご自由にご利用ください)
*/
#include <fluidsynth.h>
#include <stdint.h>
#include "prop.h"
#include "winui.h"
#include "midi.h"

uint32_t midiOutShortMsg(HMIDIOUT , uint32_t );

	/*fluid set*/
    fluid_settings_t* settings;
    fluid_synth_t* synth;
    fluid_audio_driver_t* adriver;
	fluid_sequencer_t* sequencer;

	const char mid_name[] = {"px68k-MIDI"};
	const char synth_name[] = {"fluidsynth"};

#define sf_default	"/usr/local/share/soundfonts/default.sf2"

/*
  fluid_synth Open
  and Load SoundFont2 (Loadできないと音出ないよ)
*/
uint32_t
mid_outDevList(LPHMIDIOUT phmo)
{
	uint32_t Device_num = 0;

	/*==setting synth==*/
	settings = new_fluid_settings();
	if(!settings) return 0; // fluid error
	//fluid_settings_setstr(settings, "audio.driver", "dsound"); //win
	//fluid_settings_setstr(settings, "audio.driver", "coreaudio"); //mac
	//fluid_settings_setstr(settings, "audio.driver", "alsa"); //Linux
	fluid_settings_setint(settings, "synth.polyphony", 128);
	fluid_settings_setint(settings, "synth.reverb.active", FALSE);

    /*==Set in synthesizer ==*/
    synth = new_fluid_synth(settings);
    adriver = new_fluid_audio_driver(settings, synth);
	sequencer = new_fluid_sequencer2(0);

	/*++ Load SoundFont.sf2 ++*/
	int32_t fluid_res;
	if(Config.SoundFontFile[0]){
		fluid_res = fluid_synth_sfload(synth, (char*)Config.SoundFontFile, 1);
		if(fluid_res > 0){
		  p6logd("fluidsynth:Loading SoundFont %s OK\n",(char*)Config.SoundFontFile);
		}
	}
	else{
		fluid_res = fluid_synth_sfload(synth, sf_default, 1);
		if(fluid_res < 0){
		  p6logd("fluidsynth:Can't load SoundFont...\n");
		}
	}

	/* set menu */
	strcpy(menu_items[MIDI_OUT_menu_item][Device_num],synth_name);
	Device_num ++;
	strcpy(menu_items[MIDI_OUT_menu_item][Device_num],"\0"); /* Menu END */

	*phmo = (HANDLE)mid_name; /*MIDI Active!(ダミーを代入しておく)*/

 return 1;
}

/* No support */
uint32_t
mid_inDevList(LPHMIDIOUT phmo)
{
 return 0;
}

/*---------------------------------------------
   set/change MIDI out Port and select BANK
-----------------------------------------------*/
void midOutChg(uint32_t port_no, uint32_t bank)
{
	HMIDIOUT hmo;

	/* All note off */
	for (uint32_t msg=0x7bb0; msg<0x7bc0; msg++) {
		midiOutShortMsg(hmo, msg);
	}

	/* select BANK CC20 LSB */
	//uint32_t msg = ((bank << 16) | (0x20 << 8) | 0xb0);/*Bank select2*/
	//midiOutShortMsg(hmo, msg);

	return;
}

/*---------------------------------------------
   set/change MIDI in Port
-----------------------------------------------*/
void midInChg(uint32_t port_no)
{
	return;
}

/*---------------------------------------------
   MIDI Port close
-----------------------------------------------*/
uint32_t
midiOutClose(HMIDIOUT hmo)
{
	(void)hmo;

	if(adriver)
        delete_fluid_audio_driver(adriver);

    if(synth)
        delete_fluid_synth(synth);

    if(settings)
        delete_fluid_settings(settings);

	return MMSYSERR_NOERROR;
}

/*--------------------------------------------
   Send Short Message (演奏データ送信)
----------------------------------------------*/
uint32_t
midiOutShortMsg(HMIDIOUT hmo, uint32_t msg)
{
	uint8_t messg[4];
	int32_t val;

	/* (uint32)msg を 4byte に分解 */
	  messg[0] = (uint8_t)(msg & 0xff);//status byte
	  msg>>=8;
	  messg[1] = (uint8_t)(msg & 0xff);//note No.
	  msg>>=8;
	  messg[2] = (uint8_t)(msg & 0xff);//velocity

	/* Send msg */
	switch(messg[0] & 0xf0){
		case 0xc0://prog. chg
			fluid_synth_program_change(synth,messg[0]&0x0f, messg[1]);
			break;
		case 0xd0://chnnel press.
			fluid_synth_channel_pressure(synth,messg[0]&0x0f, messg[1]);
			break;
		case 0x80://note off (chan key vel)
			fluid_synth_noteoff(synth, messg[0]&0x0f, messg[1]);
			break;
		case 0x90://     on
			fluid_synth_noteon(synth, messg[0]&0x0f, messg[1], messg[2]);
			break;
		case 0xa0://key press.
			fluid_synth_key_pressure(synth, messg[0]&0x0f, messg[1], messg[2]);//fluid v1.xx not implement
			break;
		case 0xb0://cont.chg
			fluid_synth_cc(synth, messg[0]&0x0f, messg[1], messg[2]);
			break;
		case 0xe0://pitch wheel chg
			val = (messg[2] << 7 | messg[1]) - 2^13 ;
			fluid_synth_pitch_wheel_sens(synth, messg[0]&0x0f, val);
			break;
		case 0xf0://ないと思うけど
			break;
		default://ありえないハズ
			break;
	}

	return MMSYSERR_NOERROR;
}

/*---------------------------------------------
   Exclusive GO!  (設定データ送信)
-----------------------------------------------*/
uint32_t
midiOutLongMsg(HMIDIOUT hmo, LPMIDIHDR pmh, uint32_t cbmh)
{
	(void)hmo;
	(void)cbmh;

	fluid_synth_sysex(synth, (char *)pmh->lpData, pmh->dwBufferLength, NULL, NULL, NULL, 0);

	return MMSYSERR_NOERROR;
}

/*---Dummy--*/

uint32_t
midiOutUnprepareHeader(HMIDIOUT hmo, LPMIDIHDR pmh, uint32_t cbmh)
{
	(void)hmo;
	(void)pmh;
	(void)cbmh;
	return MMSYSERR_NOERROR;
}

uint32_t
midiOutPrepareHeader(HMIDIOUT hmo, LPMIDIHDR pmh, uint32_t cbmh)
{
	(void)hmo;
	(void)pmh;
	(void)cbmh;
	return MMSYSERR_NOERROR;
}

uint32_t
midiOutReset(HMIDIOUT hmo)
{
	(void)hmo;
	return MMSYSERR_NOERROR;
}

