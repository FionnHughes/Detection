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

int main() {

	CoInitialize(nullptr);
	//filesystem::path folderPath = OpenFolderPicker();
	CoUninitialize();

	filesystem::path folderPath = "D:\\Games\\SuperTux";

	wcout << folderPath.wstring() << "\n";

	wstring vol = GetVolumePath(folderPath);

	filesystem::path targetPath = folderPath.parent_path();

	int status = remove("data.db");
	if(status != 0){
		perror("Error deleting file");
		return 1;
	}

	sqlite3* db = nullptr;
	openDB("data.db",db);
	initDatabase(db);

	thread spinner(showSpinner);
	scanDirectory(targetPath, L"SuperTux", db, -1);

	done = true;
	spinner.join();


	HANDLE volume = openVolume(vol);

	if (volume == INVALID_HANDLE_VALUE)
		wcout << L"Error opening file. Error code: " << GetLastError() << L" for " << "\\\\.\\D:" << L"\n";

	USN_JOURNAL_DATA journal{};
	DWORD bytesReturned = 0;

	int queryOk = queryJournal(volume,journal,bytesReturned);
	if(!queryOk) return 1;

	DWORDLONG lastUsn = journal.NextUsn;

	cout << "Very last: " << lastUsn << "\n";

	lastUsn = loadUsnId(lastUsn, db);
	//lastUsn = 156856088;
	lastUsn = 158744344;
	cout << "Before: " << lastUsn << "\n";

	lastUsn = readJournalSince(volume, journal, lastUsn, L"\\\\?\\D:\\Games\\SuperTux", db);

	addUsnId(lastUsn,db);
	cout << "After: " << lastUsn << "\n";




	sqlite3_close(db);

	//printDirectory(rootItems);

	return 0;
}

