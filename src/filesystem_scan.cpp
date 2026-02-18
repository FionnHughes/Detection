/*
 * filesystem_scan.cpp
 *
 *  Created on: 9 Feb 2026
 *      Author: fionn
 */

#include <windows.h>
#include <fileapi.h>
#include <openssl/evp.h>
#include <fstream>
#include <atomic>
#include <iostream>
#include <time.h>

#include "filesystem_scan.h"
#include "state.h"

using namespace std;

string sha256_file(const filesystem::path& path){
	ifstream file(path, ios::binary);
	if (!file) return "";

	EVP_MD_CTX* ctx = EVP_MD_CTX_new();
	if(!ctx) return "";

	if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
		EVP_MD_CTX_free(ctx);
		return "";
	}

	char buffer[8192];
	while(file.good()){
		file.read(buffer, sizeof(buffer));
		streamsize n = file.gcount();
		if (n > 0) {
			EVP_DigestUpdate(ctx, buffer, static_cast<size_t>(n));
		}
	}
	unsigned char hash[EVP_MAX_MD_SIZE];
	unsigned int len = 0;
	EVP_DigestFinal_ex(ctx, hash, &len);
	EVP_MD_CTX_free(ctx);

	stringstream ss;
	for(unsigned int i = 0; i < len; i++)
	{
		ss << hex << setw(2) << setfill('0') << static_cast<int>(hash[i]);
	}
	return ss.str();
}


void getAttributes(DWORD fileAttrs) {
	for( Attr attr : attrs){
		if (fileAttrs & attr.flag){
			wcout << attr.name << L" - ";
		}
	}
	wcout << L"\n";
}

int64_t filetimeToUnix(const FILETIME& ft){
	ULARGE_INTEGER ull;
	ull.HighPart = ft.dwHighDateTime;
	ull.LowPart = ft.dwLowDateTime;

	const int64_t EPOCH_DIFF = 116444736000000000LL;
	return (ull.QuadPart - EPOCH_DIFF) / 10000000LL;
}

void printDirectory(vector<unique_ptr<FSItem>>& items){
	for (auto& item : items){
		wstring typeStr;
		 switch (item->type) {
			case ItemType::File: typeStr = L"File"; break;
			case ItemType::Directory: typeStr = L"Directory"; break;
			case ItemType::Symlink: typeStr = L"Symlink"; break;
		}
		wcout << typeStr << L"\n";
		wcout << L"Name: " << item->fileName << L"\n";
		wcout << L"Path: " << item->fullPath << L"\n";
		wcout << L"Attributes: ";
		getAttributes(item->attributes);
		if (item->type == ItemType::File){
			wcout << L"File size: " << item->byteSize << L"\n";
		}
		wcout << L"\n";
	}
}

void scanDirectory(const filesystem::path& rootPath, const wchar_t* folderName, FSItem* parent, vector<unique_ptr<FSItem>>& container, sqlite3*& db, int folderId){


	HANDLE fileHandle;
	WIN32_FIND_DATAW fileData;

	filesystem::path search = rootPath / L"*";

	fileHandle = FindFirstFileW(search.c_str(), &fileData);

	if (INVALID_HANDLE_VALUE == fileHandle){
		wcerr << L"Failed to open directory.\n";
		return;
	}
	do
	{
		if (wcscmp(fileData.cFileName, L".") == 0
				|| wcscmp(fileData.cFileName, L"..") == 0
				|| (folderName && wcscmp(fileData.cFileName, folderName) != 0)) continue;

		auto newItem = make_unique<FSItem>();
		newItem->fileName = fileData.cFileName;

		//creating file path
		newItem->fullPath = rootPath / fileData.cFileName;

		//YYYY-MM-DD HH:MM:SS time format

		//for snapshots so is attributes kind of
		/*
		//getting time
		newItem->LastAccessTime = fileData.ftLastAccessTime;
		newItem->LastWriteTime = fileData.ftLastWriteTime;
		*/


		//getting file attributes
		newItem->attributes = fileData.dwFileAttributes;

		if (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
			newItem->type = ItemType::Symlink;
		else if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			newItem->type = ItemType::Directory;
		else
			newItem->type = ItemType::File;


		if(newItem->type != ItemType::Symlink){
			HANDLE hFile;
			if(newItem->type == ItemType::File){
				newItem->byteSize = (fileData.nFileSizeHigh * (MAXDWORD+1)) + fileData.nFileSizeLow;

				hFile = CreateFileW(
						newItem->fullPath.c_str(),
						GENERIC_READ,
						FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
						nullptr,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
						nullptr
					);
				newItem->sha256 = sha256_file(newItem->fullPath);

			}

			else{

				hFile = CreateFileW(
						newItem->fullPath.c_str(),
						FILE_LIST_DIRECTORY,
						FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
						nullptr,
						OPEN_EXISTING,
						FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
						nullptr
					);
			}
			if(hFile == INVALID_HANDLE_VALUE){
				DWORD lastError = GetLastError();
				wcout << L"Error opening file. Error code: " << lastError << L" for " << newItem->fullPath << L"\n";
			}
			else{
				BY_HANDLE_FILE_INFORMATION info;
				if (GetFileInformationByHandle(hFile, &info)) {
					newItem->volumeSerial = info.dwVolumeSerialNumber;
					uint64_t fileIndex = (static_cast<uint64_t>(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
					newItem->fileIndex = fileIndex;
				}
				FILETIME created{};
				GetFileTime(hFile, &created, nullptr, nullptr);

				newItem->created_at = filetimeToUnix(created);
				if(newItem->type == ItemType::File){
					file_count++;
					addFileEntry(db, newItem, folderId);
				}
				else{
					file_count++;
					int folderId = addFolderEntry(db, newItem);
					scanDirectory(newItem->fullPath, NULL, newItem.get(), newItem->children, db, folderId);
				}
				CloseHandle(hFile);
			}
		}
		container.push_back(move(newItem));

	} while (FindNextFileW(fileHandle, &fileData));
	FindClose(fileHandle);
	//printDirectory(container);
	//cout << "*************************************\n";
}
