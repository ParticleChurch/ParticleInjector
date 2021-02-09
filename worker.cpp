#include "worker.hpp"
#include "Debug.hpp"

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
	Debug::Log("vcheck...");
	emit taskDescription(0, "Working...");

	HTTP::contentType = "text/plain";

	DWORD nBytes = 0;
	char* response = (char*)HTTP::Post("http://www.a4g4.com/API/injectorVersion.php", this->injectorVersion, &nBytes);

	if (!response)
	{
		Debug::Log("err;" + std::to_string(nBytes) + " @ " + std::to_string((DWORD)response));
		emit taskComplete(0, false);
		emit taskDescription(0, "FAILED - Check your internet connection.");
		this->failed();
	}
	std::string serverVersion(response, response + nBytes);
	free(response);

	if (serverVersion != this->injectorVersion)
	{
		unsigned char* _b64 = base64::encode((unsigned char*)serverVersion.c_str(), min(256, serverVersion.size()), 0);
		std::string b64((char*)_b64);
		free(_b64);

		Debug::Log("err;" + this->injectorVersion + "/" + b64);
		emit taskComplete(0, false);
		emit taskDescription(0, "Download a newer version from https://a4g4.com");
		this->failed();
	}

	emit taskComplete(0, true);
	emit taskDescription(0, "Success!");
	return true;
}
bool Worker::waitForCSGOToOpen()
{
	Debug::Log("csgo...");
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
		Debug::Log("err;" + std::to_string((DWORD)(this->CSGO)) + "/" + std::to_string(this->CSGO_PID));
		emit taskDescription(1, "Failed to open CS:GO");
		emit taskComplete(1, false);
		this->failed();
	}

	while (!this->csgoIsInitialized())
	{
		wasAlreadyOpen = false;
		emit taskDescription(1, "Found - Waiting for CS:GO to initialize...");
		std::this_thread::sleep_for(std::chrono::seconds(1));

		DWORD ExitCode = 0;
		if (!GetExitCodeProcess(this->CSGO, &ExitCode) || ExitCode != STILL_ACTIVE)
		{
			Debug::Log("err; csgo closed");
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
	Debug::Log("dload...");
	emit taskDescription(2, "Downloading...");

	DWORD bytesRead = 0;
	HTTP::userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.141 Safari/537.36";
	HTTP::contentType = "application/json";

	this->file = HTTP::Post("amazon.com", "", &bytesRead); // www.a4g4.com/API/dll/download.php
	this->fileSize = bytesRead;
	if (!this->file || this->fileSize < 10000)
	{
		if (!this->file)
		{
			Debug::Log("err; http fail, see above");
		}
		else
		{
			unsigned char* _b64 = base64::encode(this->file, min(512, this->fileSize), 0);
			std::string b64((char*)_b64);
			free(_b64);

			Debug::Log("err; got " + std::to_string(this->fileSize) + " bytes: \n" + b64);
		}
		emit taskComplete(2, false);
		emit taskDescription(2, "Failed - Check your internet connection.");
		this->failed();
	}

	emit taskComplete(2, true);
	emit taskDescription(2, "Success!");
	return true;
}

bool Worker::inject()
{
	Debug::Log("inject...");
	emit taskDescription(3, "Parsing Encrypted File");
	Encryption::Header header = Encryption::parseHeader(this->file, this->fileSize);
	if (!header.isValid)
	{
		unsigned char* _b64 = base64::encode((unsigned char*)header.parseError.c_str(), min(512, header.parseError.size()), 0);
		std::string b64((char*)_b64);
		free(_b64);

		Debug::Log("err; invalid header: \n" + b64);
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
		Debug::Log("err; !decryptedFileBuffer");
		emit taskComplete(3, false);
		emit taskDescription(3, "Failed - Could not allocate 252 bytes.");
		this->failed();
	}

	size_t nChunks = (size_t)((fileSize - header.size) / 256);
	while (!this->mapper->Errored() && !doneMapping)
	{
		seekAddr = this->mapper->GetNextFileSeekLocation();
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

		bool isLastChunk = (chunkIndex + 1) >= nChunks;
		if (isLastChunk)
		{
			for (int i = 0; i < 252 - header.endPadding; i++)
			{
				decryptedFileBuffer[i] = decryptedFileBuffer[i + header.endPadding];
			}
		}
		byte* ptrToSoughtData = decryptedFileBuffer + seekAddr % 252;
		size_t soughtDataSize = (isLastChunk ? 252 - header.endPadding : 252) - seekAddr % 252;

		doneMapping = mapper->ProcessBytesFromFile(ptrToSoughtData, soughtDataSize);
	}
	if (!mapper->Errored())
	{
		mapper->Execute();
	}
	else
	{
		Debug::Log("err; mapper (ctx/win): " + 
			std::to_string(this->mapper->GetErrorContext()) + "/" +
			std::to_string(this->mapper->GetErrorCode())
		);

		emit taskComplete(3, false);
		emit taskDescription(3, (
			"FAILED - Mapper [code: " +
			std::to_string(this->mapper->GetErrorContext()) + "/" +
			std::to_string(this->mapper->GetErrorCode()) + "]"
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
	Debug::Open();
	Debug::Log("FAILURE");
	if (this->file) free(this->file);
	if (this->mapper) delete this->mapper;
	while (1)
		std::this_thread::sleep_for(std::chrono::hours(1));
}