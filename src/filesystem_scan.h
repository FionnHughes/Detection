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


void scanDirectory(const std::filesystem::path& rootPath, const wchar_t* folderName, FSItem* parent, std::vector<std::unique_ptr<FSItem>>& container, sqlite3*& db, int folderId);

#endif /* FILESYSTEM_SCAN_H_ */
