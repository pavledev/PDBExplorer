#pragma once

#include <iostream>
#include <vector>
#include <map>

class DemangleData
{
private:
	std::vector<std::string> arguments;
	std::vector<std::string> fragments;
	std::vector<std::map<std::vector<std::string>, std::vector<std::string>>> history;
	std::string value;
	bool verbose;

public:
	DemangleData(std::string value, bool verbose);
	std::string GetValue();
	void SetValue(std::string value);
	int GetArgumentsSize();
	std::string GetArgument(int index);
	std::string GetFragment(int index);
	void Advance(int count);
	int Index(char pos);
	void AddFragment(std::string fragment);
	void AddArgument(std::string argument);
	void EnterTemplate();
	void ExitTemplate();
	bool IsInTemplate();
	std::string Join(const std::vector<std::string>& vector, std::string delimiter);

	template <typename... Args>
	void Log(char const* const format, Args const&... args) noexcept
	{
		if (verbose)
		{
			printf_s(format, args...);

			printf_s(" REST=%s", value.c_str());
			printf_s(" ARG=%s", Join(arguments, ",").c_str());
			printf_s(" FRAG=%s\n", Join(fragments, ",").c_str());
		}
	}
};
