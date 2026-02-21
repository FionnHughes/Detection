/*
 * database.h
 *
 *  Created on: 9 Feb 2026
 *      Author: fionn
 */

#ifndef DATABASE_H_
#define DATABASE_H_

#include <string>
#include "sqlite3.h"
#include "types.h"

void openDB(std::string fileName, sqlite3*& db);
int initDatabase(sqlite3*& db);
int loadUsnId(int usn_id, sqlite3*& db);
int addUsnId(int usn_id, sqlite3*& db);
int addFileEntry(sqlite3*& db, const FSItem& item, int folderId);
int getCurrentFileId(sqlite3*& db, DWORD volumeSerial, uint64_t fileIndex);
void addFileSnapshotEntry(sqlite3*& db, const FSItem& item, int fileId, int good);

void updateFolderCount(sqlite3*& db, int folderId);
int addFolderEntry(sqlite3*& db, const FSItem& item, int folderId);
void addFolderSnapshotEntry(sqlite3*& db, const FSItem& item, int newFolderId, int good);
int getCurrentFolderId(sqlite3*& db, DWORD volumeSerial, uint64_t fileIndex);

#endif /* DATABASE_H_ */
