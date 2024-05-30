#pragma once
#include <string>
#include <iostream>

namespace Debug
{
	inline void PrintError(const std::string& title, const std::string& msg)
	{
		std::cout << title << msg << std::endl;
	}
}
