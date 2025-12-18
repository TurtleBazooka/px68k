//
// 波形生成をStreamingで行います。(for SDL3)
// ・16bitPCM:ADPCM(fmgen&Mercury)
// ・32bitPCM:YM2151(YMFM)
//
//     2025/01/05 by Kameya

#include	"SDL3/SDL.h"
#include	"SDL3/SDL_audio.h"

#include	"windows.h"
#include	"common.h"
#include	"SoundCtrl.h"
#include	"prop.h"
#include	"adpcm.h"
#include	"mercury.h"

#ifdef YMFM
#include	"ymfm_wrap.h"
#else
#include	"fmg_wrap.h"
#endif

BOOL  playing  = FALSE;

// Audio Spec for SDL3
SDL_AudioSpec fmt_pc; //PC出力
SDL_AudioDeviceID audio_devPC;
SDL_AudioSpec fmt_x68_16; //X68 16bitAudio出力
SDL_AudioSpec fmt_x68_32; //X68 32bitAudio出力
SDL_AudioStream *stream16;
SDL_AudioStream *stream32;

// callback from SDL3 for streaming
static void sdlaudio_callback16(void *userdata, SDL_AudioStream *stream16, int len , int total);
static void sdlaudio_callback32(void *userdata, SDL_AudioStream *stream32, int len , int total);

//=== SDL3 Sound freq. Change ===
void
DSound_RateChange(uint32_t rate)
{
SDL_AudioSpec fmt32; //32bit spec
SDL_AudioSpec fmt16; //16bit spec
SDL_AudioSpec fmtPC; //PC    spec

  if (playing==FALSE){ return; }//初期化済？

  // Flush stream
  SDL_FlushAudioStream(stream16);
  SDL_FlushAudioStream(stream32);

  //32bit Stream1 Rate change
  if(TRUE == SDL_GetAudioStreamFormat(stream32, &fmt32, &fmtPC)){
   fmt32.freq = rate;
   SDL_SetAudioStreamFormat(stream32, &fmt32, &fmtPC);
  }

  //16bit Stream2 Rate change
  if(TRUE == SDL_GetAudioStreamFormat(stream16, &fmt16, &fmtPC)){
   fmt16.freq = rate;
   SDL_SetAudioStreamFormat(stream16, &fmt16, &fmtPC);
  }

  return;
}

//=== SDL3 Stream 生成 ===
int32_t
DSound_Init(uint32_t rate)
{
	if (playing==TRUE){ return FALSE; }//重複初期化対策

#ifdef NO_SOUND
	playing  = FALSE;
	return FALSE;
#endif	/*-NO_SOUND */

	if (rate == 0) //44100 or 22050 or 11025
	{
	   playing = FALSE;
	   return TRUE;
	}

	//PC 出力
	memset(&fmt_pc, 0, sizeof(fmt_pc));

	fmt_pc.freq = rate;
	fmt_pc.channels = 2;
	fmt_pc.format = SDL_AUDIO_S32LE;
	audio_devPC = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &fmt_pc);
	if (audio_devPC == 0) {
	    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_OpenAudioDevice() failed: %s\n", SDL_GetError());
	    return FALSE;
	}

	//ADPCM 出力(16bitPCM)
	memset(&fmt_x68_16, 0, sizeof(fmt_x68_16));

	fmt_x68_16.freq = rate;
	fmt_x68_16.channels = 2;
	fmt_x68_16.format = SDL_AUDIO_S16LE;
	stream16 = SDL_CreateAudioStream(&fmt_x68_16, &fmt_pc);
	if (stream16 == NULL) {
	    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateAudioStream() failed: %s\n", SDL_GetError());
	    SDL_CloseAudioDevice(audio_devPC);
	    return FALSE;
	}

	//FM 出力(32bitPCM)
	memset(&fmt_x68_32, 0, sizeof(fmt_x68_32));

	fmt_x68_32.freq = rate;
	fmt_x68_32.channels = 2;
	fmt_x68_32.format = SDL_AUDIO_S32LE;
	stream32 = SDL_CreateAudioStream(&fmt_x68_32, &fmt_pc);
	if (stream32 == NULL) {
	    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateAudioStream() failed: %s\n", SDL_GetError());
	    SDL_CloseAudioDevice(audio_devPC);
	    return FALSE;
	}

	//Streaming 1
	SDL_BindAudioStream(audio_devPC,stream32);
	SDL_SetAudioStreamGetCallback(stream32, sdlaudio_callback32, NULL);

	//Streaming 2
	SDL_BindAudioStream(audio_devPC,stream16);
	SDL_SetAudioStreamGetCallback(stream16, sdlaudio_callback16, NULL);

	playing = TRUE;

	return TRUE;
}

//=== SDL3 Streaming終了 ===
int32_t
DSound_Cleanup(void)
{
	if (playing == TRUE)
	{
	  SDL_CloseAudioDevice(audio_devPC);
	  SDL_DestroyAudioStream(stream16);
	  SDL_DestroyAudioStream(stream32);
	  playing = FALSE;
	}

  return TRUE;
}

//=== SDL3 音声出力一時停止 ===
void
DSound_Pause(void)
{
	if (playing == TRUE){
		SDL_PauseAudioDevice(audio_devPC);
	}

  return;
}

//=== SDL3 音声出力再開 ===
void
DSound_Resume(void)
{
	if (playing == TRUE)
	{
		// Volume再設定
		OPM_SetVolume((uint8_t)Config.OPM_VOL);
		ADPCM_SetVolume((uint8_t)Config.PCM_VOL);
#ifndef	NO_MERCURY
		Mcry_SetVolume((uint8_t)Config.MCR_VOL);
#endif

		SDL_ResumeAudioDevice(audio_devPC);
	}

  return;
}

//=== 16bit PCM CallBack ===
static void
sdlaudio_callback16(void *userdata, SDL_AudioStream *stream16, int len , int total)
{
	if (playing == FALSE) return;

	int16_t pcmbuffer[len + 2];

	//波形生成
	ADPCM_Update((int16_t *)pcmbuffer, len / 4, pcmbuffer, &pcmbuffer[len]);
#ifndef YMFM
	OPM_Update  ((int16_t *)pcmbuffer, len / 4, pcmbuffer, &pcmbuffer[len]);
#endif
#ifndef	NO_MERCURY
	Mcry_Update((int16_t *)pcmbuffer, len / 4);
#endif

	//Streaming!
	SDL_PutAudioStreamData(stream16, pcmbuffer, len );
	SDL_FlushAudioStream(stream16);

 return;
}

//=== 32bit PCM CallBack ===
static void
sdlaudio_callback32(void *userdata, SDL_AudioStream *stream32, int len , int total)
{
	if (playing == FALSE) return;

#ifdef YMFM
	int32_t pcmbuffer[len + 2];

	//YM2151波形生成
	OPM_Update  ((int32_t *)pcmbuffer, len / 4 );

	//Streaming!
	SDL_PutAudioStreamData(stream32, pcmbuffer, len );
	SDL_FlushAudioStream(stream32);
#endif

 return;
}
