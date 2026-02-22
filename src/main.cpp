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
#include <shobjidl.h>

#include "usn_journal.h"
#include "filesystem_scan.h"
#include "database.h"
#include "types.h"
#include "sqlite3.h"
#include "state.h"

using namespace std;


bool fileExists (const string& name) {
    if (FILE *file = fopen(name.c_str(), "r")) {
        fclose(file);
        return true;
    } else {
        return false;
    }
}

wstring GetVolumePath(filesystem::path& path){
	filesystem::path root = path.root_name();
	if (root.empty()) return L"";

	return L"\\\\.\\" + root.wstring();   // "\\.\D:"

}

filesystem::path OpenFolderPicker()
{
    IFileDialog* pfd = nullptr;
    wstring result;

    HRESULT hr = CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pfd)
    );

    if (SUCCEEDED(hr))
    {
        DWORD options;
        pfd->GetOptions(&options);
        pfd->SetOptions(options | FOS_PICKFOLDERS);

        if (SUCCEEDED(pfd->Show(nullptr)))
        {
            IShellItem* item;
            if (SUCCEEDED(pfd->GetResult(&item)))
            {
            	PWSTR path = nullptr;
				if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
				{
					result = filesystem::path(path);   // <-- convert to filesystem::path
					CoTaskMemFree(path);
				}
				item->Release();
            }
        }
        pfd->Release();
    }
    return result;
}


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

bool isFirstRun() {
    HKEY hKey;
    DWORD val = 0;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\MyApp", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(val);
        RegQueryValueExW(hKey, L"FirstRunDone", nullptr, nullptr, (LPBYTE)&val, &size);
        RegCloseKey(hKey);
    }

    if (val == 0) {
        return true;
    }

    return false;
}

void completeFirstRun(){
	HKEY hKey;
	RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MyApp", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
	int val = 1;
	RegSetValueExW(hKey, L"FirstRunDone", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
	RegCloseKey(hKey);

}

void resetFirstRun(){
	LPCWSTR keyPath = L"Software\\MyApp";

	LONG result = RegDeleteKeyW(HKEY_CURRENT_USER, keyPath);

	if (result == ERROR_SUCCESS) {
		std::wcout << L"Key deleted successfully.\n";
	} else if (result == ERROR_FILE_NOT_FOUND) {
		std::wcout << L"Key does not exist.\n";
	} else {
		std::wcout << L"Failed to delete key. Error code: " << result << L"\n";
	}

}

int main() {

	filesystem::path folderPath = "D:\\Games\\SuperTux";

	wstring vol = GetVolumePath(folderPath);

	filesystem::path targetPath = folderPath.parent_path();

	sqlite3* db = nullptr;
	openDB("data.db",db);

	HANDLE volume = openVolume(vol);

	if (volume == INVALID_HANDLE_VALUE)
		wcout << L"Error opening file. Error code: " << GetLastError() << L" for " << vol << L"\n";

	USN_JOURNAL_DATA journal{};
	DWORD bytesReturned = 0;

	int queryOk = queryJournal(volume,journal,bytesReturned);
	if(!queryOk) return 1;

	resetFirstRun();

	if(isFirstRun()){
		CoInitialize(nullptr);
		filesystem::path folderPath = OpenFolderPicker();
		CoUninitialize();

		/*
		int status = remove("data.db");
		if(status != 0){
			perror("Error deleting file");
			return 1;
		}*/
		initDatabase(db);

		thread spinner(showSpinner);
		scanDirectory(folderPath, targetPath, L"SuperTux", db, -1);

		done = true;
		spinner.join();

		DWORDLONG lastUsn = journal.NextUsn;
		addUsnId(lastUsn,db);

		completeFirstRun();
	}


	DWORDLONG lastUsn = loadUsnId(lastUsn, db);
	lastUsn = 158775624;
	//lastUsn = 158744344;
	cout << "Before: " << lastUsn << "\n";

	lastUsn = readJournalSince(volume, journal, lastUsn, folderPath, db);

	addUsnId(lastUsn,db);
	cout << "After: " << lastUsn << "\n";

	sqlite3_close(db);

	return 0;
}

