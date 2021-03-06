// RansomDetection.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <RestartManager.h>
#include <Shlwapi.h>
#pragma comment(lib, "Rstrtmgr.lib")
#pragma comment(lib, "Shlwapi.lib")


#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>

typedef LONG(NTAPI *NtSuspendProcess)(IN HANDLE ProcessHandle);

void MonitorDirectory(LPCWSTR[]);
void GetFileProcName(LPTSTR);

// all purpose structure to contain directory information and provide
// the input buffer that is filled with file change data
#define MAX_DIRS 6
#define MAX_FILES 255
#define MAX_PROC 255
#define MAX_BUFFER 4096

typedef struct _DIRECTORY_INFO {
	HANDLE hDir;
	TCHAR lpszDirName[MAX_PATH];
	CHAR lpBuffer[MAX_BUFFER];
	DWORD dwBufLength;
	OVERLAPPED Overlapped;
}DIRECTORY_INFO, *PDIRECTORY_INFO, *LPDIRECTORY_INFO;

DIRECTORY_INFO DirInfo[MAX_DIRS];   // Buffer for all of the directories
TCHAR FileList[MAX_FILES*MAX_PATH]; // Buffer for all of the files
DWORD numDirs;

DWORD suspendId[MAX_PROC];
int pos = 0;

void _tmain(int argc, TCHAR *argv[])
{
	if (argc != 1)
	{
		_tprintf(TEXT("Usage: %s \n"), argv[0]);
		return;
	}

	LPCWSTR lpDirs[] = {TEXT("C:\\Cversions245"),TEXT("C:\\Xdata229"),
	 TEXT("C:\\Users\\Acly09pk"),TEXT("C:\\Users\\Qtua"),TEXT("C:\\Users\\Administrator\\Documents\\Lmirror147"),TEXT("C:\\Users\\Administrator\\Documents\\Nsetup158")};

	MonitorDirectory(lpDirs);
}

void SuspendProc(DWORD processId) {

	if (pos == 0) {
		suspendId[pos] = processId;
		pos++;
	}
	else if (pos <= MAX_PROC - 2) {

		for (int i = 0; i <= pos - 1; i++) {
			if (suspendId[i] == processId) {
				return;
			}
		}

		suspendId[pos] = processId;
		pos++;

	}
	else {
		printf("exceeds max proc");
		return;
	}

	printf("try to suspend proc %d\n", processId);

	HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

	NtSuspendProcess pfnNtSuspendProcess = (NtSuspendProcess)GetProcAddress(
		GetModuleHandle(TEXT("ntdll")), "NtSuspendProcess");

	pfnNtSuspendProcess(processHandle);
	CloseHandle(processHandle);

	printf("process suspended ");
}

void GetFileProcName(LPTSTR lpDir) {

	DWORD dwSession;
	WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };
	DWORD dwError = RmStartSession(&dwSession, 0, szSessionKey);
	wprintf(L"RmStartSession returned %d\n", dwError);
	if (dwError == ERROR_SUCCESS) {
		PCWSTR pszFile = lpDir;
		dwError = RmRegisterResources(dwSession, 1, &pszFile,
			0, NULL, 0, NULL);
		wprintf(L"RmRegisterResources(%ls) returned %d\n",
			pszFile, dwError);
		if (dwError == ERROR_SUCCESS) {
			DWORD dwReason;
			UINT i;
			UINT nProcInfoNeeded;
			UINT nProcInfo = 10;
			RM_PROCESS_INFO rgpi[10];
			dwError = RmGetList(dwSession, &nProcInfoNeeded,
				&nProcInfo, rgpi, &dwReason);
			wprintf(L"RmGetList returned %d\n", dwError);
			if (dwError == ERROR_SUCCESS) {
				wprintf(L"RmGetList returned %d infos (%d needed)\n",
					nProcInfo, nProcInfoNeeded);
				for (i = 0; i < nProcInfo; i++) {
					wprintf(L"%d.ApplicationType = %d\n", i,
						rgpi[i].ApplicationType);
					wprintf(L"%d.strAppName = %ls\n", i,
						rgpi[i].strAppName);
					wprintf(L"%d.Process.dwProcessId = %d\n", i,
						rgpi[i].Process.dwProcessId);

					SuspendProc(rgpi[i].Process.dwProcessId);
					
					/*
					HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
						FALSE, rgpi[i].Process.dwProcessId);
					if (hProcess) {
						FILETIME ftCreate, ftExit, ftKernel, ftUser;
						if (GetProcessTimes(hProcess, &ftCreate, &ftExit,
							&ftKernel, &ftUser) &&
							CompareFileTime(&rgpi[i].Process.ProcessStartTime,
								&ftCreate) == 0) {
							WCHAR sz[MAX_PATH];
							DWORD cch = MAX_PATH;
							if (QueryFullProcessImageNameW(hProcess, 0, sz, &cch) &&
								cch <= MAX_PATH) {
								wprintf(L"  = %ls\n", sz);
							}
						}
						CloseHandle(hProcess);
					}*/
				}
			}
		}
		RmEndSession(dwSession);

	}
}

void MonitorDirectory(LPCWSTR path[])
{
	char buf[MAX_DIRS][2048];
	DWORD nRet;
	BOOL result = TRUE;

	for (int i = 0; i <= MAX_DIRS-1; i++) {
		DirInfo[i].hDir = CreateFile(path[i], GENERIC_READ | FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			NULL);

		if (DirInfo[i].hDir == INVALID_HANDLE_VALUE)
		{
			printf("exit process dir %d %ws", i, path[i]);
			return; //cannot open folder
		}

		lstrcpy(DirInfo[i].lpszDirName, path[i]);
	}



	OVERLAPPED PollingOverlap[MAX_DIRS];
	HANDLE events[MAX_DIRS];

	FILE_NOTIFY_INFORMATION* pNotify;
	int offset;

	for (int i = 0; i <= MAX_DIRS - 1; i++) {
		
		PollingOverlap[i].OffsetHigh = 0;
		PollingOverlap[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		events[i] = PollingOverlap[i].hEvent;
	}


	while (result)
	{
		for (int i = 0; i <= MAX_DIRS - 1; i++)
		{
			result = ReadDirectoryChangesW(
				DirInfo[i].hDir,// handle to the directory to be watched
				&buf[i],// pointer to the buffer to receive the read results
				sizeof(buf[i]),// length of lpBuffer
				TRUE,// flag for monitoring directory or directory tree
				FILE_NOTIFY_CHANGE_FILE_NAME |
				FILE_NOTIFY_CHANGE_DIR_NAME |
				FILE_NOTIFY_CHANGE_SIZE |
				FILE_NOTIFY_CHANGE_LAST_WRITE |
				FILE_NOTIFY_CHANGE_LAST_ACCESS |
				FILE_NOTIFY_CHANGE_CREATION,
				&nRet,// number of bytes returned
				&PollingOverlap[i],// pointer to structure needed for overlapped I/O
				NULL);
		}


		DWORD dwWaitStatus; 
		WCHAR finalpath[MAX_PATH];


		dwWaitStatus = WaitForMultipleObjects(MAX_DIRS, events,
			FALSE, INFINITE);


		if (dwWaitStatus <= WAIT_OBJECT_0 + MAX_DIRS - 1 && dwWaitStatus >= WAIT_OBJECT_0) {
			int idx = dwWaitStatus - WAIT_OBJECT_0;

			offset = 0;

			wchar_t * wcstring;

			do
			{
				pNotify = (FILE_NOTIFY_INFORMATION*)((char*)buf[idx] + offset);
				wcstring = new wchar_t[pNotify->FileNameLength / 2 + 1];
				wcsncpy_s(wcstring, pNotify->FileNameLength / 2 + 1,pNotify->FileName, pNotify->FileNameLength / 2);
				wcstring[pNotify->FileNameLength / 2] = L'\0';

				switch (pNotify->Action)
				{
				case FILE_ACTION_ADDED:
					printf("\nThe file is added to the directory: [%ws] \n", wcstring);
					PathCombineW(finalpath, path[idx], (LPTSTR)wcstring);
					GetFileProcName(finalpath);
					break;
				case FILE_ACTION_REMOVED:
					printf("\nThe file is removed from the directory: [%ws] \n", wcstring);
					PathCombineW(finalpath, path[idx], (LPTSTR)wcstring);
					GetFileProcName(finalpath);
					break;
				case FILE_ACTION_MODIFIED:
					printf("\nThe file is modified. This can be a change in the time stamp or attributes: [%ws]\n", wcstring);
					PathCombineW(finalpath, path[idx], (LPTSTR)wcstring);
					GetFileProcName(finalpath);
					break;
				case FILE_ACTION_RENAMED_OLD_NAME:
					printf("\nThe file was renamed and this is the old name: [%ws]\n", wcstring);
					PathCombineW(finalpath, path[idx], (LPTSTR)wcstring);
					GetFileProcName(finalpath);
					break;
				case FILE_ACTION_RENAMED_NEW_NAME:
					printf("\nThe file was renamed and this is the new name: [%ws]\n", wcstring);
					PathCombineW(finalpath, path[idx], (LPTSTR)wcstring);
					GetFileProcName(finalpath);
					break;
				default:
					printf("\nDefault error.\n");
					break;
				}

				offset += pNotify->NextEntryOffset;

			} while (pNotify->NextEntryOffset); //(offset != 0);

		}
		else if (WAIT_TIMEOUT == dwWaitStatus) {
			printf("\nNo changes in the timeout period.\n");
		}
		else {
			printf("\n ERROR: Unhandled dwWaitStatus.\n");
			ExitProcess(GetLastError());
		}

	}

}

