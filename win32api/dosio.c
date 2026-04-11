/*	$Id: dosio.c,v 1.2 2003/12/05 18:07:15 nonaka Exp $	*/

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

#include <sys/param.h>
#include <time.h>

#include "dosio.h"

static char		curpath[MAX_PATH+32] = "";
static char*	curfilep = curpath;

void
dosio_init(void)
{

	/* Nothing to do. */
}

void
dosio_term(void)
{

	/* Nothing to do. */
}

/* ファイル操作 */
FILEH
File_Open(char* filename)
{
	FILEH	ret;

	ret = CreateFile(filename, GENERIC_READ | GENERIC_WRITE,
	    0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (ret == (FILEH)INVALID_HANDLE_VALUE) {
		ret = CreateFile(filename, GENERIC_READ,
		    0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (ret == (FILEH)INVALID_HANDLE_VALUE)
			return (FILEH)FALSE;
	}
	return ret;
}

FILEH
File_Create(char* filename, int32_t ftype)
{
	FILEH	ret;

	(void)ftype;

	ret = CreateFile(filename, GENERIC_READ | GENERIC_WRITE,
	    0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (ret == (FILEH)INVALID_HANDLE_VALUE)
		return (FILEH)FALSE;
	return ret;
}

uint32_t
File_Seek(FILEH handle, uint32_t pointer, uint16_t mode)
{

	return SetFilePointer(handle, pointer, 0, mode);
}

uint32_t
File_Read(FILEH handle, void *data, uint32_t length)
{
	uint32_t	readsize;

	if (ReadFile(handle, data, length, &readsize, NULL) == 0)
		return 0;
	return readsize;
}

uint32_t
File_Write(FILEH handle, void *data, uint32_t length)
{
	uint32_t	writesize;

	if (WriteFile(handle, data, length, &writesize, NULL) == 0)
		return 0;
	return writesize;
}

int32_t
File_ZeroClr(FILEH handle, uint32_t length)
{
	char	buf[256];
	uint32_t	size;
	uint32_t	wsize;
	uint32_t	ret = 0;

	memset(buf, 0, sizeof(buf));
	while (length > 0) {
		wsize = (length >= sizeof(buf)) ? sizeof(buf) : length;

		size = File_Write(handle, buf, wsize);
		if (size == 0)
			return -1;

		ret += size;
		if (size != wsize)
			break;
		length -= wsize;
	}
	return ret;
}

uint16_t
File_LineRead(FILEH handle, void *data, uint16_t length)
{
	char*	p = (char*)data;
	uint32_t	readsize;
	uint32_t	pos;
	uint16_t	ret = 0;

	if ((length == 0) || ((pos = File_Seek(handle, 0, 1)) == -1))
		return 0;

	memset(data, 0, length);
	if (ReadFile(handle, data, length-1, &readsize, NULL) == 0)
		return 0;

	while (*p) {
		ret++;
		pos++;
		if ((*p == 0x0d) || (*p == 0x0a)) {
			break;
		}
		p++;
	}
	*p = '\0';

	File_Seek(handle, pos, 0);

	return ret;
}

int16_t
File_Close(FILEH handle)
{

	FAKE_CloseHandle(handle);
	return 0;
}

int16_t
File_Attr(char* filename)
{

	return (int16_t)GetFileAttributes(filename);
}


							// カレントファイル操作
void
File_Setcd(char* exename)
{

	strncpy(curpath, exename, sizeof(curpath));
	plusyen(curpath, sizeof(curpath));
	curfilep = curpath + strlen(exename) + 1;
	*curfilep = '\0';
}

char*
File_Getcd(char* filename)
{

	strncpy(curfilep, filename, MAX_PATH - (curfilep - curpath));
	return curpath;
}

FILEH
File_OpenCurDir(char* filename)
{

	strncpy(curfilep, filename, MAX_PATH - (curfilep - curpath));
	return File_Open(curpath);
}

FILEH
File_CreateCurDir(char* filename, int32_t ftype)
{

	strncpy(curfilep, filename, MAX_PATH - (curfilep - curpath));
	return File_Create(curpath, ftype);
}

int16_t
File_AttrCurDir(char* filename)
{

	strncpy(curfilep, filename, MAX_PATH - (curfilep - curpath));
	return File_Attr(curpath);
}

int32_t
File_GetFType(char* filename)
{

	(void)filename;

	return FTYPE_NONE;
}


char*
getFileName(char* filename)
{
	char* p;
	char* q;

	for (p = q = filename; *p != '\0'; p++)
		if (*p == '/')
			q = p + 1;
	return q;
}

void
cutFileName(char* filename)
{
	char* p;
	char* q;

	for (p = filename, q = NULL; *p != '\0'; p++)
		if (*p == '/')
			q = p + 1;
	if (q != NULL)
		*q = '\0';
}

char*
getExtName(char* filename)
{
	char*	p;
	char*	q;

	p = getFileName(filename);
	q = NULL;

	while (*p != '\0') {
		if (*p == '.')
			q = p + 1;
		p++;
	}
	if (q == NULL)
		q = p;
	return q;
}

void
cutExtName(char* filename)
{
	char*	p;
	char*	q;

	p = getFileName(filename);
	q = NULL;

	while (*p != '\0') {
		if (*p == '.')
			q = p;
		p++;
	}
	if (q != NULL)
		*q = '\0';
}

int32_t
kanji1st(char* str, int32_t pos)
{
	int32_t	ret = 0;
	uint8_t	c;

	for (; pos > 0; pos--) {
		c = (uint8_t)str[pos];
		if (!((0x81 <= c && c <= 0x9f) || (0xe0 <= c && c <= 0xfc)))
			break;
		ret ^= 1;
	}
	return ret;
}

int32_t
kanji2nd(char* str, int32_t pos)
{
	int32_t	ret = 0;
	uint8_t	c;

	while (pos-- > 0) {
		c = (uint8_t)str[pos];
		if (!((0x81 <= c && c <= 0x9f) || (0xe0 <= c && c <= 0xfc)))
			break;
		ret ^= 1;
	}
	return ret;
}


int32_t
ex_a2i(char* str, int32_t min, int32_t max)
{
	int32_t	ret = 0;
	char	c;

	if (str == NULL)
		return(min);

	for (;;) {
		c = *str++;
		if (c == ' ')
			continue;
		if ((c < '0') || (c > '9'))
			break;
		ret = ret * 10 + (c - '0');
	}

	if (ret < min)
		return min;
	else if (ret > max)
		return max;
	return ret;
}

void
cutyen(char* str)
{
	int32_t pos = (int32_t)strlen(str) - 1;

	if ((pos > 0) && (str[pos] == '/'))
		str[pos] = '\0';
}

void
plusyen(char* str, int32_t len)
{
	int32_t pos = (int32_t)strlen(str);

	if (pos) {
		if (str[pos-1] == '/')
			return;
	}
	if ((pos + 2) >= len)
		return;
	str[pos++] = '/';
	str[pos] = '\0';
}


void
fname_mix(char* str, char* mix, int32_t size)
{
	char* p;
	int32_t len;
	char c;
	char check;

	cutFileName(str);
	if (mix[0] == '/')
		str[0] = '\0';

	len = (int32_t)strlen(str);
	p = str + len;
	check = '.';
	while (len < size) {
		c = *mix++;
		if (c == '\0')
			break;

		if (c == check) {
			/* current dir */
			if (mix[0] == '/') {
				mix++;
				continue;
			}
			/* parent dir */
			if (mix[0] == '.' && mix[1] == '/') {
				mix += 2;
				cutyen(str);
				cutFileName(str);
				len = (int32_t)strlen(str);
				p = str + len;
				continue;
			}
		}
		if (c == '/')
			check = '.';
		else
			check = 0;
		*p++ = c;
		len++;
	}
	if (p < str + len)
		*p = '\0';
	else
		str[len - 1] = '\0';
}

/*
 * UNIX -> DOS 日時変換
 */
/* $NetBSD: msdosfs_conv.c,v 1.29 2001/01/18 20:28:27 jdolecek Exp $ */
/*-
 * Copyright (C) 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1995, 1997 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
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
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

/*
 * Days in each month in a regular year.
 */
uint16_t const regyear[] = {
	31, 28, 31, 30, 31, 30,
	31, 31, 30, 31, 30, 31
};

/*
 * Days in each month in a leap year.
 */
uint16_t const leapyear[] = {
	31, 29, 31, 30, 31, 30,
	31, 31, 30, 31, 30, 31
};

/*
 * Variables used to remember parts of the last time conversion.  Maybe we
 * can avoid a full conversion.
 */
static time_t lasttime;
static uint32_t lastday;
static uint16_t lastddate;
static uint16_t lastdtime;

/*
 * Convert the unix version of time to dos's idea of time to be used in
 * file timestamps. The passed in unix time is assumed to be in GMT.
 */
void
unix2dostime(time_t t, uint16_t *ddp, uint16_t *dtp, uint8_t *dhp)
{
	time_t tt;
	uint32_t days;
	uint32_t inc;
	uint32_t year;
	uint32_t month;
	const uint16_t *months;

	/*
	 * If the time from the last conversion is the same as now, then
	 * skip the computations and use the saved result.
	 */
	tt = t;
	t &= ~1;
	if (lasttime != t) {
		lasttime = t;
		lastdtime = (((t / 2) % 30) << DT_2SECONDS_SHIFT)
		    + (((t / 60) % 60) << DT_MINUTES_SHIFT)
		    + (((t / 3600) % 24) << DT_HOURS_SHIFT);

		/*
		 * If the number of days since 1970 is the same as the last
		 * time we did the computation then skip all this leap year
		 * and month stuff.
		 */
		days = (uint32_t)(t / (24 * 60 * 60));
		if (days != lastday) {
			lastday = days;
			for (year = 1970;; year++) {
				inc = year & 0x03 ? 365 : 366;
				if (days < inc)
					break;
				days -= inc;
			}
			months = year & 0x03 ? regyear : leapyear;
			for (month = 0; month < 12; month++) {
				if (days < months[month])
					break;
				days -= months[month];
			}
			lastddate = ((days + 1) << DD_DAY_SHIFT)
			    + ((month + 1) << DD_MONTH_SHIFT);
			/*
			 * Remember dos's idea of time is relative to 1980.
			 * unix's is relative to 1970.  If somehow we get a
			 * time before 1980 then don't give totally crazy
			 * results.
			 */
			if (year > 1980)
				lastddate += (year - 1980) << DD_YEAR_SHIFT;
		}
	}
	if (dtp)
		*dtp = lastdtime;
	if (dhp)
		*dhp = (tt & 1) * 100;

	*ddp = lastddate;
}

/*
 * The number of seconds between Jan 1, 1970 and Jan 1, 1980. In that
 * interval there were 8 regular years and 2 leap years.
 */
#define	SECONDSTO1980	(((8 * 365) + (2 * 366)) * (24 * 60 * 60))

static uint16_t lastdosdate;
static uint32_t lastseconds;

/*
 * Convert from dos' idea of time to unix'. This will probably only be
 * called from the stat(), and fstat() system calls and so probably need
 * not be too efficient.
 */
void
dos2unixtime(uint32_t dd, uint32_t dt, uint32_t dh, time_t *tp)
{
	uint32_t seconds;
	uint32_t m, month;
	uint32_t y, year;
	uint32_t days;
	const uint16_t *months;

	if (dd == 0) {
		/*
		 * Uninitialized field, return the epoch.
		 */
		tp = 0;
		return;
	}
	seconds = ((dt & DT_2SECONDS_MASK) >> DT_2SECONDS_SHIFT) * 2
	    + ((dt & DT_MINUTES_MASK) >> DT_MINUTES_SHIFT) * 60
	    + ((dt & DT_HOURS_MASK) >> DT_HOURS_SHIFT) * 3600
	    + dh / 100;
	/*
	 * If the year, month, and day from the last conversion are the
	 * same then use the saved value.
	 */
	if (lastdosdate != dd) {
		lastdosdate = dd;
		days = 0;
		year = (dd & DD_YEAR_MASK) >> DD_YEAR_SHIFT;
		for (y = 0; y < year; y++)
			days += y & 0x03 ? 365 : 366;
		months = year & 0x03 ? regyear : leapyear;
		/*
		 * Prevent going from 0 to 0xffffffff in the following
		 * loop.
		 */
		month = (dd & DD_MONTH_MASK) >> DD_MONTH_SHIFT;
		if (month == 0) {
			//printf("dos2unixtime(): month value out of range (%ld)\n",month);
			month = 1;
		}
		for (m = 0; m < month - 1; m++)
			days += months[m];
		days += ((dd & DD_DAY_MASK) >> DD_DAY_SHIFT) - 1;
		lastseconds = (days * 24 * 60 * 60) + SECONDSTO1980;
	}
	*tp = seconds + lastseconds;
}
