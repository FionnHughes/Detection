/*
 * usn_journal.h
 *
 *  Created on: 9 Feb 2026
 *      Author: fionn
 */

#ifndef USN_JOURNAL_H_
#define USN_JOURNAL_H_

#include <windows.h>
#include <filesystem>
#include "sqlite3.h"
#include "types.h"

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


HANDLE openVolume(const std::wstring& volPath);

int queryJournal(HANDLE volume, USN_JOURNAL_DATA& journalData, DWORD& bytesReturned);

//void loadLastUsn(std::string fileName, DWORDLONG& lastUsn);

void saveLastUsn(std::string fileName, DWORDLONG& lastUsn);

int readJournalSince(HANDLE& volume, USN_JOURNAL_DATA& journal, DWORDLONG& lastUsn, const std::filesystem::path volPath, sqlite3*& db);

#endif /* USN_JOURNAL_H_ */
