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


/* for POSIX Kernel Time 1ns/unit */
uint64_t
Get_nsecCount(void)
{
#define	NSEC_PER_SEC	1000000000

  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t boottime_nsec = (ts.tv_sec * NSEC_PER_SEC) + (ts.tv_nsec);
  return(boottime_nsec);

}

/* for WIN-API compatible ms単位/49日周期 */
uint64_t
Get_msecCount(void)
{
#define	MSEC_PER_SEC	1000
#define	NSEC_PER_MSEC	1000000

  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t boottime_msec = (ts.tv_sec * MSEC_PER_SEC) + (ts.tv_nsec / NSEC_PER_MSEC);
  return(boottime_msec);
}

// SDL3 like for SDL2
// File-Load/Save/Free
void *
File_Load(char* filename,size_t* size) // 一気読込み
{
	FILE *fp;

	fp = fopen(filename, "rb");
	if (fp == NULL)
		return NULL;

    fseek(fp, 0, SEEK_END);//file容量を調べる
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

	char *buffer = (char *)malloc((size_t)fsize);//メモリ確保

	fsize = fread( buffer , sizeof(char) , (size_t)fsize , fp );
	fclose( fp );

	if (fsize <= 0){
		free(buffer);
		return NULL;
	}

	*size = fsize;

	return(buffer);
}

BOOL
File_Save(char* filename,void* dp, size_t size)//一気書込み
{
	FILE *fp;

	fp = fopen(filename, "wb");
	if (fp == NULL)
		return FALSE;

	uint32_t fsize = (uint32_t)fwrite( dp, 1,size, fp);

	fclose( fp );

   return(TRUE);
}

void
File_Free(void* fp)//一気解放
{
   free(fp);  
}



// old file util.
BOOL
ReadFile(void *h, void* buf, uint32_t len, uint32_t* lp, LPOVERLAPPED lpov)
{
	struct internal_file *fp;

	(void)lpov;

	if (h == (void*)INVALID_HANDLE_VALUE)
		return FALSE;

	fp = LocalLock(h);
	*lp = (uint32_t)read(fp->fd, buf, len);
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
	*lp = (uint32_t)write(fp->fd, buf, len);
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
	return (uint32_t)newpos;
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

