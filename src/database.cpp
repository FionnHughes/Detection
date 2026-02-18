/*
 * database.cpp
 *
 *  Created on: 9 Feb 2026
 *      Author: fionn
 */
#include <typeinfo>
#include <iostream>

#include "database.h"


using namespace std;

void openDB(string fileName, sqlite3*& db){
	int exit = 0;
	exit = sqlite3_open(fileName.c_str(), &db);

	if (exit){ cerr << "Error open DB " << sqlite3_errmsg(db) << "\n"; }
	else{ cout << "Opened Database Successfully!" << "\n"; }
}

void createTable(string sqlCode, sqlite3*& db){
	char* messaggeError;

	int exit = sqlite3_exec(db, sqlCode.c_str(), NULL, 0, &messaggeError);

	if (exit != SQLITE_OK) {
		std::cerr << "Error Create Table: " << messaggeError << std::endl;
		sqlite3_free(messaggeError);
	}
	else
		std::cout << "Table created Successfully" << std::endl;
}

void dropAllTables(sqlite3*& db){
	const char* dropSql = "DROP TABLE IF EXISTS Files;";
	char* messageError = nullptr;

	int exit = sqlite3_exec(db, dropSql, nullptr, 0, &messageError);
	if (exit != SQLITE_OK) {
	    std::cerr << "Error dropping table: " << messageError << std::endl;
	    sqlite3_free(messageError);
	} else {
	    std::cout << "Table dropped successfully\n";
	}
	messageError = nullptr;

	dropSql = "DROP TABLE IF EXISTS Snapshots;";

	exit = sqlite3_exec(db, dropSql, nullptr, 0, &messageError);
	if (exit != SQLITE_OK) {
		std::cerr << "Error dropping table: " << messageError << std::endl;
		sqlite3_free(messageError);
	} else {
		std::cout << "Table dropped successfully\n";
	}
	messageError = nullptr;

	dropSql = "DROP TABLE IF EXISTS Folders;";

	exit = sqlite3_exec(db, dropSql, nullptr, 0, &messageError);
	if (exit != SQLITE_OK) {
		std::cerr << "Error dropping table: " << messageError << std::endl;
		sqlite3_free(messageError);
	} else {
		std::cout << "Table dropped successfully\n";
	}
	messageError = nullptr;

	dropSql = "DROP TABLE IF EXISTS FolderSnapshots;";

	exit = sqlite3_exec(db, dropSql, nullptr, 0, &messageError);
	if (exit != SQLITE_OK) {
		std::cerr << "Error dropping table: " << messageError << std::endl;
		sqlite3_free(messageError);
	} else {
		std::cout << "Table dropped successfully\n";
	}
	messageError = nullptr;

	dropSql = "DROP TABLE IF EXISTS Audits;";

	exit = sqlite3_exec(db, dropSql, nullptr, 0, &messageError);
	if (exit != SQLITE_OK) {
		std::cerr << "Error dropping table: " << messageError << std::endl;
		sqlite3_free(messageError);
	} else {
		std::cout << "Table dropped successfully\n";
	}
}

int initDatabase(sqlite3*& db) {

	dropAllTables(db);

	string sqlFiles =
			"CREATE TABLE IF NOT EXISTS Files("
			"file_id 			   INTEGER      PRIMARY KEY AUTOINCREMENT,"
			"folder_id             INTEGER,"
			"path 			       TEXT     	NOT NULL,"
			"filename	           TEXT     	NOT NULL,"
			"scan_time INTEGER DEFAULT (strftime('%s','now')),"
			"volume_serial         INTEGER      NOT NULL,"
			"file_index            INTEGER      NOT NULL,"
			"current_snapshot_id   INTEGER,"
			"UNIQUE(volume_serial, file_index),"
			"FOREIGN KEY (folder_id) 	REFERENCES Folders(folder_id),"
			"FOREIGN KEY (current_snapshot_id) 	REFERENCES FileSnapshots(snapshot_id));";

	createTable(sqlFiles,db);

	string sqlFileSnapshots =
			"CREATE TABLE IF NOT EXISTS FileSnapshots("
			"snapshot_id           INTEGER      PRIMARY KEY AUTOINCREMENT,"
			"file_id               INTEGER      NOT NULL,"
			"version_number        INTEGER      NOT NULL,"
			"created_at            INTEGER 	    DEFAULT (strftime('%s','now')),"
			"sha256                TEXT     	NOT NULL,"
			"size_bytes            INTEGER      NOT NULL,"
			"mtime                 INTEGER      NOT NULL,"
			"attributes            INTEGER      NOT NULL,"
			"content_blob          BLOB,"
			"blob_path             TEXT,"
			"is_known_good         INTEGER      NOT NULL	DEFAULT 0,"
			"parent_creator_id     INTEGER      NOT NULL,"
			"FOREIGN KEY (file_id) 				REFERENCES Files(file_id),"
			"FOREIGN KEY (parent_creator_id)	REFERENCES FileSnapshots(snapshot_id));";

	createTable(sqlFileSnapshots,db);

	string sqlFolders =
			"CREATE TABLE IF NOT EXISTS Folders("
			"folder_id             INTEGER      PRIMARY KEY AUTOINCREMENT,"
			"path 			       TEXT     	NOT NULL,"
			"filename	           TEXT     	NOT NULL,"
			"scan_time 			   INTEGER 		DEFAULT (strftime('%s','now')),"
			"volume_serial         INTEGER      NOT NULL,"
			"folder_index          INTEGER      NOT NULL,"
			"parent_folder_id      INTEGER,"
			"current_folder_snapshot_id   INTEGER,"
			"UNIQUE(volume_serial, folder_index),"
			"FOREIGN KEY (parent_folder_id) 	REFERENCES Folders(folder_id),"
			"FOREIGN KEY (current_folder_snapshot_id) 	REFERENCES FolderSnapshots(folder_snapshot_id));";

	createTable(sqlFolders,db);

	string sqlFolderSnapshots =
			"CREATE TABLE IF NOT EXISTS FolderSnapshots("
			"folder_snapshot_id    INTEGER 		PRIMARY KEY AUTOINCREMENT,"
			"folder_id             INTEGER 		NOT NULL,"
			"version_number        INTEGER 		NOT NULL,"
			"created_at 		   INTEGER 		DEFAULT (strftime('%s','now')),"
			"mtime                 INTEGER 		NOT NULL,"
			"attributes            INTEGER 		NOT NULL,"
			"item_count            INTEGER 		NOT NULL,"
			"content_hash          TEXT 		NOT NULL,"
			"parent_snapshot_id    INTEGER,"
			"is_known_good         INTEGER 		DEFAULT 0,"
			"FOREIGN KEY (folder_id) REFERENCES Folders(folder_id),"
			"FOREIGN KEY (parent_snapshot_id) REFERENCES FolderSnapshots(folder_snapshot_id));";

	createTable(sqlFolderSnapshots,db);

	string sqlAudits =
			"CREATE TABLE IF NOT EXISTS Audits("
			"audit_id              INTEGER      PRIMARY KEY AUTOINCREMENT,"
			"file_id               INTEGER,"
			"snapshot_id           INTEGER,"
			"folder_id             INTEGER,"
			"folder_snapshot_id    INTEGER,"
			"change_type           TEXT,"
			"anomoly_flags         TEXT,"
			"user_sid              TEXT,"
			"process_name          TEXT,"
			"FOREIGN KEY (file_id) REFERENCES Files(file_id),"
			"FOREIGN KEY (snapshot_id) REFERENCES FileSnapshots(snapshot_id),"
			"FOREIGN KEY (folder_id) REFERENCES Folders(folder_id),"
			"FOREIGN KEY (folder_snapshot_id) REFERENCES FolderSnapshots(folder_snapshot_id));";

	createTable(sqlAudits,db);

	return 0;
}
int addFileEntry(sqlite3*& db, const unique_ptr<FSItem>& item, int folderId){

	const char* insertSql =
			"INSERT OR IGNORE INTO Files (folder_id, path, filename, volume_serial, file_index, current_snapshot_id)"
			"VALUES (?, ?, ?, ?, ?, ?)";

	sqlite3_stmt* stmt = nullptr;

	int rc = sqlite3_prepare_v2(db,insertSql, -1, &stmt, nullptr);

	if(rc != SQLITE_OK){
		cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
	}
	sqlite3_bind_int(stmt, 1, folderId);

	string pathStr = item->fullPath.string();
	sqlite3_bind_text(stmt, 2, pathStr.c_str(), -1, SQLITE_TRANSIENT);

	int len = WideCharToMultiByte(CP_UTF8, 0, item->fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
	string nameUtf8(len - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, item->fileName.c_str(), -1, nameUtf8.data(), len, nullptr, nullptr);

	sqlite3_bind_text(stmt, 3, nameUtf8.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 4, item->volumeSerial);
	sqlite3_bind_int(stmt, 5, item->fileIndex);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
	    std::cerr << "Insert failed: " << sqlite3_errmsg(db) << "\n";
	}

	const char* selectSql =
			"SELECT folder_id FROM Folders WHERE volume_serial = ? AND folder_index = ?";
	rc = sqlite3_prepare_v2(db, selectSql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		cerr << "Select prepare failed: " << sqlite3_errmsg(db) << "\n";
		return -1;
	}
	sqlite3_bind_int(stmt, 1, item->volumeSerial);
	sqlite3_bind_int(stmt, 2, item->fileIndex);

	int fileId = -1;
	if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		fileId = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);
	/*

	insertSql =
			"INSERT OR IGNORE INTO FileSnapshots (file_id, version_number, created_at, sha256, size_bytes, mtime, attributes, content_blob, blob_path, is_known_good)"
			"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, DEFAULT)";
	stmt = nullptr;
	rc = sqlite3_prepare_v2(db,insertSql, -1, &stmt, nullptr);

	if(rc != SQLITE_OK){
		cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
	}
	sqlite3_bind_int(stmt, 1, fileId);
	sqlite3_bind_int(stmt, 2, 1);
	sqlite3_bind_int(stmt, 3, );

*/
	return 0;
}
int addFolderEntry(sqlite3*& db, const std::unique_ptr<FSItem>& item){
	const char* insertSql =
			"INSERT OR IGNORE INTO Folders (path, filename, volume_serial, folder_index, current_folder_snapshot_id)"
			"VALUES (?, ?, ?, ?, ?)";

	sqlite3_stmt* stmt = nullptr;

	int rc = sqlite3_prepare_v2(db,insertSql, -1, &stmt, nullptr);

	if(rc != SQLITE_OK){
		cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
	}

	string pathStr = item->fullPath.string();
	sqlite3_bind_text(stmt, 1, pathStr.c_str(), -1, SQLITE_TRANSIENT);

	int len = WideCharToMultiByte(CP_UTF8, 0, item->fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
	string nameUtf8(len - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, item->fileName.c_str(), -1, nameUtf8.data(), len, nullptr, nullptr);

	sqlite3_bind_text(stmt, 2, nameUtf8.c_str(), -1, SQLITE_TRANSIENT);

	sqlite3_bind_int(stmt, 3, item->volumeSerial);
	sqlite3_bind_int(stmt, 4, item->fileIndex);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		std::cerr << "Insert failed: " << sqlite3_errmsg(db) << "\n";
	}
	//getting current folder id

	const char* selectSql =
			"SELECT folder_id FROM Folders WHERE volume_serial = ? AND folder_index = ?";
	rc = sqlite3_prepare_v2(db, selectSql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		cerr << "Select prepare failed: " << sqlite3_errmsg(db) << "\n";
		return -1;
	}
	sqlite3_bind_int(stmt, 1, item->volumeSerial);
	sqlite3_bind_int(stmt, 2, item->fileIndex);

	int folderId = -1;
	if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		folderId = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);

	return folderId;
}



