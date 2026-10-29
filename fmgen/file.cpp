//	$fmgen-Id: file.cpp,v 1.6 1999/12/28 11:14:05 cisc Exp $
//  Mod. for px68k by kameya 2026/10/13

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
//	ファイルを開く (OK:return TRUE)
// ---------------------------------------------------------------------------

bool FileIO::Open(const char* filename, uint32_t flg)
{
	BOOL ret = TRUE;

	strncpy(path, filename, MAX_PATH);

	if (flg & readonly) { hfile = fopen(filename , "rb"); }
	else{
	 if (flg & create ) {  hfile = fopen(filename , "rwb+"); }
	}

	flags = (flg & readonly) | (hfile == NULL ? 0 : open);

	if(hfile == NULL){
		error = file_not_found;
		ret = FALSE;
	}
	SetLogicalOrigin(0);

	return ret;

}

// ---------------------------------------------------------------------------
//	ファイルがない場合は作成
// ---------------------------------------------------------------------------

bool FileIO::CreateNew(const char* filename)
{

	return(Open(filename, create));
}

// ---------------------------------------------------------------------------
//	ファイルを作り直す
// ---------------------------------------------------------------------------
/*
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
*/
// ---------------------------------------------------------------------------
//	ファイルを閉じる
// ---------------------------------------------------------------------------

void FileIO::Close()
{
	if (GetFlags() & open)
	{
		if(hfile){
		 fclose(hfile);
		 hfile = NULL;
		}
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
	
	if (fread(dest, 1, size, hfile) != 0){
		return -1;
	}

	return size;
}

// ---------------------------------------------------------------------------
//	ファイルへの書き出し
// ---------------------------------------------------------------------------

int32_t FileIO::Write(const void* dest, int32_t size)
{
	if (!(GetFlags() & open) || (GetFlags() & readonly))
		return -1;
	
	int32_t writtensize = fwrite(dest, 1, size, hfile);
	if (writtensize != size){
		return -1;
	}

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
	case begin://先頭
		wmethod = SEEK_SET;
		break;
	case current://現在
		wmethod = SEEK_CUR; 
		break;
	case end: //最後
		wmethod = SEEK_END; 
		break;
	default:
		return false;
	}

	if(0 == fseek(hfile, pos, wmethod)){//Seek正常＝0
	 return TRUE;
	}

	return FALSE;
}

// ---------------------------------------------------------------------------
//	ファイルの位置を得る
// ---------------------------------------------------------------------------

int32_t FileIO::Tellp()
{
	if (!(GetFlags() & open))
		return 0;

	return ftell(hfile);
}

// ---------------------------------------------------------------------------
//	現在の位置をファイルの終端とする
// ---------------------------------------------------------------------------
/*
bool FileIO::SetEndOfFile()
{
	if (!(GetFlags() & open))
		return false;
	return ::SetEndOfFile(hfile) != 0;
}
*/
