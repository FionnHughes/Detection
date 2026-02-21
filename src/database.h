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
int addFileEntry(sqlite3*& db, const std::unique_ptr<FSItem>& item, int folderId);
int addFolderEntry(sqlite3*& db, const std::unique_ptr<FSItem>& item);


#endif /* DATABASE_H_ */
