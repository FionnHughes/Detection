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
#include "types.h"

HANDLE openVolume(const std::wstring& volPath);

int queryJournal(HANDLE volume, USN_JOURNAL_DATA& journalData, DWORD& bytesReturned);

//void loadLastUsn(std::string fileName, DWORDLONG& lastUsn);

void saveLastUsn(std::string fileName, DWORDLONG& lastUsn);

int readJournalSince(HANDLE& volume, USN_JOURNAL_DATA& journal, DWORDLONG& lastUsn, const std::filesystem::path volPath);

#endif /* USN_JOURNAL_H_ */
