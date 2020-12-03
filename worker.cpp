#include "worker.hpp"

void Worker::run()
{
	if (!this->checkVersion())
		goto FAIL;

	if (!this->waitForCSGOToOpen())
		goto FAIL;

	if (!this->download())
		goto FAIL;

	if (!this->inject())
		goto FAIL;

	emit taskDescription(3, "Success! Closing in 3 seconds.");
	std::this_thread::sleep_for(std::chrono::seconds(1));
	emit taskDescription(3, "Success! Closing in 2 seconds.");
	std::this_thread::sleep_for(std::chrono::seconds(1));
	emit taskDescription(3, "Success! Closing in 1 seconds.");
	std::this_thread::sleep_for(std::chrono::seconds(1));

	// main thread will exit after return
	return;

FAIL:
	while (1) std::this_thread::sleep_for(std::chrono::seconds(1));; // never return
	return;
}

bool Worker::checkVersion()
{
	emit taskDescription(0, "Working...");

	std::string out;
	bool success = HTTP::Post("https://www.a4g4.com/API/InjectorVersion.php", this->injectorVersion, out);
	if (!success)
	{
		emit taskComplete(0, false);
		emit taskDescription(0, "FAILED - Check your internet connection.");
		this->failed();
	}

	bool matches = out == this->injectorVersion;
	if (!matches)
	{
		emit taskComplete(0, false);
		emit taskDescription(0, "Download a newer version from https://particle.church");
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
	std::this_thread::sleep_for(std::chrono::seconds(2));

	emit taskComplete(2, true);
	emit taskDescription(2, "Success!");
	return true;
}

bool Worker::inject()
{
	std::this_thread::sleep_for(std::chrono::seconds(2));
	std::string DLLPath = this->getDesktopPath() + "\\CSGOCollabV1.dll";
	qInfo() << DLLPath.c_str();
	qInfo() << this->CSGO_PID;
	qInfo() << sizeof(DWORD);
	qInfo() << sizeof(HANDLE);
	qInfo() << sizeof(LPVOID);
	if (!INJECTOR::CSGO(DLLPath, this->CSGO_PID))
	{
		emit taskComplete(3, false);
		emit taskDescription(3, "FAILED - Please try again.");
		this->failed();
	}

	emit taskComplete(3, true);
	emit taskDescription(3, "Success! Closing in 3 seconds.");
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
	while (1)
		std::this_thread::sleep_for(std::chrono::seconds(1));
}

std::string Worker::getDesktopPath()
{
	char path[MAX_PATH + 1];
	if (!SHGetSpecialFolderPath(HWND_DESKTOP, path, CSIDL_DESKTOP, FALSE))
		return "C:/";
	return std::string(path);
}