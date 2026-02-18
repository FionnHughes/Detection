/*
 * types.h
 *
 *  Created on: 9 Feb 2026
 *      Author: fionn
 */

#ifndef TYPES_H_
#define TYPES_H_


#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <memory>



enum class ItemType {
    File,
    Directory,
    Symlink
};


struct FSItem {
	std::wstring fileName;
	ItemType type;
	std::filesystem::path fullPath;
	FILETIME LastAccessTime;
	FILETIME LastWriteTime;
	DWORD attributes;
	int64_t created_at;
	FSItem* parent = nullptr;
	DWORD volumeSerial;
	uint64_t fileIndex;

	//File Only
	int byteSize = 0;
	std::string sha256;

	//Folder Only
	//fs::path parentPath;

	std::vector<std::unique_ptr<FSItem>> children;

};

struct Attr{
	DWORD flag;
	std::wstring name;
};

static const Attr attrs[] = {
	{FILE_ATTRIBUTE_READONLY, L"READONLY"},
	{FILE_ATTRIBUTE_HIDDEN, L"HIDDEN"},
	{FILE_ATTRIBUTE_SYSTEM, L"SYSTEM"},
	{FILE_ATTRIBUTE_DIRECTORY, L"DIRECTORY"},
	{FILE_ATTRIBUTE_ARCHIVE, L"ARCHIVE"},
	{FILE_ATTRIBUTE_DEVICE, L"DEVICE"},
	{FILE_ATTRIBUTE_NORMAL, L"NORMAL"},
	{FILE_ATTRIBUTE_TEMPORARY, L"TEMPORARY"},
	{FILE_ATTRIBUTE_SPARSE_FILE, L"SPARSE_FILE"},
	{FILE_ATTRIBUTE_REPARSE_POINT, L"REPARSE_POINT"},
	{FILE_ATTRIBUTE_COMPRESSED, L"COMPRESSED"}
};

struct Reason {
    DWORD mask;
    const wchar_t* name;
};

static const Reason usnReasons[] = {
    { USN_REASON_DATA_OVERWRITE,       L"DATA_OVERWRITE" },
    { USN_REASON_DATA_EXTEND,          L"DATA_EXTEND" },
    { USN_REASON_DATA_TRUNCATION,      L"DATA_TRUNCATION" },
    { USN_REASON_FILE_CREATE,          L"FILE_CREATE" },
    { USN_REASON_FILE_DELETE,          L"FILE_DELETE" },
    { USN_REASON_RENAME_NEW_NAME,      L"RENAME_NEW_NAME" },
    { USN_REASON_RENAME_OLD_NAME,      L"RENAME_OLD_NAME" },
    { USN_REASON_BASIC_INFO_CHANGE,    L"BASIC_INFO_CHANGE" },
    { USN_REASON_ENCRYPTION_CHANGE,    L"ENCRYPTION_CHANGE" },
    { USN_REASON_REPARSE_POINT_CHANGE, L"REPARSE_POINT_CHANGE" }
};


#endif /* TYPES_H_ */
