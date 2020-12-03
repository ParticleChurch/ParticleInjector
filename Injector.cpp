#include "injector.hpp"

namespace INJECTOR
{
    LPVOID ntOpenFile;
    HANDLE procHandle;

    bool InjectDLL(DWORD PID, std::string file_path)
    {
        long dll_size = file_path.length() + 1;

        HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, PID);
        if (!hProc) return false;

        LPVOID MyAlloc = VirtualAllocEx(hProc, NULL, dll_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (MyAlloc == NULL) return false;

        int IsWriteOK = WriteProcessMemory(hProc, MyAlloc, file_path.c_str(), dll_size, 0);
        if (IsWriteOK == 0) return false;

        DWORD lpThreadId;
        LPTHREAD_START_ROUTINE addrLoadLibrary = (LPTHREAD_START_ROUTINE)GetProcAddress(LoadLibraryExA("kernel32", NULL, LOAD_IGNORE_CODE_AUTHZ_LEVEL), "LoadLibraryA");
        HANDLE ThreadReturn = CreateRemoteThread(hProc, NULL, 0, addrLoadLibrary, MyAlloc, 0, &lpThreadId);

        if (ThreadReturn == NULL) return false;

        if ((hProc != NULL) && (MyAlloc != NULL) && (IsWriteOK != ERROR_INVALID_HANDLE) && (ThreadReturn != NULL)) {
            return true;
        }
    }

    bool CSGO_Bypass() {
        if (ntOpenFile) {
            char originalBytes[5];
            memcpy(originalBytes, ntOpenFile, 5);
            WriteProcessMemory(procHandle, ntOpenFile, originalBytes, 5, NULL);
            return true;
        }
        else
            return false;
    }
    bool CSGO_Backup() {
        if (ntOpenFile) {
            //So, when I patching first 5 bytes I need to backup them to 0? (I think)
            char originalBytes[5];
            memcpy(originalBytes, ntOpenFile, 5);
            WriteProcessMemory(procHandle, ntOpenFile, originalBytes, 0, NULL);
        }
        else
            return false;
    }
    bool CSGO(std::string filePath, DWORD PID) {

        //getting ready for exploit
        ntOpenFile = GetProcAddress(LoadLibraryW(L"ntdll"), "NtOpenFile");
        //getting the processID for injection/read/writing
        //getting the processHandle for injection/read/writing
        procHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, PID);
        if (CSGO_Bypass()) {
            if (InjectDLL(PID, filePath)) {
                return CSGO_Backup();
            }
            else {
                return false;
            }
        }
        else
            return false;
    }
}