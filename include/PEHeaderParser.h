#pragma once

#include <Windows.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include "dbghelp.h"
#include "DIA SDK/cvconst.h"
#include <QObject>
#include <QHash>
#include <QMultiHash>

#pragma comment(lib, "dbghelp.lib")

#define MAKE_PTR(cast, ptr, addValue) (cast)((DWORD_PTR)(ptr) + (DWORD_PTR)(addValue))

extern "C" char* __unDName(char* outputString, const char* name, int maxStringLength, void* pAlloc, void* pFree, int disableFlags);

class PEHeaderParser : public QObject
{
	Q_OBJECT

private:
	LPVOID view;
	PIMAGE_NT_HEADERS imageNTHeaders = {};
	std::vector<PIMAGE_SECTION_HEADER> sectionHeaders;
	bool pe32Plus;

public:
	PEHeaderParser(QObject* parent = Q_NULLPTR);
	bool ReadPEHeader(QString fileName);
	bool ReadSections();
	bool ReadImportTable(std::unordered_map<std::string, std::string>& imports);
	bool ReadImportTable(QMultiHash<QString, QString>& imports);
	bool ReadExportTable(QHash<QString, QString>& exports);
	std::string UndecorateName(const char* decoratedName);
	CV_CPU_TYPE_e GetMachineType();
	DWORD ConvertVAToRVA(DWORD va);
	DWORD ConvertVAToFileOffset(DWORD va);
	DWORD ConvertRVAToVA(DWORD rva);
	DWORD ConvertRVAToFileOffset(DWORD rva);
	DWORD ConvertFileOffsetToVA(DWORD fileOffset);
	DWORD ConvertFileOffsetToRVA(DWORD fileOffset);

signals:
	void SendStatusMessage(const QString& status);
};
