/*
 * usn_journal.cpp
 *
 *  Created on: 9 Feb 2026
 *      Author: fionn
 */

#include <iostream>
#include <fstream>
#include <set>

#include "usn_journal.h"
#include "filesystem_scan.h"
using namespace std;

constexpr DWORD BUFFER_SIZE = 8 * 1024;

struct usnChanges{
	set<wstring> reasons;
	filesystem::path filePath;
	DWORD bitReasons;
};


FILE_ID_DESCRIPTOR getFileIdDescriptor(const DWORDLONG fileId)
{
    FILE_ID_DESCRIPTOR fileDescriptor;
    fileDescriptor.Type = FileIdType;
    fileDescriptor.FileId.QuadPart = fileId;
    fileDescriptor.dwSize = sizeof(fileDescriptor);

    return fileDescriptor;
}

HANDLE openVolume(const wstring& volPath){
	HANDLE volHandle = CreateFileW(
			volPath.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE	,
			nullptr,
			OPEN_EXISTING,
			0,
			nullptr
		);
	return volHandle;
}


int queryJournal(HANDLE volume, USN_JOURNAL_DATA& journalData, DWORD& bytesReturned){

	BOOL queryOk = DeviceIoControl(
			volume,
			FSCTL_QUERY_USN_JOURNAL,
			nullptr,
			0,
			&journalData,
			sizeof(journalData),
			&bytesReturned,
			nullptr
		);
	if (!queryOk) {
		cerr << "Failed to query USN journal. It may not exist. Error: " << GetLastError() << "\n";
		return 0;
	} else {
		cout << "USN journal queried successfully.\n";
		return 1;
	}
}

void loadLastUsn(string fileName, DWORDLONG& lastUsn){
	string lastUsnStr;

	fstream lastUsnFile(fileName, ios::in);
	if (lastUsnFile.is_open()) {
		getline(lastUsnFile, lastUsnStr);
		lastUsnFile.close();
	}
	if (lastUsnStr.empty()){
		ofstream outFile(fileName, ios::out);
		outFile << lastUsn;
		outFile.close();
		cout << "Writing to database\n";
	}
	else {
		cout << "Reading from database\n";
		lastUsn = stoull(lastUsnStr);
	}
}

void saveLastUsn(string fileName, DWORDLONG& lastUsn){
	ofstream outFile("LastUSN.txt", ios::out);
	outFile << lastUsn;
	outFile.close();
	cout << "Writing new lastUsn: " << lastUsn << " to database\n";
}

filesystem::path normalize(const filesystem::path& p)
{
    std::wstring s = p.native();

    // strip \\?\ prefix if present
    if (s.rfind(L"\\\\?\\", 0) == 0)
        s.erase(0, 4);

    return filesystem::weakly_canonical(filesystem::path(s));
}

int readJournalSince(HANDLE& volume, USN_JOURNAL_DATA& journal, DWORDLONG& lastUsn, const filesystem::path volPath, sqlite3*& db){

	READ_USN_JOURNAL_DATA readData{};
	readData.StartUsn       = lastUsn;
	readData.ReasonMask     = 0xFFFFFFFF;   // all reasons
	readData.UsnJournalID   = journal.UsnJournalID;

	BYTE readBuffer[BUFFER_SIZE];
	DWORD bytesReturned = 0;

	vector<usnChanges> seenFiles;

	cout << "-------------------" << volPath << "\n";

	do{

		BOOL readOk = DeviceIoControl(
				volume,
				FSCTL_READ_USN_JOURNAL,
				&readData,
				sizeof(readData),
				&readBuffer,
				sizeof(readBuffer),
				&bytesReturned,
				nullptr
			);

		if (!readOk) {
			cerr << "Failed to read USN journal. Error: " << GetLastError() << "\n";
			return -1;
		}
		if (bytesReturned <= sizeof(USN)) {
			// No records
			break;
		}

		USN* nextUsn = (USN*)readBuffer;
		DWORD offset = sizeof(USN);

		//cout << "Bytes returned: "<< bytesReturned << ". Offset is: " << offset << "\n\n";

		while (offset < bytesReturned) {
			//cout << "Offset: " << offset << "\n";
			USN_RECORD* UsnRecord = (USN_RECORD*)(readBuffer + offset);
			std::wstring fname(UsnRecord->FileName, UsnRecord->FileNameLength / sizeof(WCHAR));

			if ( fname.find(L"FVE2.") == 0) {
				offset += UsnRecord->RecordLength;
				lastUsn = UsnRecord->Usn;
				continue;
			}

			FILE_ID_DESCRIPTOR fileId = getFileIdDescriptor(UsnRecord->FileReferenceNumber);

			TCHAR filePath[MAX_PATH];
			HANDLE fileHandle = OpenFileById(
				volume,
				&fileId,
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr,
				FILE_FLAG_BACKUP_SEMANTICS
			);

			if (fileHandle == INVALID_HANDLE_VALUE) {
				DWORD err = GetLastError();
				if (err == ERROR_INVALID_PARAMETER) {
					//wcout << L"USN Record could not be opened (error: " << err << L") -- file name: " << fname << L"\n";
				}
				offset += UsnRecord->RecordLength;
				lastUsn = UsnRecord->Usn;
				continue;
			}

			GetFinalPathNameByHandle(fileHandle,filePath, MAX_PATH, 0);

			filesystem::path pathToUsn(filePath);

			pathToUsn = normalize(pathToUsn);
			//cout << pathToUsn << "\n";



			if (std::mismatch(volPath.begin(), volPath.end(), pathToUsn.begin()).first == volPath.end()) {  // path starts with targetDir

				DWORD reason = UsnRecord->Reason;

				set<wstring> relevantReasons;
				for (auto& r : usnReasons) {
					if (reason & r.mask) {
						relevantReasons.insert(r.name);
						//wcout << L"Change detected: " << r.name << L"\n";
					}
				}
				if(relevantReasons.size()){
					bool repeat = false;
					for (auto& item : seenFiles){
						if(item.filePath == pathToUsn){
							item.reasons.insert(relevantReasons.begin(), relevantReasons.end());
							repeat = true;
						}
					}
					if(!repeat){
						usnChanges newChange;
						newChange.filePath = pathToUsn;
						newChange.reasons = relevantReasons;
						seenFiles.push_back(newChange);
					}
				}
			}
			offset += UsnRecord->RecordLength;
			lastUsn = UsnRecord->Usn;
			CloseHandle(fileHandle);
		}
		readData.StartUsn = *nextUsn;

	} while(bytesReturned > sizeof(USN));
	CloseHandle(volume);

	wcout << L"\n";
	for (auto& item : seenFiles){
		wcout << item.filePath.filename().wstring() << "\n" << item.filePath.wstring() << "\n";
		item.bitReasons = 0;
		for (auto& reason : item.reasons){
			for (auto& r : usnReasons) {
				if (reason == r.name) {
					item.bitReasons = item.bitReasons | r.mask;
				}
			}
			wcout << reason << L" ";
		}
		wcout << item.bitReasons << L" ";
		wcout << L"\n\n";
		HANDLE fileHandle;
		WIN32_FIND_DATAW fileData;
		DWORD volumeSerial;
		uint64_t fileIndex;
		FindFirstFileW(item.filePath.c_str(), &fileData);

		if(filesystem::is_regular_file(item.filePath)){

			fileHandle = openFile(item.filePath);

		}
		else{
			fileHandle = openFolder(item.filePath);
		}
		getVolumeFileIndex(fileHandle, volumeSerial, fileIndex);
		FSItem newData;

		newData.change = item.bitReasons;




		getFileDetails(volPath, item.filePath.parent_path(), fileHandle, fileData, db, getCurrentFolderId(db, volumeSerial, fileIndex), 0, newData);
		CloseHandle(fileHandle);
	}
	return lastUsn;
}

