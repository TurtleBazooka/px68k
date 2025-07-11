/*
 * Copyright 2000 Masaru OKI
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
 * 3. The name of the author may not be used to endorse or promote products
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
#include <sys/types.h>
#include <time.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "windows.h"


/*-----
 *
 * From PEACE - http://chiharu.hauN.org/peace/ja/
 *
 */
enum {
	HTYPE_MEMORY = 0x100,
	HTYPE_HEAP,
	HTYPE_PROCESS,
	HTYPE_THREAD,
	HTYPE_MODULE,
	HTYPE_FILE,
	HTYPE_FILEMAPPING,
	HTYPE_SEMAPHORE,
	HTYPE_MUTEX,
	HTYPE_EVENT,
	HTYPE_HWND,
	HTYPE_MENU,
#if 0
	HTYPE_HDC,
	HTYPE_BMP,
	HTYPE_ICON,
	HTYPE_FONT,
	HTYPE_PALETTE,
	HTYPE_PEN,
	HTYPE_BRUSH,
	HTYPE_RGN,
	HTYPE_METAFILE,
#endif
	HTYPE_CONSOLE,
	HTYPE_KEY,
};

struct internal_handle {
	void		*p;
	uint32_t	flags;
	size_t		psize;
	uint32_t	refcount;
	int			type;
};

struct internal_file {
	int	fd;
};

#define ptrtohandle(h)							\
    ((struct internal_handle *)((void *)(h) - sizeof(struct internal_handle)))

#define isfixed(h)							\
    ((ptrtohandle(h)->p == (h)) ? 1 : 0)

#define sethandletype(h,t)						\
    do {								\
        if (isfixed(h))							\
	    ptrtohandle(h)->type = (t);					\
	else								\
	    (((struct internal_handle *)(h))->type = (t));		\
    } while (/* CONSTCOND */0)

#define handletype(h)							\
    ((isfixed(h)) ?							\
        ptrtohandle(h)->type : (((struct internal_handle *)(h))->type))


/* for POSIX Kernel Time 1μs/unit */
uint32_t
Get_usecCount(void)
{
#define	USEC_PER_SEC	1000000
#define	NSEC_PER_USEC	1000

  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t boottime_usec = (ts.tv_sec * USEC_PER_SEC) + (ts.tv_nsec / NSEC_PER_USEC);
  return((uint32_t)(boottime_usec & 0xffffffff));

}

/* for WIN-API compatible ms単位/49日周期 */
uint32_t
Get_msecCount(void)
{
#define	MSEC_PER_SEC	1000
#define	NSEC_PER_MSEC	1000000

  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t boottime_msec = (ts.tv_sec * MSEC_PER_SEC) + (ts.tv_nsec / NSEC_PER_MSEC);
  return((uint32_t)(boottime_msec & 0xffffffff));
}

BOOL
ReadFile(void *h, void* buf, uint32_t len, uint32_t* lp, LPOVERLAPPED lpov)
{
	struct internal_file *fp;

	(void)lpov;

	if (h == (void*)INVALID_HANDLE_VALUE)
		return FALSE;

	fp = LocalLock(h);
	*lp = read(fp->fd, buf, len);
	LocalUnlock(h);
	if (*lp <= 0)
		return FALSE;
	return TRUE;
}

BOOL
WriteFile(void *h, const void* buf, uint32_t len, uint32_t* lp, LPOVERLAPPED lpov)
{
	struct internal_file *fp;

	(void)lpov;

	if (h == (void *)INVALID_HANDLE_VALUE)
		return FALSE;

	fp = LocalLock(h);
	*lp = write(fp->fd, buf, len);
	LocalUnlock(h);
	if (*lp <= 0)
		return FALSE;
	return TRUE;
}

void *
CreateFile(const char* filename, uint32_t rdwr, uint32_t share,
	    LPSECURITY_ATTRIBUTES sap,
	    uint32_t crmode, uint32_t flags, void *template)
{
	struct internal_file *fp;
	void *h;
	int32_t fd, fmode = 0;

	(void)share;
	(void)sap;
	(void)flags;
	(void)template;
#ifdef _WIN32
 	fmode |=O_BINARY;
#endif
	switch (rdwr & (GENERIC_READ|GENERIC_WRITE)) {
	case GENERIC_READ:
		fmode |= O_RDONLY;
		break;
	case GENERIC_WRITE:
		fmode |= O_WRONLY;
		break;
	case GENERIC_READ|GENERIC_WRITE:
		fmode |= O_RDWR;
	default:
		break;
	}
	switch (crmode) {
	case CREATE_ALWAYS:
		fmode |= O_CREAT;
		break;	
	case OPEN_EXISTING:
		break;
	}
	fd = open(filename, fmode, 0644);
	if (fd < 0)
		return (void *)INVALID_HANDLE_VALUE;

	h = LocalAlloc(0, sizeof(struct internal_file));
	sethandletype(h, HTYPE_FILE);
	fp = LocalLock(h);
	fp->fd = fd;
	LocalUnlock(h);
	return h;
}

uint32_t
SetFilePointer(void *h, int32_t pos, int32_t* newposh, uint32_t whence)
{
	struct internal_file *fp;
	off_t newpos;
	int32_t fd;

	(void)newposh;

	fp = LocalLock(h);
	fd = fp->fd;
	LocalUnlock(h);
	newpos = lseek(fd, (int)pos, whence);
	return newpos;
}

BOOL
FAKE_CloseHandle(void *h)
{
	switch (handletype(h)) {
	case HTYPE_FILE:
	    {
		struct internal_file *fp;

		fp = LocalLock(h);
		close(fp->fd);
		LocalUnlock(h);
	    }
		break;
	default:
		return FALSE;
	}
	LocalFreeX(h);
	return TRUE;
}

int32_t
GetFileAttributes(const char* path)
{
	struct stat sb;
	int32_t attr = FILE_ATTRIBUTE_NORMAL;

	if (stat(path, &sb) == 0) {
		if (S_ISDIR(sb.st_mode))
			return FILE_ATTRIBUTE_DIRECTORY;
		if (!(sb.st_mode & S_IWUSR))
			attr |= FILE_ATTRIBUTE_READONLY;
		return attr;
	}

	return -1;
}

HLOCAL
LocalAlloc(uint32_t flags, uint32_t bytes)
{

	return GlobalAlloc(flags, bytes);
}

HLOCAL
LocalFreeX(HLOCAL h)
{

	return GlobalFree(h);
}

void*
LocalLock(HLOCAL h)
{

	return GlobalLock(h);
}

BOOL
LocalUnlock(HLOCAL h)
{

	return GlobalUnlock(h);
}

HGLOBAL
GlobalAlloc(uint32_t flags, uint32_t bytes)
{
	struct internal_handle *p;

	(void)flags;

	p = malloc(bytes + sizeof(struct internal_handle));
	if (p != 0) {
		p->p = &p[1];
		p->psize = bytes;
		p->refcount = 0;
		p->type = HTYPE_MEMORY;
		return p->p;
	}
	return 0;
}

HGLOBAL
GlobalFree(HGLOBAL h)
{
	struct internal_handle *ih = h;

	if (h == 0)
		return 0;
	if (!isfixed(h)) {
		free(ih);
		return 0;
	}
	ih = GlobalHandle(h);
	if (ih->p == &ih[1]) {
		free(ih);
		return 0;
	}

	return h;
}

HGLOBAL
GlobalHandle(const void* p)
{

	return (HGLOBAL)(p - sizeof(struct internal_handle));
}

void*
GlobalLock(HGLOBAL h)
{
	struct internal_handle *ih = h;

	if (isfixed(h))
		return h;
	ih->refcount++;

	return ih->p;
}

BOOL
GlobalUnlock(HGLOBAL h)
{
	struct internal_handle *ih = h;

	if (isfixed(h) || ih->refcount == 0)
		return FALSE;

	if (--ih->refcount != 0) {
		/* still locked */
		return TRUE;
	}

	/* unlocked */
	return FALSE;
}

uint32_t
GetPrivateProfileString(const char* sect, const char* key, const char* defvalue,
			 char* buf, uint32_t len, const char* inifile)
{
	char lbuf[256];
	FILE *fp;

	if (sect == NULL
	 || key == NULL
	 || defvalue == NULL
	 || buf == NULL
	 || len == 0
	 || inifile == NULL)
		return 0;

	memset(buf, 0, len);

	fp = fopen(inifile, "rb");
	if (fp == NULL)
		goto nofile;
	while (!feof(fp)) {
		fgets(lbuf, sizeof(lbuf), fp);
		/* XXX should be case insensitive */
		if (lbuf[0] == '['
		    && !strncasecmp(sect, &lbuf[1], strlen(sect))
		    && lbuf[strlen(sect) + 1] == ']')
			break;
	}
	if (feof(fp))
		goto notfound;
	while (!feof(fp)) {
		fgets(lbuf, sizeof(lbuf), fp);
		if (lbuf[0] == '[' && strchr(lbuf, ']'))
			goto notfound;
		/* XXX should be case insensitive */
		if (!strncasecmp(key, lbuf, strlen(key))
		    && lbuf[strlen(key)] == '=') {
			char *dst, *src;
			src = &lbuf[strlen(key) + 1];
			dst = buf;
			while (*src != '\r' && *src != '\n' && *src != '\0')
				*dst++ = *src++;
			*dst = '\0';
			fclose(fp);
			return strlen(buf);
		}
	}
notfound:
	// printf(("[%s]:%s not found\n", sect, key));
	fclose(fp);
nofile:
	strncpy(buf, defvalue, len);
	/* not include nul */
	return strlen(buf);
}

uint32_t
GetPrivateProfileInt(const char* sect, const char* key, int32_t defvalue, const char* inifile)
{
	char lbuf[256];
	FILE *fp;

	if (sect == NULL
	 || key == NULL
	 || inifile == NULL)
		return 0;

	fp = fopen(inifile, "rb");
	if (fp == NULL)
		goto nofile;
	while (!feof(fp)) {
		fgets((char *)lbuf, sizeof(lbuf), fp);
		/* XXX should be case insensitive */
		if (lbuf[0] == '['
		    && !strncasecmp(sect, (char *)&lbuf[1], strlen(sect))
		    && lbuf[strlen(sect) + 1] == ']')
			break;
	}
	if (feof(fp))
		goto notfound;
	while (!feof(fp)) {
		fgets((char *)lbuf, sizeof(lbuf), fp);
		if (lbuf[0] == '[' && strchr((char *)lbuf, ']'))
			goto notfound;
		/* XXX should be case insensitive */
		if (!strncasecmp(key, (char *)lbuf, strlen(key))
		    && (char)lbuf[strlen(key)] == (char)'=') {
			int32_t value;
			sscanf((char *)&lbuf[strlen(key) + 1], "%d", &value);
			fclose(fp);
			return value;
		}
	}
notfound:
	fclose(fp);
nofile:
	return defvalue;
}
