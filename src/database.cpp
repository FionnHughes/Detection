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


int addUsnId(int usn_id, sqlite3*& db){
	const char* insertSql =
			"INSERT INTO Journal (id, usn_id) VALUES (1, ?)"
			"ON CONFLICT(id) DO UPDATE SET usn_id = excluded.usn_id;";
	sqlite3_stmt* stmt = nullptr;

	sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr);
	sqlite3_bind_int(stmt, 1, usn_id);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return usn_id;
}

int loadUsnId(int usn_id, sqlite3*& db){
	const char* selectSql =
			"SELECT usn_id FROM Journal WHERE id = ?";

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, selectSql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		cerr << "Select prepare failed: " << sqlite3_errmsg(db) << "\n";
		return -1;
	}
	sqlite3_bind_int(stmt, 1, 1);

	int id = -1;
	if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		id = sqlite3_column_int(stmt, 0);
	}
	if (id == -1){
		cout << "Journal USN not found\n";
		return addUsnId(usn_id, db);
	}
	return id;
}

int initDatabase(sqlite3*& db) {

	string sqlFiles =
			"CREATE TABLE IF NOT EXISTS Files("
			"file_id 			   INTEGER      PRIMARY KEY AUTOINCREMENT,"
			"folder_id             INTEGER,"
			"path 			       TEXT     	NOT NULL,"
			"filename	           TEXT     	NOT NULL,"
			"scan_time 			   INTEGER      DEFAULT (strftime('%s','now')),"
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
			"atime                 INTEGER      NOT NULL,"
			"mtime                 INTEGER      NOT NULL,"
			"attributes            INTEGER      NOT NULL,"
			"change                INTEGER      NOT NULL,"
			"blob_path             TEXT,"
			"compression           TEXT         NOT NULL,"
			"is_known_good         INTEGER      NOT NULL	DEFAULT 0,"
			"parent_snapshot_id    INTEGER,"
			"FOREIGN KEY (file_id) 				REFERENCES Files(file_id),"
			"FOREIGN KEY (parent_snapshot_id)	REFERENCES FileSnapshots(snapshot_id));";

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
			"created_at 		   INTEGER 		NOT NULL,"
			"atime                 INTEGER      NOT NULL,"
			"mtime                 INTEGER 		NOT NULL,"
			"attributes            INTEGER 		NOT NULL,"
			"change                INTEGER      NOT NULL,"
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

	string sqlUsnJournal =
			"CREATE TABLE IF NOT EXISTS Journal("
			"id INTEGER PRIMARY KEY CHECK (id = 1),"
			"usn_id INTEGER);";

	createTable(sqlUsnJournal,db);


	return 0;
}
void updateSnapshotCount(sqlite3*& db, int folderId){
	const char* updateCountSql =
		"UPDATE FolderSnapshots SET item_count = item_count + 1 "
		"WHERE folder_snapshot_id = (SELECT current_folder_snapshot_id FROM Folders WHERE folder_id = ?)";
	sqlite3_stmt* updateStmt = nullptr;

	int rc = sqlite3_prepare_v2(db, updateCountSql, -1, &updateStmt, nullptr);
	if (rc != SQLITE_OK) {
		cerr << "Update prepare failed: " << sqlite3_errmsg(db) << "\n";
	}
	else {
		sqlite3_bind_int(updateStmt, 1, folderId);
		rc = sqlite3_step(updateStmt);
		if (rc != SQLITE_DONE) {
			cerr << "Update failed: " << sqlite3_errmsg(db) << "\n";
		}
		sqlite3_finalize(updateStmt);
	}
}

int getCurrentFileId(sqlite3*& db, DWORD volumeSerial, uint64_t fileIndex){
	const char* selectSql =
			"SELECT file_id FROM Files WHERE volume_serial = ? AND file_index = ?";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, selectSql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		cerr << "Select prepare failed: " << sqlite3_errmsg(db) << "\n";
		return -1;
	}
	sqlite3_bind_int(stmt, 1, volumeSerial);
	sqlite3_bind_int(stmt, 2, fileIndex);

	int fileId = -1;
	if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		fileId = sqlite3_column_int(stmt, 0);
	}
	return fileId;
}

int addFileEntry(sqlite3*& db, const FSItem& item, int folderId){

	const char* insertSql =
			"INSERT INTO Files (folder_id, path, filename, volume_serial, file_index, current_snapshot_id)"
			"VALUES (?, ?, ?, ?, ?, ?)";

	sqlite3_stmt* stmt = nullptr;

	int rc = sqlite3_prepare_v2(db,insertSql, -1, &stmt, nullptr);

	if(rc != SQLITE_OK){
		cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
	}
	sqlite3_bind_int(stmt, 1, folderId);

	string pathStr = item.fullPath.string();
	sqlite3_bind_text(stmt, 2, pathStr.c_str(), -1, SQLITE_TRANSIENT);

	int len = WideCharToMultiByte(CP_UTF8, 0, item.fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
	string nameUtf8(len - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, item.fileName.c_str(), -1, nameUtf8.data(), len, nullptr, nullptr);

	sqlite3_bind_text(stmt, 3, nameUtf8.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 4, item.volumeSerial);
	sqlite3_bind_int(stmt, 5, item.fileIndex);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
	    std::cerr << "Insert failed: " << sqlite3_errmsg(db) << "\n";
	}


	sqlite3_finalize(stmt);

	updateSnapshotCount(db, folderId);

	return 0;
}

void addFileSnapshotEntry(sqlite3*& db, const FSItem& item, int fileId, int good){
	const char* insertSql =
			"INSERT INTO FileSnapshots (file_id, version_number, created_at, sha256, size_bytes, atime, mtime, attributes, change, blob_path, compression, is_known_good)"
			"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db,insertSql, -1, &stmt, nullptr);

	if(rc != SQLITE_OK){
		cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
	}
	sqlite3_bind_int(stmt, 1, fileId);
	sqlite3_bind_int(stmt, 2, 1);
	sqlite3_bind_int(stmt, 3, item.created_at);
	sqlite3_bind_text(stmt, 4, item.sha256.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 5, item.byteSize);
	sqlite3_bind_int(stmt, 6, item.atime);
	sqlite3_bind_int(stmt, 7, item.mtime);
	sqlite3_bind_int(stmt, 8, item.attributes);
	sqlite3_bind_int(stmt, 9, item.attributes);
	sqlite3_bind_text(stmt, 11, "zstd", -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 12, 1);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		std::cerr << "Insert failed: " << sqlite3_errmsg(db) << "\n";
	}
	sqlite3_finalize(stmt);

}


int getCurrentFolderId(sqlite3*& db, DWORD volumeSerial, uint64_t fileIndex){


	const char* selectSql =
			"SELECT folder_id FROM Folders WHERE volume_serial = ? AND folder_index = ?";
	sqlite3_stmt* stmt;

	int rc = sqlite3_prepare_v2(db, selectSql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		cerr << "Select prepare failed: " << sqlite3_errmsg(db) << "\n";
		return -1;
	}
	sqlite3_bind_int(stmt, 1, volumeSerial);
	sqlite3_bind_int(stmt, 2, fileIndex);

	int newFolderId = -1;
	if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		newFolderId = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);

	return newFolderId;
}




int addFolderEntry(sqlite3*& db, const FSItem& item, int folderId){
	const char* insertSql =
			"INSERT INTO Folders (path, filename, volume_serial, folder_index, parent_folder_id)"
			"VALUES (?, ?, ?, ?, ?)";

	sqlite3_stmt* stmt = nullptr;

	int rc = sqlite3_prepare_v2(db,insertSql, -1, &stmt, nullptr);

	if(rc != SQLITE_OK){
		cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
	}

	string pathStr = item.fullPath.string();
	sqlite3_bind_text(stmt, 1, pathStr.c_str(), -1, SQLITE_TRANSIENT);

	int len = WideCharToMultiByte(CP_UTF8, 0, item.fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
	string nameUtf8(len - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, item.fileName.c_str(), -1, nameUtf8.data(), len, nullptr, nullptr);

	sqlite3_bind_text(stmt, 2, nameUtf8.c_str(), -1, SQLITE_TRANSIENT);

	sqlite3_bind_int(stmt, 3, item.volumeSerial);
	sqlite3_bind_int(stmt, 4, item.fileIndex);
	sqlite3_bind_int(stmt, 5, folderId);


	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		std::cerr << "Insert failed: " << sqlite3_errmsg(db) << "\n";
	}
	//getting current folder id

	int newFolderId = getCurrentFolderId(db, item.volumeSerial, item.fileIndex);

	//getting folder_snapshot_id
	const char* selectSql =
			"SELECT folder_snapshot_id FROM FolderSnapshots WHERE folder_id = ?";
	stmt = nullptr;

	rc = sqlite3_prepare_v2(db, selectSql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		cerr << "Select prepare failed: " << sqlite3_errmsg(db) << "\n";
		return -1;
	}
	sqlite3_bind_int(stmt, 1, newFolderId);

	int folderSnapshotId = -1;
	if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		folderSnapshotId = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);

	const char* updateSql =
	    "UPDATE Folders SET current_folder_snapshot_id = ? WHERE folder_id = ?";

	stmt = nullptr;
	rc = sqlite3_prepare_v2(db,updateSql, -1, &stmt, nullptr);
	if(rc != SQLITE_OK){
		cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
	}

	sqlite3_bind_int(stmt, 1, folderSnapshotId);
	sqlite3_bind_int(stmt, 2, newFolderId);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		std::cerr << "Insert failed: " << sqlite3_errmsg(db) << "\n";
	}
	sqlite3_finalize(stmt);

	updateSnapshotCount(db, folderId);

	return newFolderId;
}

void addFolderSnapshotEntry(sqlite3*& db, const FSItem& item, int newFolderId, int good){
	const char* insertSql =
			"INSERT INTO FolderSnapshots (folder_id, version_number, created_at, atime, mtime, attributes, change, item_count, content_hash, is_known_good)"
			"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db,insertSql, -1, &stmt, nullptr);

	if(rc != SQLITE_OK){
		cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
	}
	sqlite3_bind_int(stmt, 1, newFolderId);
	sqlite3_bind_int(stmt, 2, 1);
	sqlite3_bind_int(stmt, 3, item.created_at);
	sqlite3_bind_int(stmt, 4, item.atime);
	sqlite3_bind_int(stmt, 5, item.mtime);
	sqlite3_bind_int(stmt, 6, item.attributes);

	//change item count and hash
	sqlite3_bind_int(stmt, 7, 0);
	sqlite3_bind_int(stmt, 8, 0);
	sqlite3_bind_text(stmt, 9, "HASH", -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 10, good);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		std::cerr << "Insert failed: " << sqlite3_errmsg(db) << "\n";
	}
	sqlite3_finalize(stmt);
}
