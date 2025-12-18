/* 
 * Copyright (c) 2003 NONAKA Kimihiro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include	"windows.h"
#include	"common.h"
#include	"dswin.h"
#include	"prop.h"
#include	"adpcm.h"
#include	"mercury.h"

#ifdef	YMFM
#include "ymfm_wrap.h"
#else
#include "fmg_wrap.h"
#endif

uint16_t	playing = FALSE;

#define PCMBUF_SIZE 48000*2
int16_t pcmbuffer[PCMBUF_SIZE];
int16_t *pcmbufp = pcmbuffer;
int16_t *pbsp = pcmbuffer;
int16_t *pbrp = pcmbuffer, *pbwp = pcmbuffer;
int16_t *pbep = &pcmbuffer[PCMBUF_SIZE];
uint32_t ratebase = 44100;
int32_t DSound_PreCounter = 0;
int16_t sdlsndbuf[PCMBUF_SIZE];

int32_t audio_fd = -1;

static void sdlaudio_callback(void *userdata, uint8_t *stream, int32_t len);

#ifndef NOSOUND

#include	"SDL2/SDL.h"
#include	"SDL2/SDL_audio.h"

//for SDL2
SDL_AudioDeviceID audio_dev;
SDL_AudioFormat deviceFormat;

static uint8_t sound_silence = 0;

int32_t
DSound_Init(uint32_t rate, uint32_t buflen)
{
	SDL_AudioSpec fmt; //要求format
	SDL_AudioSpec actfmt; // SDL返答format

	// Linuxは2倍(SDL1.2)、Android(SDL2.0)は4倍のlenでcallbackされた。
	// この値を小さくした方が音の遅延は少なくなるが負荷があがる
	uint32_t samples = 2048;

	if (playing) {
		return FALSE;
	}

	if (rate == 0) {
		audio_fd = -1;
		return TRUE;
	}

	ratebase = rate;

	memset(&fmt, 0, sizeof(fmt));

	fmt.freq = rate;
	fmt.channels = 2;
	fmt.samples = samples;
	fmt.callback = sdlaudio_callback;
	fmt.userdata = NULL;
	fmt.format = AUDIO_S16SYS;
	audio_dev = SDL_OpenAudioDevice(NULL, 0, &fmt, &actfmt, 0);//SDL2側で合わせてね
	if (audio_dev == 0) {
	  p6logd("SDL Audio open device error.\n");
	  return FALSE;
	}
	if(actfmt.size == 0){ //自動計算しない場合用(普通はありえない)
	 p6logd("SDL Audio open size error.\n");
	 return FALSE;
	}
	deviceFormat  = actfmt.format; //保存
	sound_silence = actfmt.silence;

	SDL_PauseAudioDevice(audio_dev, 0); //Start! SDL2

	audio_fd = 1; //flag
	playing = TRUE;

	pbrp=pbsp;
	pbwp=pbsp;

	return TRUE;
}

void
DSound_Play(void)
{
	if (audio_fd >= 0){
		ADPCM_SetVolume((uint8_t)Config.PCM_VOL);
		OPM_SetVolume((uint8_t)Config.OPM_VOL);
#ifndef	NO_MERCURY
		Mcry_SetVolume((uint8_t)Config.MCR_VOL);
#endif

		SDL_PauseAudioDevice(audio_dev,0);
	}
}

void
DSound_Stop(void)
{
	if (audio_fd >= 0){
		SDL_PauseAudioDevice(audio_dev,1);
	}
}

int32_t
DSound_Cleanup(void)
{
	playing = FALSE;

	if (audio_fd >= 0) {
		SDL_CloseAudioDevice(audio_dev);//SDL2/3
		audio_fd = -1;
	}
	return TRUE;
}

static void sound_send(int32_t length)
{

	SDL_LockAudioDevice(audio_dev);

	ADPCM_Update((int16_t *)pbwp, length, pbsp, pbep);
	OPM_Update  ((int16_t *)pbwp, length, pbsp, pbep);

#ifndef	NO_MERCURY
	//Mcry_Update((int16_t *)pbwp, length);
#endif

	pbwp += length * 2;// 2ch
	if (pbwp >= pbep) {
		pbwp = pbsp + (pbwp - pbep);
	}

	SDL_UnlockAudioDevice(audio_dev);

 return;
}

void FASTCALL DSound_Send0(int32_t clock)
{
	int32_t length = 0;
	int32_t rate;

	if (audio_fd < 0) {
		return;
	}

	DSound_PreCounter += (ratebase * clock);
	while (DSound_PreCounter >= 10000000L) {
		length++;
		DSound_PreCounter -= 10000000L;
	}
	if (length == 0) {
		return;
	}
	sound_send(length);
}

static void FASTCALL DSound_Send(int32_t length)
{
	int32_t rate;

	if (audio_fd < 0) {
		return;
	}
	sound_send(length);
}

static void
sdlaudio_callback(void *userdata, uint8_t *stream, int32_t len)
{
	int32_t lena, lenb, datalen, rate;
	int16_t *buf;

	// 実行時間測定用(デバック)
	//static uint32_t bef;
	//uint32_t now = timeGetTime();
	//p6logd("tdiff %4d : len %d ", now - bef, len);

	/* clear stream buffer for SDL2.(and SDL1) */
	SDL_memset(stream, sound_silence, len);// len はByte数

cb_start:
	if (pbrp <= pbwp) {
		// pcmbuffer
		// +---------+-------------+----------+
		// |         |/////////////|          |
		// +---------+-------------+----------+
		// A         A<--datalen-->A          A
		// |         |             |          |
		// pbsp     pbrp          pbwp       pbep

		datalen = pbwp - pbrp;
		if(datalen == 0){return;}
		if (datalen < (len / 2)) {
			// needs more data
			//DSound_Send((len - datalen) / 4);
			sound_send((len - datalen) / 4);
		}
#if 0
		datalen = pbwp - pbrp;
		if (datalen < (len / 2)) {
			printf("xxxxx not enough sound data xxxxx\n");
		}
#endif
		if (pbrp > pbwp) {
			// chage to TYPEC or TYPED
			goto cb_start;
		}

		buf = pbrp;
		pbrp += (len / 2);
		//printf("TYPEA: ");

	} else {
		// pcmbuffer
		// +---------+-------------+----------+
		// |/////////|             |//////////|
		// +------+--+-------------+----------+
		// <-lenb->  A             <---lena--->
		// A         |             A          A
		// |         |             |          |
		// pbsp     pbwp          pbrp       pbep

		lena = pbep - pbrp;
		if (lena >= (len / 2)) {
			buf = pbrp;
			pbrp += (len / 2);
			//printf("TYPEC: ");
		} else {
			lenb = (len / 2) - lena;
			if ((pbwp - pbsp) < lenb) {
				//DSound_Send((lenb - (pbwp - pbsp)) / 4);
				sound_send((lenb - (pbwp - pbsp)) / 2);
			}
#if 0
			if ((pbwp - pbsp) < lenb) {
				printf("xxxxx not enough sound data xxxxx\n");
			}
#endif
			if(lena != 0){ SDL_memcpy(sdlsndbuf, pbrp, lena * 2); }
			if(lenb != 0){ SDL_memcpy(&sdlsndbuf[lena], pbsp, lenb * 2); }
			buf = sdlsndbuf;
			pbrp = pbsp + lenb;
			//printf("TYPED: ");
		}
	}

	SDL_MixAudioFormat(stream, (uint8_t *)buf, deviceFormat, len, SDL_MIX_MAXVOLUME);

	//bef = now; //デバック用
}

#else	/* NOSOUND */
int32_t
DSound_Init(uint32_t rate, uint32_t buflen)
{
	return FALSE;
}

void
DSound_Play(void)
{
}

void
DSound_Stop(void)
{
}

int32_t
DSound_Cleanup(void)
{
	return TRUE;
}

void FASTCALL
DSound_Send0(int32_t clock)
{
}
#endif	/* !NOSOUND */
