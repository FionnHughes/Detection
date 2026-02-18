//============================================================================
// Name        : anomaly_detection.cpp
// Author      : Fionn Hughes
// Version     : 1.0
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================



#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>

#include "usn_journal.h"
#include "filesystem_scan.h"
#include "database.h"
#include "types.h"
#include "sqlite3.h"
#include "state.h"

using namespace std;
namespace fs = std::filesystem;


void showSpinner(){
	const char spinner[] = "|/-\\";
	int i = 0;
	while(!done){
		if(file_count % 1000 == 0){
			cout << "\rLoaded " << file_count << " files... " << spinner[i++ % 4] << flush;
			this_thread::sleep_for(chrono::milliseconds(100));
		}
	}
	cout << "\rLoading " << file_count <<" files done!      \n";
}

int main() {

	vector<unique_ptr<FSItem>> rootItems;
	fs::path targetPath = L"\\\\?\\D:\\Games";

	HANDLE volume = openVolume(L"\\\\.\\D:");

	if (volume == INVALID_HANDLE_VALUE)
		wcout << L"Error opening file. Error code: " << GetLastError() << L" for " << "\\\\.\\D:" << L"\n";

	USN_JOURNAL_DATA journal{};
	DWORD bytesReturned = 0;

	int queryOk = queryJournal(volume,journal,bytesReturned);
	if(!queryOk) return 1;

	DWORDLONG lastUsn = journal.NextUsn;

	loadLastUsn("LastUSN.txt",lastUsn);
	lastUsn = 156856088;

	readJournalSince(volume, journal, lastUsn, L"\\\\?\\D:\\Games\\SuperTux");

	saveLastUsn("LastUSN.txt",lastUsn);

	sqlite3* db = nullptr;

	openDB("data.db",db);

	initDatabase(db);

	thread spinner(showSpinner);
	scanDirectory(targetPath, L"SuperTux", nullptr, rootItems, db, -1);

	done = true;
	spinner.join();

	sqlite3_close(db);

	//printDirectory(rootItems);

	return 0;
}

