/*
 * filesystem_scan.h
 *
 *  Created on: 9 Feb 2026
 *      Author: fionn
 */

#ifndef FILESYSTEM_SCAN_H_
#define FILESYSTEM_SCAN_H_

#include <filesystem>
#include <vector>
#include <memory>

#include "database.h"
#include "types.h"


void scanDirectory(const std::filesystem::path& rootPath, const wchar_t* folderName, sqlite3*& db, int folderId);
void getFileDetails(const std::filesystem::path& rootPath, HANDLE& fileHandle, WIN32_FIND_DATAW& fileData, sqlite3*& db, int folderId, bool single_entry);

void openFolder(HANDLE hFile, std::filesystem::path fullPath);
void openFile(HANDLE hFile, std::filesystem::path fullPath);

void getVolumeFileIndex(const HANDLE hFile, DWORD& volumeSerial, uint64_t& fileIndex);


#endif /* FILESYSTEM_SCAN_H_ */
