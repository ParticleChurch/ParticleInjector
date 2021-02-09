#pragma once
#include <string>
#include <vector>
#include <Windows.h>
#include <iostream>
#include "base64.hpp"

namespace Debug
{
	constexpr bool Enabled = true;
	extern bool Opened;

	extern std::vector<std::string> LogHistory;
	extern void Log(std::string str);
	extern void Open();
}