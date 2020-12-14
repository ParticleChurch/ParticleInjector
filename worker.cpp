#include "worker.hpp"

void Worker::run()
{
	// give the user some time to at least see the window pop up, sheesh
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	if (!this->checkVersion())
		this->failed();

	std::this_thread::sleep_for(std::chrono::milliseconds(250));
	if (!this->waitForCSGOToOpen())
		this->failed();

	std::this_thread::sleep_for(std::chrono::milliseconds(250));
	if (!this->download())
		this->failed();

	std::this_thread::sleep_for(std::chrono::milliseconds(250));
	if (!this->inject())
		this->failed();

	emit taskDescription(3, "Success! Closing in 3 seconds.");
	std::this_thread::sleep_for(std::chrono::seconds(1));
	emit taskDescription(3, "Success! Closing in 2 seconds.");
	std::this_thread::sleep_for(std::chrono::seconds(1));
	emit taskDescription(3, "Success! Closing in 1 seconds.");
	std::this_thread::sleep_for(std::chrono::seconds(1));

	// main thread will exit after return
	return;
}

bool Worker::checkVersion()
{
	emit taskDescription(0, "Working...");

	HTTP::contentType = "text/plain";

	DWORD nBytes = 0;
	char* response = (char*)HTTP::Post("http://www.a4g4.com/API/injectorVersion.php", this->injectorVersion, &nBytes);

	if (!response)
	{
		emit taskComplete(0, false);
		emit taskDescription(0, "FAILED - Check your internet connection.");
		this->failed();
	}
	std::string currentVersion(response, response + nBytes);
	std::cout << "bytes read: " << nBytes << std::endl;
	std::cout << "response: " << response << std::endl;
	free(response);

	if (currentVersion != this->injectorVersion)
	{
		emit taskComplete(0, false);
		emit taskDescription(0, "Download a newer version from https://particle.church");
		std::cout << "Current version: " << currentVersion.c_str() << std::endl;
		std::cout << "My version: " << this->injectorVersion.c_str() << std::endl;
		this->failed();
	}

	emit taskComplete(0, true);
	emit taskDescription(0, "Success!");
	return true;
}
bool Worker::waitForCSGOToOpen()
{
	bool wasAlreadyOpen = true;
	HANDLE proc;
	while (!(proc = this->getCSGO()))
	{
		wasAlreadyOpen = false;
		emit taskDescription(1, "Please open CS:GO.");
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	this->CSGO = proc;
	this->CSGO_PID = GetProcessId(proc);
	if (!this->CSGO || !this->CSGO_PID)
	{
		emit taskDescription(1, "Failed to get CS:GO process ID.");
		return false;
	}

	while (!this->csgoIsInitialized())
	{
		wasAlreadyOpen = false;
		emit taskDescription(1, "Found - Waiting for CS:GO to initialize...");
		std::this_thread::sleep_for(std::chrono::seconds(1));

		DWORD ExitCode = 0;
		if (!GetExitCodeProcess(this->CSGO, &ExitCode) || ExitCode != STILL_ACTIVE)
		{
			// those fuckers closed csgo before it initialized >:(
			emit taskDescription(1, "FAILED - CSGO has been closed.");
			emit taskComplete(1, false);
			this->failed();
		}
	}

	if (!wasAlreadyOpen)
	{
		std::this_thread::sleep_for(std::chrono::seconds(3));
	}

	emit taskComplete(1, true);
	emit taskDescription(1, "Success!");
	return true;
}
bool Worker::download()
{
	emit taskDescription(2, "Downloading...");

	DWORD bytesRead = 0;
	this->file = HTTP::Post("https://www.a4g4.com/API/dll.php", "lol ur gay", &bytesRead);
	this->fileSize = bytesRead;
	if (!this->file || this->fileSize <= 0)
	{
		emit taskComplete(2, false);
		emit taskDescription(2, "Failed - Check your internet connection.");
	}

	emit taskComplete(2, true);
	emit taskDescription(2, "Success!");
	return true;
}

bool Worker::inject()
{
	emit taskDescription(3, "Parsing Encrypted File");
	Encryption::Header header = Encryption::parseHeader(this->file, this->fileSize);
	if (!header.isValid)
	{
		emit taskComplete(3, false);
		emit taskDescription(3, "Encryption Header Invalid - Contact a developer.");
		this->failed();
	}
	uint64_t dllSize = Encryption::getDecryptedSize(header, this->fileSize);

	emit taskDescription(3, "Mapping...");
	this->mapper = new ManualMapper(this->CSGO);
	DWORD seekAddr = 0;
	bool doneMapping = false;

	byte* decryptedFileBuffer = (byte*)malloc(252);
	if (!decryptedFileBuffer)
	{
		emit taskComplete(3, false);
		emit taskDescription(3, "Failed - Could not allocate 252 bytes.");
		this->failed();
	}

	while (!this->mapper->Errored() && !doneMapping)
	{
		seekAddr = this->mapper->GetNextFileSeekLocation();
		size_t nBytesToEOF = dllSize - seekAddr;
		if (seekAddr > dllSize)
		{
			emit taskComplete(3, false);
			emit taskDescription(3, "Failed - Attempted to access past EOF.");
			this->failed();
		}
		emit taskDescription(3, ("Processing chunk @ " + std::to_string(seekAddr)).c_str());

		// map seekAddress to fileAddress
		size_t chunkIndex = seekAddr / 252;
		uint64_t chunkBase = (uint64_t)chunkIndex * (uint64_t)256 + (uint64_t)header.size;
		Encryption::decryptChunk(header, chunkIndex, this->file + chunkBase, decryptedFileBuffer);

		doneMapping = mapper->ProcessBytesFromFile(decryptedFileBuffer + seekAddr % 252, 252 - (seekAddr % 252));
	}
	if (!mapper->Errored())
	{
		mapper->Execute();
	}
	else
	{
		emit taskComplete(3, false);
		emit taskDescription(3, (
			"FAILED - Mapper error: " +
			std::to_string(this->mapper->GetErrorContext()) + " - " +
			std::to_string(this->mapper->GetErrorCode())
		).c_str());

		this->failed();
	}
	free(this->file);

	emit taskComplete(3, true);
	emit taskDescription(3, "Success!");
	return true;
}

HANDLE Worker::getCSGO()
{
	HANDLE proc;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	PROCESSENTRY32 entry{};
	entry.dwSize = sizeof(entry);
	do
	{
		proc = OpenProcess(PROCESS_ALL_ACCESS, false, entry.th32ProcessID);
		if (this->processIsCSGO(proc))
			return proc;
	} while (Process32Next(snapshot, &entry));
	CloseHandle(snapshot);

	return 0;
}

bool Worker::processIsCSGO(HANDLE hProcess)
{
	if (!hProcess) return false;

	char _FileName[MAX_PATH + 1];
	GetModuleFileNameEx(hProcess, 0, _FileName, MAX_PATH);
	std::string FileName(_FileName);
	std::string csgo = "csgo.exe";

	if (FileName.size() < csgo.size()) return false;
	if (FileName.substr(FileName.size() - csgo.size()) != csgo) return false;
	
	return true;
}


bool Worker::csgoIsInitialized()
{
	bool allModulesLoaded = true;
	{
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, this->CSGO_PID);
		MODULEENTRY32 moduleEntry{};
		moduleEntry.dwSize = sizeof(moduleEntry);

		int numModules = CSGODLLs.size();
		bool* modulesFound = (bool*)malloc(numModules);
		if (!modulesFound) return false;

		do
		{
			for (int i = 0; i < numModules; i++)
			{
				if (CSGODLLs.at(i) == moduleEntry.szModule)
				{
					modulesFound[i] = true;
				}
			}
		} while (Module32Next(snapshot, &moduleEntry));

		for (int i = 0; i < numModules; i++)
		{
			if (!modulesFound[i])
				allModulesLoaded = false;
		}
		CloseHandle(snapshot);
		free(modulesFound);
	}
	if (!allModulesLoaded)
		return false;

	bool WindowOpen = false;
	{
		HWND hCurWnd = nullptr;
		do
		{
			hCurWnd = FindWindowEx(nullptr, hCurWnd, nullptr, nullptr);
			DWORD WindowProcessID = 0;
			GetWindowThreadProcessId(hCurWnd, &WindowProcessID);
			if (WindowProcessID == this->CSGO_PID)
			{
				WindowOpen = true;
				break;
			}
		} while (hCurWnd != nullptr);
	}
	if (!WindowOpen)
		return false;

	return true;
}

void Worker::failed()
{
	if (this->file) free(this->file);
	if (this->mapper) delete this->mapper;
	while (1)
		std::this_thread::sleep_for(std::chrono::hours(1));
}