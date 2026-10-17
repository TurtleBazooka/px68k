/*	$Id: peace.h,v 1.1.1.1 2003/04/28 18:06:55 nonaka Exp $	*/

#ifndef	__NP2_PEACE_H__
#define	__NP2_PEACE_H__

#ifdef __cplusplus
extern "C" {
#endif

uint64_t	Get_nsecCount(void);
uint64_t	Get_msecCount(void);

void * File_Load(char* filename,size_t* size);
BOOL   File_Save(char* filename,void* dp, size_t size);
void   File_Free(void* fp);

BOOL	ReadFile(void*, void*, uint32_t, uint32_t*, LPOVERLAPPED);
BOOL	WriteFile(void*, const void* , uint32_t, uint32_t*, LPOVERLAPPED);
void*	CreateFile(const char*, uint32_t, uint32_t, LPSECURITY_ATTRIBUTES,
		uint32_t, uint32_t, void*);
uint32_t	SetFilePointer(void*, int32_t, int32_t*, uint32_t);
BOOL	FAKE_CloseHandle(void*);
int32_t	GetFileAttributes(const char*);

HLOCAL	LocalAlloc(uint32_t, uint32_t);
HLOCAL	LocalFreeX(HLOCAL);
void*	LocalLock(HLOCAL);
BOOL	LocalUnlock(HLOCAL);

HGLOBAL GlobalAlloc(uint32_t, uint32_t);
HGLOBAL	GlobalFree(HGLOBAL);
void*	GlobalLock(HGLOBAL);
BOOL	GlobalUnlock(HGLOBAL);
HGLOBAL	GlobalHandle(const void* p);

#ifdef __cplusplus
};
#endif

#endif	/* __NP2_PEACE_H__ */
