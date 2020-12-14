#include "ManualMapper.hpp"
#include <fstream>

DWORD __stdcall internalLibraryLoader(void* parameter)
{
	ManualMapperLoadData* loaderData = (ManualMapperLoadData*)parameter;

	PIMAGE_BASE_RELOCATION pIBR = loaderData->baseRelocation;

	DWORD delta = (DWORD)((LPBYTE)loaderData->imageBase - loaderData->NTHeaders->OptionalHeader.ImageBase);

	while (pIBR->VirtualAddress)
	{
		if (pIBR->SizeOfBlock >= sizeof(IMAGE_BASE_RELOCATION))
		{
			int count = (pIBR->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
			PWORD list = (PWORD)(pIBR + 1);

			for (int i = 0; i < count; i++)
			{
				if (list[i])
				{
					PDWORD ptr = (PDWORD)((LPBYTE)loaderData->imageBase + (pIBR->VirtualAddress + (list[i] & 0xFFF)));
					*ptr += delta;
				}
			}
		}

		pIBR = (PIMAGE_BASE_RELOCATION)((LPBYTE)pIBR + pIBR->SizeOfBlock);
	}

	PIMAGE_IMPORT_DESCRIPTOR pIID = loaderData->importDirectory;

	// Resolve DLL imports
	while (pIID->Characteristics)
	{
		PIMAGE_THUNK_DATA OrigFirstThunk = (PIMAGE_THUNK_DATA)((LPBYTE)loaderData->imageBase + pIID->OriginalFirstThunk);
		PIMAGE_THUNK_DATA FirstThunk = (PIMAGE_THUNK_DATA)((LPBYTE)loaderData->imageBase + pIID->FirstThunk);

		HMODULE hModule = loaderData->fnLoadLibraryA((LPCSTR)loaderData->imageBase + pIID->Name);

		if (!hModule)
			return FALSE;

		while (OrigFirstThunk->u1.AddressOfData)
		{
			if (OrigFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)
			{
				// Import by ordinal
				DWORD Function = (DWORD)loaderData->fnGetProcAddress(hModule,
					(LPCSTR)(OrigFirstThunk->u1.Ordinal & 0xFFFF));

				if (!Function)
					return FALSE;

				FirstThunk->u1.Function = Function;
			}
			else
			{
				// Import by name
				PIMAGE_IMPORT_BY_NAME pIBN = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)loaderData->imageBase + OrigFirstThunk->u1.AddressOfData);
				DWORD Function = (DWORD)loaderData->fnGetProcAddress(hModule, (LPCSTR)pIBN->Name);
				if (!Function)
					return FALSE;

				FirstThunk->u1.Function = Function;
			}
			OrigFirstThunk++;
			FirstThunk++;
		}
		pIID++;
	}

	if (loaderData->NTHeaders->OptionalHeader.AddressOfEntryPoint)
	{
		MMAP_dllmain EntryPoint = (MMAP_dllmain)((LPBYTE)loaderData->imageBase + loaderData->NTHeaders->OptionalHeader.AddressOfEntryPoint);

		return EntryPoint((HMODULE)loaderData->imageBase, DLL_PROCESS_ATTACH, NULL); // Call the entry point
	}
	return TRUE;
}

DWORD __stdcall stub()
{
	return 0;
}

bool ManualMapper::processStillAlive()
{
	DWORD ExitCode = 0;
	return GetExitCodeProcess(this->process, &ExitCode) && ExitCode == STILL_ACTIVE;
}

ManualMapper::ManualMapper(HANDLE process)
{
	// assume handle is valid
	this->process = process;
}

ManualMapper::~ManualMapper()
{
	if (this->sectionHeaders) free(this->sectionHeaders);

	// only free library if we did not execute it
	if (this->processStillAlive())
	{
		if (!this->executed)
			if (this->remoteAddressOfImage) VirtualFreeEx(this->process, this->remoteAddressOfImage, 0, MEM_RELEASE);
		if (this->remoteAddressOfLoader) VirtualFreeEx(this->process, this->remoteAddressOfLoader, 0, MEM_RELEASE);
	}

	// note: it's not up to this class to CloseHandle(this->process), as it wasn't opened by us
}

DWORD ManualMapper::GetNextFileSeekLocation()
{
	return this->seekFileLocation;
}

bool ManualMapper::ProcessBytesFromFile(BYTE* bytes, size_t nBytes)
{
	if (this->Errored()) return true;
	if (this->executed) return true;

	// assume that bytes[0] = fileBytes[this->fileLocation]

	if (this->nBytesReadDOSHeader < sizeof(IMAGE_DOS_HEADER))
	{
		// currently reading the DOS header from file
		// the DOS header is always at the beginning of the file
		this->DOSHeaderFileLocation = 0;

		size_t bytesToRead = min(nBytes, sizeof(IMAGE_DOS_HEADER) - this->nBytesReadDOSHeader);
		BYTE* pDOSHeader = (BYTE*)&this->DOSHeader;
		memcpy(pDOSHeader + this->nBytesReadDOSHeader, bytes, bytesToRead);
		this->nBytesReadDOSHeader += bytesToRead;

		if (this->nBytesReadDOSHeader >= sizeof(IMAGE_DOS_HEADER))
		{
			// the DOS header has been fully read
			// now move on to NT Headers
			// the NT headers are always at file location = DOSHeader.e_lfanew
			this->NTHeadersFileLocation = this->DOSHeader.e_lfanew;
			this->seekFileLocation = this->NTHeadersFileLocation;
		}
		else
		{
			// the DOS header is not done being read
			this->seekFileLocation = this->DOSHeaderFileLocation + this->nBytesReadDOSHeader;
		}
		return false;
	}
	else if (this->nBytesReadNTHeaders < sizeof(IMAGE_NT_HEADERS))
	{
		// currently reading NT headers from file

		size_t bytesToRead = min(nBytes, sizeof(IMAGE_NT_HEADERS) - this->nBytesReadNTHeaders);
		BYTE* pNTHeaders = (BYTE*)&this->NTHeaders;
		memcpy(pNTHeaders + this->nBytesReadNTHeaders, bytes, bytesToRead);
		this->nBytesReadNTHeaders += bytesToRead;

		if (this->nBytesReadNTHeaders >= sizeof(IMAGE_NT_HEADERS))
		{
			// the NT headers have been fully read
			// now move on to Section Headers
			if (this->NTHeaders.FileHeader.NumberOfSections <= 0)
			{
				this->errorCode = GetLastError();
				this->errorContext = 1;
				return true;
			}

			this->sectionHeadersSize = (int64_t)this->NTHeaders.FileHeader.NumberOfSections * (int64_t)sizeof(IMAGE_SECTION_HEADER);
			this->sectionHeaders = (IMAGE_SECTION_HEADER*)malloc(this->sectionHeadersSize);
			if (!this->sectionHeaders)
			{
				this->errorCode = GetLastError();
				this->errorContext = 2;
				return true;
			}

			// section headers are always stored right after NT headers
			this->sectionHeadersFileLocation = this->NTHeadersFileLocation + sizeof(IMAGE_NT_HEADERS);
			this->seekFileLocation = this->sectionHeadersFileLocation;
			return false;
		}
		else
		{
			// not done reading NT headers
			this->seekFileLocation = this->NTHeadersFileLocation + this->nBytesReadNTHeaders;
		}
		return false;
	}
	else if (this->nBytesReadSectionHeaders < this->sectionHeadersSize)
	{
		// currently reading section headers from file
		size_t bytesToRead = min(nBytes, this->sectionHeadersSize - this->nBytesReadSectionHeaders);
		BYTE* pSectionHeaders = (BYTE*)this->sectionHeaders;
		memcpy(pSectionHeaders + this->nBytesReadSectionHeaders, bytes, bytesToRead);
		this->nBytesReadSectionHeaders += bytesToRead;

		if (this->nBytesReadSectionHeaders >= this->sectionHeadersSize)
		{
			// Section headers have been fully read
			// now move on to copying information to process

			// first, allocate enough space for our PE
			this->remoteAddressOfImage = VirtualAllocEx(this->process, NULL, this->NTHeaders.OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			if (!this->remoteAddressOfImage)
			{
				this->errorCode = GetLastError();
				this->errorContext = 3;
				return true;
			}

			// then, write our "headers"
			// these are simply the first OptionalHeader.SizeOfHeaders bytes of the file
			this->seekFileLocation = 0;
		}
		else
		{
			this->seekFileLocation = this->sectionHeadersFileLocation + this->nBytesReadSectionHeaders;
		}
		return false;
	}
	else if (this->nBytesHeadersWritten < this->NTHeaders.OptionalHeader.SizeOfHeaders)
	{
		// currently writing headers from file to process
		size_t bytesToWrite = min(nBytes, this->NTHeaders.OptionalHeader.SizeOfHeaders - this->nBytesHeadersWritten);

		bool wpm = WriteProcessMemory(this->process, (BYTE*)this->remoteAddressOfImage + this->nBytesHeadersWritten, bytes, bytesToWrite, NULL);
		if (!wpm)
		{
			this->errorCode = GetLastError();
			this->errorContext = 4;
			return true;
		}

		this->nBytesHeadersWritten += bytesToWrite;
		if (this->nBytesHeadersWritten >= this->NTHeaders.OptionalHeader.SizeOfHeaders)
		{
			// done copying headers to process
			// next step is to copy sections to csgo

			this->indexOfSectionToCopy = 0;
			this->nBytesCopiedForSection = 0;
			this->seekFileLocation = this->sectionHeaders[0].PointerToRawData;
		}
		else
		{
			// still copying headers to process
			this->seekFileLocation = this->nBytesHeadersWritten;
		}
		return false;
	}
	else if (this->indexOfSectionToCopy < this->NTHeaders.FileHeader.NumberOfSections)
	{
		// currently copying sections to process

		IMAGE_SECTION_HEADER sectionHeader = this->sectionHeaders[this->indexOfSectionToCopy];
		size_t bytesToWrite = min(nBytes, sectionHeader.SizeOfRawData - this->nBytesCopiedForSection);

		bool wpm = WriteProcessMemory(this->process, (BYTE*)this->remoteAddressOfImage + sectionHeader.VirtualAddress + this->nBytesCopiedForSection, bytes, bytesToWrite, NULL);
		if (!wpm)
		{
			this->errorCode = GetLastError();
			this->errorContext = 5;
			return true;
		}

		this->nBytesCopiedForSection += bytesToWrite;
		if (this->nBytesCopiedForSection >= sectionHeader.SizeOfRawData)
		{
			// finished copying this section
			this->indexOfSectionToCopy++;
			this->nBytesCopiedForSection = 0;

			if (this->indexOfSectionToCopy >= this->NTHeaders.FileHeader.NumberOfSections)
			{
				// completely done copying all sections
				// now allocate for loader func
				this->remoteAddressOfLoader = VirtualAllocEx(this->process, NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
				if (!this->remoteAddressOfLoader)
				{
					this->errorCode = GetLastError();
					this->errorContext = 6;
					return true;
				}

				// tmp vars
				uint64_t baseRelocationOffset = this->NTHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
				uint64_t importDirectoryOffset = this->NTHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

				// fill loader data
				this->loaderParameters.imageBase = this->remoteAddressOfImage;

				this->loaderParameters.NTHeaders = (IMAGE_NT_HEADERS*)((BYTE*)this->remoteAddressOfImage + this->DOSHeader.e_lfanew);
				this->loaderParameters.baseRelocation = (IMAGE_BASE_RELOCATION*)((BYTE*)this->remoteAddressOfImage + baseRelocationOffset);
				this->loaderParameters.importDirectory = (IMAGE_IMPORT_DESCRIPTOR*)((BYTE*)this->remoteAddressOfImage + importDirectoryOffset);

				// it is a matter of luck that the addresses are the same in each process
				// TODO: GetProcAddress for these, instead of hoping kernel32
				this->loaderParameters.fnLoadLibraryA = LoadLibraryA;
				this->loaderParameters.fnGetProcAddress = GetProcAddress;

				// copy parameters into loader space
				{
					bool wpm = WriteProcessMemory(this->process, this->remoteAddressOfLoader, &this->loaderParameters, sizeof(ManualMapperLoadData), NULL);
					if (!wpm)
					{
						this->errorCode = GetLastError();
						this->errorContext = 7;
						return true;
					}
				}

				// copy loader into loader space (after the parameters)
				{
					void* remoteAddress = (BYTE*)this->remoteAddressOfLoader + sizeof(ManualMapperLoadData);
					size_t fnSize = (DWORD)stub - (DWORD)internalLibraryLoader;
					bool wpm = WriteProcessMemory(this->process, remoteAddress, internalLibraryLoader, fnSize, NULL);
					if (!wpm)
					{
						this->errorCode = GetLastError();
						this->errorContext = 8;
						return true;
					}
				}

				// the loader and parameter has been copied, now must call ManualMapper::Execute()
				return true;
			}
			else
			{
				// there are more sections to copy
				this->seekFileLocation = this->sectionHeaders[this->indexOfSectionToCopy].PointerToRawData;
			}
		}
		else
		{
			// not finished copying this section
			this->seekFileLocation = sectionHeader.PointerToRawData + this->nBytesCopiedForSection;
		}
		return false;
	}

	return true;
}

int ManualMapper::GetErrorCode()
{
	return this->errorCode;
}

int ManualMapper::GetErrorContext()
{
	return this->errorContext;
}

bool ManualMapper::Errored()
{
	return this->errorCode || this->errorContext;
}

bool ManualMapper::Execute()
{
	if (this->Errored()) return false;
	if (!this->remoteAddressOfLoader) return false;
	if (this->executed) return false;

	HANDLE hThread = CreateRemoteThread(
		this->process,
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)((BYTE*)this->remoteAddressOfLoader + sizeof(ManualMapperLoadData)),
		this->remoteAddressOfLoader,
		0,
		NULL
	);
	if (!hThread)
	{
		this->errorCode = GetLastError();
		this->errorContext = 9;
		return false;
	}
	WaitForSingleObject(hThread, INFINITE);

	// free loader code, it won't be used anymore
	if (VirtualFreeEx(this->process, this->remoteAddressOfLoader, 0, MEM_RELEASE))
		this->remoteAddressOfLoader = nullptr;

	this->executed = true;
	return true;
}

const char* ManualMapper::GetNextStepDescription()
{
	if (this->Errored()) return "Errored";
	if (this->executed) return "Injected!";

	if (this->nBytesReadDOSHeader < sizeof(IMAGE_DOS_HEADER))
		return "Reading MS-DOS header";
	else if (this->nBytesReadNTHeaders < sizeof(IMAGE_NT_HEADERS))
		return "Reading NT headers";
	else if (this->nBytesReadSectionHeaders < this->sectionHeadersSize)
		return "Reading section headers";
	else if (this->nBytesHeadersWritten < this->NTHeaders.OptionalHeader.SizeOfHeaders)
		return "Writing headers";
	else if (this->indexOfSectionToCopy < this->NTHeaders.FileHeader.NumberOfSections)
		return "Writing DLL sections";
	else
		return "Injecting";
}

/* useful for debugging
#include <iostream>
void ManualMapper::printHeaders()
{
	std::cout << "===== DOS =====" << std::endl;
	std::cout << "e_magic    : " << this->DOSHeader.e_magic << std::endl;
	std::cout << "e_cblp     : " << this->DOSHeader.e_cblp << std::endl;
	std::cout << "e_cp       : " << this->DOSHeader.e_cp << std::endl;
	std::cout << "e_crlc     : " << this->DOSHeader.e_crlc << std::endl;
	std::cout << "e_cparhdr  : " << this->DOSHeader.e_cparhdr << std::endl;
	std::cout << "e_minalloc : " << this->DOSHeader.e_minalloc << std::endl;
	std::cout << "e_maxalloc : " << this->DOSHeader.e_maxalloc << std::endl;
	std::cout << "e_ss       : " << this->DOSHeader.e_ss << std::endl;
	std::cout << "e_sp       : " << this->DOSHeader.e_sp << std::endl;
	std::cout << "e_csum     : " << this->DOSHeader.e_csum << std::endl;

	std::cout << "===== NT =====" << std::endl;
	std::cout << "FileHeader.Machine              : " << this->NTHeaders.FileHeader.Machine << std::endl;
	std::cout << "FileHeader.NumberOfSections     : " << this->NTHeaders.FileHeader.NumberOfSections << std::endl;
	std::cout << "FileHeader.TimeDateStamp        : " << this->NTHeaders.FileHeader.TimeDateStamp << std::endl;
	std::cout << "FileHeader.PointerToSymbolTable : " << this->NTHeaders.FileHeader.PointerToSymbolTable << std::endl;
	std::cout << "FileHeader.NumberOfSymbols      : " << this->NTHeaders.FileHeader.NumberOfSymbols << std::endl;
	std::cout << "FileHeader.SizeOfOptionalHeader : " << this->NTHeaders.FileHeader.SizeOfOptionalHeader << std::endl;
	std::cout << "FileHeader.Characteristics      : " << this->NTHeaders.FileHeader.Characteristics << std::endl;
	std::cout << "OptionalHeader.ImageBase                   : " << this->NTHeaders.OptionalHeader.ImageBase << std::endl;
	std::cout << "OptionalHeader.SectionAlignment            : " << this->NTHeaders.OptionalHeader.SectionAlignment << std::endl;
	std::cout << "OptionalHeader.FileAlignment               : " << this->NTHeaders.OptionalHeader.FileAlignment << std::endl;
	std::cout << "OptionalHeader.MajorOperatingSystemVersion : " << this->NTHeaders.OptionalHeader.MajorOperatingSystemVersion << std::endl;
	std::cout << "OptionalHeader.MinorOperatingSystemVersion : " << this->NTHeaders.OptionalHeader.MinorOperatingSystemVersion << std::endl;
	std::cout << "OptionalHeader.MajorImageVersion           : " << this->NTHeaders.OptionalHeader.MajorImageVersion << std::endl;
	std::cout << "OptionalHeader.MinorImageVersion           : " << this->NTHeaders.OptionalHeader.MinorImageVersion << std::endl;
	std::cout << "OptionalHeader.MajorSubsystemVersion       : " << this->NTHeaders.OptionalHeader.MajorSubsystemVersion << std::endl;
	std::cout << "OptionalHeader.MinorSubsystemVersion       : " << this->NTHeaders.OptionalHeader.MinorSubsystemVersion << std::endl;
	std::cout << "OptionalHeader.Win32VersionValue           : " << this->NTHeaders.OptionalHeader.Win32VersionValue << std::endl;
	std::cout << "OptionalHeader.SizeOfImage                 : " << this->NTHeaders.OptionalHeader.SizeOfImage << std::endl;
	std::cout << "OptionalHeader.SizeOfHeaders               : " << this->NTHeaders.OptionalHeader.SizeOfHeaders << std::endl;
	std::cout << "OptionalHeader.CheckSum                    : " << this->NTHeaders.OptionalHeader.CheckSum << std::endl;
	std::cout << "OptionalHeader.Subsystem                   : " << this->NTHeaders.OptionalHeader.Subsystem << std::endl;
	std::cout << "OptionalHeader.DllCharacteristics          : " << this->NTHeaders.OptionalHeader.DllCharacteristics << std::endl;
	std::cout << "OptionalHeader.SizeOfStackReserve          : " << this->NTHeaders.OptionalHeader.SizeOfStackReserve << std::endl;
	std::cout << "OptionalHeader.SizeOfStackCommit           : " << this->NTHeaders.OptionalHeader.SizeOfStackCommit << std::endl;
	std::cout << "OptionalHeader.SizeOfHeapReserve           : " << this->NTHeaders.OptionalHeader.SizeOfHeapReserve << std::endl;
	std::cout << "OptionalHeader.SizeOfHeapCommit            : " << this->NTHeaders.OptionalHeader.SizeOfHeapCommit << std::endl;
	std::cout << "OptionalHeader.LoaderFlags                 : " << this->NTHeaders.OptionalHeader.LoaderFlags << std::endl;
	std::cout << "OptionalHeader.NumberOfRvaAndSizes         : " << this->NTHeaders.OptionalHeader.NumberOfRvaAndSizes << std::endl;

	for (int i = 0; i < this->NTHeaders.FileHeader.NumberOfSections; i++)
	{
		IMAGE_SECTION_HEADER sh = this->sectionHeaders[i];
		std::cout << "===== SECTION " << i << " =====" << std::endl;
		std::cout << "Misc.PhysicalAddress : " << sh.Misc.PhysicalAddress << std::endl;
		std::cout << "Misc.VirtualSize     : " << sh.Misc.VirtualSize << std::endl;
		std::cout << "VirtualAddress       : " << sh.VirtualAddress << std::endl;
		std::cout << "SizeOfRawData        : " << sh.SizeOfRawData << std::endl;
		std::cout << "PointerToRawData     : " << sh.PointerToRawData << std::endl;
		std::cout << "PointerToRelocations : " << sh.PointerToRelocations << std::endl;
		std::cout << "PointerToLinenumbers : " << sh.PointerToLinenumbers << std::endl;
		std::cout << "NumberOfRelocations  : " << sh.NumberOfRelocations << std::endl;
		std::cout << "NumberOfLinenumbers  : " << sh.NumberOfLinenumbers << std::endl;
		std::cout << "Characteristics      : " << sh.Characteristics << std::endl;
	}

	std::cout << "===========================" << std::endl;
}
//*/