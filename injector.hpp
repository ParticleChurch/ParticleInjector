#include <Windows.h>
#include <string>


namespace INJECTOR
{
    extern LPVOID ntOpenFile;
    extern HANDLE procHandle;

    extern bool InjectDLL(DWORD PID, std::string file_path);
    extern bool CSGO_Bypass();
    extern bool CSGO_Backup();
    extern bool CSGO(std::string filePath, DWORD PID);
}