#include "Debug.hpp"

// https://stackoverflow.com/questions/15543571/allocconsole-not-displaying-cout
void CreateConsole()
{
	if (!AttachConsole(ATTACH_PARENT_PROCESS) && !AllocConsole()) {
		return;
	}

	// std::cout, std::clog, std::cerr, std::cin
	FILE* fDummy;
	freopen_s(&fDummy, "CONOUT$", "w", stdout);
	freopen_s(&fDummy, "CONOUT$", "w", stderr);
	freopen_s(&fDummy, "CONIN$", "r", stdin);
	std::cout.clear();
	std::clog.clear();
	std::cerr.clear();
	std::cin.clear();

	// std::wcout, std::wclog, std::wcerr, std::wcin
	HANDLE hConOut = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	HANDLE hConIn = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
	SetStdHandle(STD_ERROR_HANDLE, hConOut);
	SetStdHandle(STD_INPUT_HANDLE, hConIn);
	std::wcout.clear();
	std::wclog.clear();
	std::wcerr.clear();
	std::wcin.clear();
}

namespace Debug
{
	bool Opened = false;
	std::vector<std::string> LogHistory = {"A4G4 Injector - Debugger"};

	void Log(std::string str)
	{
		if (!Enabled) return;

		if (Opened) std::cout << str << std::endl;
		else LogHistory.push_back(str);
	}
	void Open()
	{
		if (!Enabled) return;

		if (Opened) return;
		Opened = true;
		CreateConsole();

		for (size_t i = 0; i < LogHistory.size(); i++)
			std::cout << LogHistory[i] << std::endl;
	}
}