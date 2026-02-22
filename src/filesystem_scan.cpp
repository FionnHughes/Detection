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
#include <zstd.h>
#include <vector>

#include "filesystem_scan.h"
#include "state.h"

using namespace std;

constexpr size_t CHUNK_SIZE = 1024 * 1024;

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

bool compressFileZstdStream(const filesystem::path& srcPath, const filesystem::path& dstPath, int compressionLevel = 3) {
	ZSTD_CCtx* cctx = ZSTD_createCCtx();
	if (!cctx) {
	    // Handle error: Could not create compression context
	    return 1;
	}

	std::ifstream src(srcPath, std::ios::binary);
	if (!src) return false;
	std::ofstream dst(dstPath, std::ios::binary);
	if (!dst) return false;

	ZSTD_CStream* cstream = ZSTD_createCStream();
	if (!cstream) return false;
	if (ZSTD_isError(ZSTD_initCStream(cstream, compressionLevel))) {
		ZSTD_freeCStream(cstream);
		return false;
	}

	std::vector<char> inBuf(CHUNK_SIZE);
	std::vector<char> outBuf(ZSTD_CStreamOutSize());
	size_t toRead = inBuf.size();

	while (src) {
		src.read(inBuf.data(), toRead);
		std::streamsize inSize = src.gcount();

		ZSTD_inBuffer input = { inBuf.data(), static_cast<size_t>(inSize), 0 };
		while (input.pos < input.size) {
			ZSTD_outBuffer output = { outBuf.data(), outBuf.size(), 0 };
			size_t ret = ZSTD_compressStream(cstream, &output, &input);
			if (ZSTD_isError(ret)) {
				ZSTD_freeCStream(cstream);
				return false;
			}
			dst.write(outBuf.data(), output.pos);
		}
	}
	// FINALIZE: Flush and close stream
	int finished = 0;
	while (!finished) {
		ZSTD_outBuffer output = { outBuf.data(), outBuf.size(), 0 };
		size_t ret = ZSTD_endStream(cstream, &output);
		if (ZSTD_isError(ret)) {
			ZSTD_freeCStream(cstream);
			return false;
		}
		dst.write(outBuf.data(), output.pos);
		finished = (ret == 0);
	}
	ZSTD_freeCStream(cstream);
	return true;

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

HANDLE openFolder(filesystem::path fullPath){
	return CreateFileW(
			fullPath.c_str(),
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			nullptr
		);
}


HANDLE openFile(filesystem::path fullPath){
	return CreateFileW(
			fullPath.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
			nullptr
		);
}

void getVolumeFileIndex(const HANDLE hFile, DWORD& volumeSerial, uint64_t& fileIndex){
	BY_HANDLE_FILE_INFORMATION info;
		if (GetFileInformationByHandle(hFile, &info)) {
			volumeSerial = info.dwVolumeSerialNumber;
			uint64_t idx = (static_cast<uint64_t>(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
			fileIndex = idx;
		}
}


filesystem::path removePrefix(const filesystem::path& full, const filesystem::path& prefix) {
    auto fullIt   = full.begin();
    auto prefixIt = prefix.begin();

    while (fullIt != full.end() && prefixIt != prefix.end() && *fullIt == *prefixIt) {
        ++fullIt;
        ++prefixIt;
    }

    // prefix didn’t fully match
    if (prefixIt != prefix.end())
        return full;

    filesystem::path result;
    for (; fullIt != full.end(); ++fullIt)
        result /= *fullIt;

    return result;
}

void getFileDetails(const filesystem::path& rootPath, const filesystem::path& thisPath, HANDLE& fileHandle, WIN32_FIND_DATAW& fileData, sqlite3*& db, int folderId, int good, FSItem newItem){

	newItem.fileName = fileData.cFileName;

	//creating file path
	newItem.fullPath = thisPath / fileData.cFileName;

	//getting file attributes
	newItem.attributes = fileData.dwFileAttributes;

	if (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		newItem.type = ItemType::Symlink;
	else if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		newItem.type = ItemType::Directory;
	else
		newItem.type = ItemType::File;

	std::filesystem::path dstDir = "files";
	std::filesystem::path dstPath = dstDir / removePrefix(newItem.fullPath,rootPath);
	dstPath.replace_extension(".zst");

	filesystem::create_directory(dstPath.parent_path());


	if(newItem.type != ItemType::Symlink){
		HANDLE hFile;
		if(newItem.type == ItemType::File){
			hFile = openFile(newItem.fullPath);
			newItem.byteSize = (fileData.nFileSizeHigh * (MAXDWORD+1)) + fileData.nFileSizeLow;
			newItem.sha256 = sha256_file(newItem.fullPath);



			newItem.blobPath = dstPath;
			compressFileZstdStream(newItem.fullPath, dstPath);
		}

		else{
			hFile = openFolder(newItem.fullPath);
		}
		if(hFile == INVALID_HANDLE_VALUE){
			DWORD lastError = GetLastError();
			wcout << L"Error opening file. Error code: " << lastError << L" for " << newItem.fullPath << L"\n";
		}
		else{
			getVolumeFileIndex(hFile, newItem.volumeSerial, newItem.fileIndex);

			FILETIME created{}, accessed{}, written{};
			GetFileTime(hFile, &created, &accessed, &written);

			newItem.created_at = filetimeToUnix(created);
			newItem.atime = filetimeToUnix(accessed);
			newItem.mtime = filetimeToUnix(written);

			if(newItem.type == ItemType::File){
				file_count++;

				if(good){
					addFileEntry(db, newItem, folderId);
				}
				int fileId = getCurrentFileId(db, newItem.volumeSerial, newItem.fileIndex);
				addFileSnapshotEntry(db, newItem, fileId, good);
				if (good) {
					updateFileId(db, fileId);
				}

			}
			else{
				file_count++;
				if(good){
					folderId = addFolderEntry(db, newItem, folderId);
					scanDirectory(rootPath, newItem.fullPath, NULL, db, folderId);
				}
				addFolderSnapshotEntry(db, newItem, folderId, good);
				if (good) {
					updateFolderId(db, folderId);
				}

			}
			CloseHandle(hFile);
		}
	}
}

void scanDirectory(const filesystem::path& rootPath, const filesystem::path& thisPath, const wchar_t* folderName, sqlite3*& db, int folderId){


	HANDLE fileHandle;
	WIN32_FIND_DATAW fileData;

	filesystem::path search = thisPath / L"*";

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

		getFileDetails(rootPath, thisPath, fileHandle, fileData, db, folderId, 1);

	} while (FindNextFileW(fileHandle, &fileData));
	FindClose(fileHandle);
	//printDirectory(container);
	//cout << "*************************************\n";
}
