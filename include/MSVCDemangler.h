#pragma once

#include <iostream>
#include <map>
#include "DemangleData.h"
#include "DataType.h"

class MSVCDemangler
{
public:
	MSVCDemangler();
	std::string DemangleSymbol(std::string symbol, std::string& rest, bool verbose = false);

private:
	std::string quoteB = "`";
	std::string quoteE = "'";

	std::map<std::string, std::string> callingConventions;
	std::map<std::string, std::multimap<std::string, std::vector<std::string>>> dataTypes;
	std::map<std::string, std::string> enumTypes;
	std::map<std::string, std::string> specialFragments;
	std::map<std::string, std::multimap<std::string, std::string>> thunkAccesses;

	std::string DemangleReentrantSymbol(DemangleData* data);
	std::tuple<DataType, std::string, std::string> DemangleFunctionPrototypeSymbol(DemangleData* data);
	std::string DemangleLocalStaticGuardSymbol(std::vector<std::string> names, DemangleData* data);
	std::string DemangleVariableSymbol(std::vector<std::string> names, DemangleData* data);
	std::string DemangleFunctionSymbol(std::vector<std::string> names, DemangleData* data);
	std::string ExtractTemplate(DemangleData* data);
	std::string ExtractNameString(DemangleData* data);
	std::string ExtractSpecialName(DemangleData* data);
	std::vector<std::string> ExtractNamesList(DemangleData* data);
	std::string ExtractNameFragment(DemangleData* data);
	std::vector<std::string> ArgList(DemangleData* data, std::string stop = "");
	DataType GetDataType(DemangleData* data, int depth = 0);
	void FinalizeName(std::vector<std::string>& names, DataType& returnType);

	template <typename T>
	T ParseValue(DemangleData* data, std::map<std::string, T> table, std::string logMessage = "");

	template <>
	std::string ParseValue<std::string>(DemangleData* data, std::map<std::string, std::string> table, std::string logMessage)
	{
		//Function for accessing the tables below

		for (auto it = table.begin(); it != table.end(); it++)
		{
			std::string name = data->GetValue();

			if (name.substr(0, it->first.length()) == it->first)
			{
				data->Advance(static_cast<int>(it->first.length()));

				if (logMessage.length())
				{
					data->Log("%s%s", logMessage, it->first);
				}

				return it->second;
			}
		}

		if (logMessage.length())
		{
			data->Log("%NONE", logMessage);
		}

		return {};
	}

	template <>
	std::multimap<std::string, std::vector<std::string>> ParseValue<std::multimap<std::string, std::vector<std::string>>>(DemangleData* data, std::map<std::string, std::multimap<std::string, std::vector<std::string>>> table, std::string logMessage)
	{
		//Function for accessing the tables below

		for (auto it = table.begin(); it != table.end(); it++)
		{
			std::string name = data->GetValue();

			if (name.substr(0, it->first.length()) == it->first)
			{
				data->Advance(static_cast<int>(it->first.length()));

				if (logMessage.length())
				{
					data->Log("%s%s", logMessage, it->first);
				}

				return it->second;
			}
		}

		if (logMessage.length())
		{
			data->Log("%NONE", logMessage);
		}

		return {};
	}

	template <>
	std::multimap<std::string, std::string> ParseValue<std::multimap<std::string, std::string>>(DemangleData* data, std::map<std::string, std::multimap<std::string, std::string>> table, std::string logMessage)
	{
		//Function for accessing the tables below

		for (auto it = table.begin(); it != table.end(); it++)
		{
			std::string name = data->GetValue();

			if (name.substr(0, it->first.length()) == it->first)
			{
				data->Advance(static_cast<int>(it->first.length()));

				if (logMessage.length())
				{
					data->Log("%s%s", logMessage, it->first);
				}

				return it->second;
			}
		}

		if (logMessage.length())
		{
			data->Log("%NONE", logMessage);
		}

		return {};
	}

	int DecodeNumber(DemangleData* data);
	std::vector<std::string> GetClassModifiers(DemangleData* data);
	std::string Join(const std::vector<std::string>& vector, std::string delimiter);

	template <typename T, typename... Args>
	std::vector<T> GetVector(const Args&... args)
	{
		return { args... };
	}
};
