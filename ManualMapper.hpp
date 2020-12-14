#pragma once

#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>

typedef HMODULE(__stdcall* MMAP_LoadLibraryA)(LPCSTR);
typedef FARPROC(__stdcall* MMAP_GetProcAddress)(HMODULE, LPCSTR);
typedef int(__stdcall* MMAP_dllmain)(HMODULE, DWORD, LPVOID);

struct ManualMapperLoadData
{
	void* imageBase;

	IMAGE_NT_HEADERS* NTHeaders;
	IMAGE_BASE_RELOCATION* baseRelocation;
	IMAGE_IMPORT_DESCRIPTOR* importDirectory;

	MMAP_LoadLibraryA fnLoadLibraryA;
	MMAP_GetProcAddress fnGetProcAddress;
};

class ManualMapper
{
private:
	HANDLE process = nullptr;

	/*
		HEADER INFO
	*/
	DWORD DOSHeaderFileLocation = 0;
	size_t nBytesReadDOSHeader = 0;
	IMAGE_DOS_HEADER DOSHeader{};

	DWORD NTHeadersFileLocation = 0;
	size_t nBytesReadNTHeaders = 0;
	IMAGE_NT_HEADERS NTHeaders{};

	DWORD sectionHeadersFileLocation = 0;
	size_t nBytesReadSectionHeaders = 0;
	size_t sectionHeadersSize = 0;
	IMAGE_SECTION_HEADER* sectionHeaders = nullptr; // its an array, btw ;)

	/*
		REMOTE COPYING INFORMATION
	*/
	void* remoteAddressOfImage = 0;
	size_t nBytesHeadersWritten = 0;
	size_t indexOfSectionToCopy = 0;
	size_t nBytesCopiedForSection = 0;

	void* remoteAddressOfLoader = 0;
	ManualMapperLoadData loaderParameters{};

	/*
		CLASS INFORMATION
	*/
	DWORD seekFileLocation = 0;
	int errorCode = 0;
	int errorContext = 0;
	bool executed = false;

	/*
		UTIL
	*/
	bool processStillAlive();

public:
	ManualMapper(HANDLE process);
	~ManualMapper();

	// returns the file location that the bytes in next ProcessBytesFromFile call should come from
	DWORD GetNextFileSeekLocation();

	// returns true if its done mapping the DLL
	bool ProcessBytesFromFile(BYTE* bytes, size_t nBytes);

	// returns a value from namespace ManualMapperError
	int GetErrorCode();
	int GetErrorContext();
	bool Errored();

	// begins the execution of the DLL
	// returns true if it was actually ready to execute
	bool Execute();

	// returns a char* descriptive of the
	// next step in the injection processs
	const char* GetNextStepDescription();


	// debug
	void printHeaders();
};