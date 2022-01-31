#pragma once

struct PublicSymbol
{
	QString decoratedName;
	QString undecoratedName;
	DWORD addressOffset;
	DWORD addressSection;
	BOOL isInCode;
	BOOL isFunction;
	BOOL isInManagedCode;
	BOOL isInMSILCode;
	ULONGLONG length;
	LocationType locationType;
	DWORD virtualAddress;
	DWORD relativeVirtualAddress;
};
