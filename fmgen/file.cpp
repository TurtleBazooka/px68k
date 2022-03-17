//	$fmgen-Id: file.cpp,v 1.6 1999/12/28 11:14:05 cisc Exp $

#include "common.h"

#include "headers.h"
#include "file.h"

// ---------------------------------------------------------------------------
//	構築/消滅
// ---------------------------------------------------------------------------

FileIO::FileIO()
{
	flags = 0;
}

FileIO::FileIO(const char* filename, uint32_t flg)
{
	flags = 0;
	Open(filename, flg);
}

FileIO::~FileIO()
{
	Close();
}

// ---------------------------------------------------------------------------
//	ファイルを開く
// ---------------------------------------------------------------------------

bool FileIO::Open(const char* filename, uint32_t flg)
{
	Close();

	strncpy(path, filename, MAX_PATH);

	uint32_t access = (flg & readonly ? 0 : GENERIC_WRITE) | GENERIC_READ;
	uint32_t share = (flg & readonly) ? FILE_SHARE_READ : 0;
	uint32_t creation = flg & create ? CREATE_ALWAYS : OPEN_EXISTING;

	hfile = CreateFile(filename, access, share, 0, creation, 0, 0);
	
	flags = (flg & readonly) | (hfile == INVALID_HANDLE_VALUE ? 0 : open);
	if (!(flags & open))
	{
		switch (FAKE_GetLastError())
		{
		case ERROR_FILE_NOT_FOUND:		error = file_not_found; break;
		case ERROR_SHARING_VIOLATION:	error = sharing_violation; break;
		default: error = unknown; break;
		}
	}
	SetLogicalOrigin(0);

	return !!(flags & open);
}

// ---------------------------------------------------------------------------
//	ファイルがない場合は作成
// ---------------------------------------------------------------------------

bool FileIO::CreateNew(const char* filename)
{
	Close();

	strncpy(path, filename, MAX_PATH);

	uint32_t access = GENERIC_WRITE | GENERIC_READ;
	uint32_t share = 0;
	uint32_t creation = CREATE_NEW;

	hfile = CreateFile(filename, access, share, 0, creation, 0, 0);
	
	flags = (hfile == INVALID_HANDLE_VALUE ? 0 : open);
	SetLogicalOrigin(0);

	return !!(flags & open);
}

// ---------------------------------------------------------------------------
//	ファイルを作り直す
// ---------------------------------------------------------------------------

bool FileIO::Reopen(uint32_t flg)
{
	if (!(flags & open)) return false;
	if ((flags & readonly) && (flg & create)) return false;

	if (flags & readonly) flg |= readonly;

	Close();

	uint32_t access = (flg & readonly ? 0 : GENERIC_WRITE) | GENERIC_READ;
	uint32_t share = flg & readonly ? FILE_SHARE_READ : 0;
	uint32_t creation = flg & create ? CREATE_ALWAYS : OPEN_EXISTING;

	hfile = CreateFile(path, access, share, 0, creation, 0, 0);
	
	flags = (flg & readonly) | (hfile == INVALID_HANDLE_VALUE ? 0 : open);
	SetLogicalOrigin(0);

	return !!(flags & open);
}

// ---------------------------------------------------------------------------
//	ファイルを閉じる
// ---------------------------------------------------------------------------

void FileIO::Close()
{
	if (GetFlags() & open)
	{
		FAKE_CloseHandle(hfile);
		flags = 0;
	}
}

// ---------------------------------------------------------------------------
//	ファイルからの読み出し
// ---------------------------------------------------------------------------

int32_t FileIO::Read(void* dest, int32_t size)
{
	if (!(GetFlags() & open))
		return -1;
	
	uint32_t readsize;
	if (!ReadFile(hfile, dest, size, &readsize, 0))
		return -1;
	return readsize;
}

// ---------------------------------------------------------------------------
//	ファイルへの書き出し
// ---------------------------------------------------------------------------

int32_t FileIO::Write(const void* dest, int32_t size)
{
	if (!(GetFlags() & open) || (GetFlags() & readonly))
		return -1;
	
	uint32_t writtensize;
	if (!WriteFile(hfile, dest, size, &writtensize, 0))
		return -1;
	return writtensize;
}

// ---------------------------------------------------------------------------
//	ファイルをシーク
// ---------------------------------------------------------------------------

bool FileIO::Seek(int32 pos, SeekMethod method)
{
	if (!(GetFlags() & open))
		return false;
	
	uint32_t wmethod;
	switch (method)
	{
	case begin:	
		wmethod = FILE_BEGIN; pos += lorigin; 
		break;
	case current:	
		wmethod = FILE_CURRENT; 
		break;
	case end:		
		wmethod = FILE_END; 
		break;
	default:
		return false;
	}

	return -1 != SetFilePointer(hfile, pos, 0, wmethod);
}

// ---------------------------------------------------------------------------
//	ファイルの位置を得る
// ---------------------------------------------------------------------------

int32_t FileIO::Tellp()
{
	if (!(GetFlags() & open))
		return 0;

	return SetFilePointer(hfile, 0, 0, FILE_CURRENT) - lorigin;
}

// ---------------------------------------------------------------------------
//	現在の位置をファイルの終端とする
// ---------------------------------------------------------------------------

bool FileIO::SetEndOfFile()
{
	if (!(GetFlags() & open))
		return false;
	return ::SetEndOfFile(hfile) != 0;
}
