#pragma once

#include <iostream>
#include <tuple>

class DataType
{
	/*
	* Usually a data type is a string, but if it is a function type,
	* then it is a triplet of strings
	*	( return type, calling convention & qualifiers, arguments )
	* We create a dedicated class, because we want to use += (aka. __iadd__)
	*/
private:
	std::tuple<std::string, std::string, std::string> value;
	bool isFunctionType;

public:
	DataType()
	{
		value = {};
		isFunctionType = false;
	}

	DataType(std::tuple<std::string, std::string, std::string> value, bool isFunctionType)
	{
		this->value = value;
		this->isFunctionType = isFunctionType;
	}

	std::string GetValue()
	{
		if (isFunctionType)
		{
			std::string callingConvention = get<1>(value);

			if (callingConvention.length())
			{
				callingConvention = "(" + callingConvention + ")";
			}

			return get<0>(value) + " " + callingConvention + get<2>(value);
		}

		return get<0>(value);
	}

	void SetValue(std::tuple<std::string, std::string, std::string> value)
	{
		this->value = value;
	}

	void Append(std::string value)
	{
		if (isFunctionType)
		{
			get<2>(this->value) += value;
		}
		else
		{
			get<0>(this->value) += value;
		}
	}

	void Prepend(std::string value)
	{
		get<0>(this->value).insert(0, value);
	}

	void Add(std::string value)
	{
		if (isFunctionType)
		{
			get<1>(this->value) += value;
		}
		else
		{
			get<0>(this->value) += value;
		}
	}
};
