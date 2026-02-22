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
	int64_t atime;
	int64_t mtime;
	DWORD attributes;
	int64_t created_at;
	FSItem* parent = nullptr;
	DWORD volumeSerial;
	uint64_t fileIndex;

	//File Only
	int byteSize = 0;
	std::string sha256;

	DWORD change;
	std::filesystem::path blobPath;

	//Folder Only
	//fs::path parentPath;

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


#endif /* TYPES_H_ */
