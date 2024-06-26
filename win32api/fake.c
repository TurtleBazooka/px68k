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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
 *      This product includes software developed by NONAKA Kimihiro.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "windows.h"
//#include "mmsystem.h"

int32_t
MessageBox(int32_t hWnd, const char* str, const char* title, uint32_t flags)
{

	(void)hWnd;
	(void)flags;

	printf("----- %s\n", title);
	printf("%s\n", str);
	printf("-----\n\n");

	return 0;
}

void
PostQuitMessage(int32_t m)
{

	exit(m);
}

uint32_t
FAKE_GetLastError(void)
{

	return NO_ERROR;
}

BOOL
SetEndOfFile(void *hFile)
{

	(void)hFile;

	return FALSE;
}
/*
WINMMAPI MMRESULT
midiOutPrepareHeader(HMIDIOUT hmo, LPMIDIHDR pmh, uint32_t cbmh)
{

	(void)hmo;
	(void)pmh;
	(void)cbmh;
	return !MIDIERR_STILLPLAYING;	// (ぉ
}

WINMMAPI MMRESULT
midiOutUnprepareHeader(HMIDIOUT hmo, LPMIDIHDR pmh, uint32_t cbmh)
{

	(void)hmo;
	(void)pmh;
	(void)cbmh;
	return MMSYSERR_NOERROR;
}

WINMMAPI MMRESULT
midiOutShortMsg(HMIDIOUT hmo, uint32_t dwMsg)
{

	(void)hmo;
	(void)dwMsg;
	return MMSYSERR_NOERROR;
}

WINMMAPI MMRESULT
midiOutLongMsg(HMIDIOUT hmo, LPMIDIHDR pmh, uint32_t cbmh)
{

	(void)hmo;
	(void)pmh;
	(void)cbmh;
	return MMSYSERR_NOERROR;
}

WINMMAPI MMRESULT
midiOutOpen(LPHMIDIOUT phmo, uint32_t uDeviceID, uint32_t dwCallback,
    uint32_t dwInstance, uint32_t fdwOpen)
{

	(void)phmo;
	(void)uDeviceID;
	(void)dwCallback;
	(void)dwInstance;
	(void)fdwOpen;
	return !MMSYSERR_NOERROR;	// (ぃ
}

WINMMAPI MMRESULT
midiOutClose(HMIDIOUT hmo)
{

	(void)hmo;
	return MMSYSERR_NOERROR;
}

WINMMAPI MMRESULT
midiOutReset(HMIDIOUT hmo)
{

	(void)hmo;
	return MMSYSERR_NOERROR;
}
*/
static int32_t _WritePrivateProfileString_subr(
			FILE **, int32_t, int32_t, const char*, const char*);

BOOL
WritePrivateProfileString(const char* sect, const char* key, const char* str, const char* inifile)
{
	char lbuf[256];
	char newbuf[256];
	struct stat sb;
	FILE *fp;
	int32_t pos;
	int32_t found = 0;
	int32_t notfound = 0;
	int32_t delta;

	if (stat(inifile, &sb) == 0)
		fp = fopen(inifile, "rb+");
	else
		fp = fopen(inifile, "wb+");
	if (!fp)
		return FALSE;

	while (!feof(fp)) {
		fgets(lbuf, sizeof(lbuf), fp);
		/* XXX should be case insensitive */
		if (lbuf[0] == '['
		    && !strncasecmp(sect, &lbuf[1], strlen(sect))
		    && lbuf[strlen(sect) + 1] == ']') {
			found = 1;
			break;
		}
	}
	if (feof(fp) && !found) {
		/*
		 * Now create new section and key.
		 */
		rewind(fp);
		snprintf(newbuf, sizeof(newbuf), "[%s]\n", sect);
		if (fwrite(newbuf, strlen(newbuf), 1, fp) < 1)
			goto writefail;
		snprintf(newbuf, sizeof(newbuf), "%s=%s\n", key, str);
		if (fwrite(newbuf, strlen(newbuf), 1, fp) < 1) 
			goto writefail;
		fclose(fp);
		return TRUE;
	}

	pos = 0;	/* gcc happy */
	found = 0;
	while (!feof(fp)) {
		pos = ftell(fp);
		fgets(lbuf, sizeof(lbuf), fp);
		if (lbuf[0] == '[' && strchr(lbuf, ']')) {
			notfound = 1;
			break;
		}
		/* XXX should be case insensitive */
		if (!strncasecmp(key, lbuf, strlen(key))
		    && lbuf[strlen(key)] == '=') {
			found = 1;
			snprintf(newbuf, sizeof(newbuf), "%s=%s\n", key, str);
			delta = strlen(newbuf) - strlen(lbuf);
			if (delta == 0) {
				if (!strncasecmp(newbuf, lbuf, strlen(newbuf)))
					break;
				/* overwrite */
				fseek(fp, pos, SEEK_SET);
				if (fwrite(newbuf, strlen(newbuf), 1, fp) < 1)
					goto writefail;
			} else if (delta > 0) {
				if (!_WritePrivateProfileString_subr(&fp, pos,
				    ftell(fp), newbuf, inifile))
					goto writefail;
			} else {
				if (!_WritePrivateProfileString_subr(&fp, pos,
				    ftell(fp), newbuf, inifile))
					goto writefail;
			}
			break;
		}
	}
	if (feof(fp) && !found) {
		/*
		 * Now create new key.
		 */
		fseek(fp, 0L, SEEK_END);
		snprintf(newbuf, sizeof(newbuf), "%s=%s\n", key, str);
		if (fwrite(newbuf, strlen(newbuf), 1, fp) < 1)
			goto writefail;
	}
	else if (notfound) {
		snprintf(newbuf, sizeof(newbuf), "%s=%s\n", key, str);
		if (!_WritePrivateProfileString_subr(&fp, pos,
		    ftell(fp), newbuf, inifile))
			goto writefail;
	}

	fclose(fp);
	return TRUE;

writefail:
	fclose(fp);
	return FALSE;
}

/*
 * XXX: REWRITE ME!!!
 */
static int32_t
_WritePrivateProfileString_subr(FILE **fp, int32_t pos, int32_t nowpos,
		const char* buf, const char* file)
{
	struct stat sb;
	char *p;

	if (stat(file, &sb) < 0)
		return 1;

	p = (char *)malloc(sb.st_size);
	if (!p)
		return 1;
	rewind(*fp);
	if (fread(p, sb.st_size, 1, *fp) < 1)
		goto out;
	fclose(*fp);

	*fp = fopen(file, "wb+");
	if (*fp == NULL)
		goto out;
	if (fwrite(p, pos, 1, *fp) < 1)
		goto out;
	if (fwrite(buf, strlen(buf), 1, *fp) < 1)
		goto out;
	if (sb.st_size - nowpos > 0)
		if (fwrite(p + nowpos, sb.st_size - nowpos, 1, *fp) < 1)
			goto out;
	free(p);
	return 0;

out:
	free(p);
	return 1;
}
