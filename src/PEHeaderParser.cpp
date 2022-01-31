#include "PEHeaderParser.h"

PEHeaderParser::PEHeaderParser(QObject* parent)
{
	setParent(parent);
}

bool PEHeaderParser::ReadPEHeader(QString fileName)
{
	HANDLE file;
	uintptr_t fileSize;
	DWORD bytesRead;
	LPVOID fileData;
	PIMAGE_SECTION_HEADER sectionHeader = {};

	QByteArray buffer;
	buffer = fileName.toLocal8Bit();
	LPCSTR fileName2 = buffer.constData();

	file = CreateFileA(fileName2, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file == INVALID_HANDLE_VALUE)
	{
		emit SendStatusMessage(QString("Failed to open file. Error code: %1").arg(GetLastError()));

		return false;
	}

	//HANDLE fileMap = CreateFileMapping(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
	HANDLE fileMap = CreateFileMapping(file, nullptr, PAGE_READONLY, 0, 0, nullptr);

	if (!fileMap)
	{
		emit SendStatusMessage(QString("Failed to map file into memory. Error code: %1").arg(GetLastError()));
		return false;
	}

	view = MapViewOfFile(fileMap, FILE_MAP_READ, 0, 0, 0);

	if (!view)
	{
		emit SendStatusMessage(QString("Failed to open view. Error code: %1").arg(GetLastError()));
		return false;
	}

	PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(view);

	if (IsBadReadPtr(dosHeader, sizeof(IMAGE_DOS_HEADER)))
	{
		return false;
	}

	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
	{
		return false;
	}

	imageNTHeaders = MAKE_PTR(PIMAGE_NT_HEADERS, dosHeader, dosHeader->e_lfanew);

	if (!imageNTHeaders)
	{
		return false;
	}

	if (IsBadReadPtr(imageNTHeaders, sizeof(imageNTHeaders->Signature)))
	{
		return false;
	}

	if (imageNTHeaders->Signature != IMAGE_NT_SIGNATURE)
	{
		return false;
	}

	if (IsBadReadPtr(&imageNTHeaders->FileHeader, sizeof(IMAGE_FILE_HEADER)))
	{
		return false;
	}

	if (IsBadReadPtr(&imageNTHeaders->OptionalHeader, imageNTHeaders->FileHeader.SizeOfOptionalHeader))
	{
		return false;
	}

	PIMAGE_SECTION_HEADER sectionHeaders = IMAGE_FIRST_SECTION(imageNTHeaders);

	if (IsBadReadPtr(sectionHeaders, imageNTHeaders->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER)))
	{
		return false;
	}

	if (imageNTHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
	{
		pe32Plus = false;
	}
	else
	{
		if (imageNTHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
		{
			pe32Plus = true;
		}
		else
		{
			return false;
		}
	}

	ReadSections();

	return true;
}

bool PEHeaderParser::ReadSections()
{
	PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(imageNTHeaders);

	for (int i = 0; i < imageNTHeaders->FileHeader.NumberOfSections; i++, sectionHeader++)
	{
		sectionHeaders.push_back(sectionHeader);
	}

	return true;
}

bool PEHeaderParser::ReadImportTable(std::unordered_map<std::string, std::string>& imports)
{
	DWORD thunk = 0;
	PIMAGE_THUNK_DATA thunkData = {};
	DWORD ordinal = 0;
	DWORD rva = 0;
	DWORD size = 0;

	if (pe32Plus)
	{
		PIMAGE_OPTIONAL_HEADER64 header = reinterpret_cast<PIMAGE_OPTIONAL_HEADER64>(&imageNTHeaders->OptionalHeader);

		rva = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
		size = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
	}
	else
	{
		PIMAGE_OPTIONAL_HEADER32 header = reinterpret_cast<PIMAGE_OPTIONAL_HEADER32>(&imageNTHeaders->OptionalHeader);

		rva = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
		size = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
	}

	if (rva == 0 && size == 0)
	{
		return false;
	}

	if (size < sizeof(IMAGE_IMPORT_DESCRIPTOR))
	{
		return false;
	}

	IMAGE_IMPORT_DESCRIPTOR* importDescriptor = MAKE_PTR(IMAGE_IMPORT_DESCRIPTOR*, view, rva);

	if (IsBadReadPtr(importDescriptor, size))
	{
		return false;
	}

	for (; importDescriptor->FirstThunk; importDescriptor++)
	{
		std::string dllName;

		if (!IsBadReadPtr(static_cast<char*>(view) + importDescriptor->Name, 0x1000))
		{
			dllName = static_cast<char*>(view) + importDescriptor->Name;
		}

		PIMAGE_THUNK_DATA thunkData = MAKE_PTR(PIMAGE_THUNK_DATA, view, importDescriptor->FirstThunk);

		for (; thunkData->u1.AddressOfData; thunkData++)
		{
			auto rva = ULONG_PTR(thunkData) - ULONG_PTR(view);
			auto data = thunkData->u1.AddressOfData;

			if (data & IMAGE_ORDINAL_FLAG)
			{
				DWORD ordinal = data & ~IMAGE_ORDINAL_FLAG;
			}
			else
			{
				PIMAGE_IMPORT_BY_NAME importByName = MAKE_PTR(PIMAGE_IMPORT_BY_NAME, view, data);

				if (!IsBadReadPtr(importByName, 0x1000))
				{
					std::string mangledName = static_cast<char*>(importByName->Name);
					std::string demangledName = UndecorateName(mangledName.c_str());

					if (demangledName.length() > 0)
					{
						if (demangledName.find("::") != std::string::npos)
						{
							std::string typeName = demangledName.substr(0, demangledName.find("::", 0));

							imports.insert(make_pair(typeName, dllName));
						}
					}
					else
					{
						if (mangledName.find("::") != std::string::npos)
						{
							std::string typeName = mangledName.substr(0, mangledName.find("::", 0));

							imports.insert(make_pair(typeName, dllName));
						}
					}
				}
			}
		}
	}

	return true;
}

bool PEHeaderParser::ReadImportTable(QMultiHash<QString, QString>& imports)
{
	DWORD thunk = 0;
	PIMAGE_THUNK_DATA thunkData = {};
	DWORD ordinal = 0;
	DWORD rva = 0;
	DWORD size = 0;

	if (pe32Plus)
	{
		PIMAGE_OPTIONAL_HEADER64 header = reinterpret_cast<PIMAGE_OPTIONAL_HEADER64>(&imageNTHeaders->OptionalHeader);

		rva = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
		size = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
	}
	else
	{
		PIMAGE_OPTIONAL_HEADER32 header = reinterpret_cast<PIMAGE_OPTIONAL_HEADER32>(&imageNTHeaders->OptionalHeader);

		rva = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
		size = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
	}

	if (rva == 0 && size == 0)
	{
		return false;
	}

	if (size < sizeof(IMAGE_IMPORT_DESCRIPTOR))
	{
		return false;
	}

	IMAGE_IMPORT_DESCRIPTOR* importDescriptor = MAKE_PTR(IMAGE_IMPORT_DESCRIPTOR*, view, rva);

	if (IsBadReadPtr(importDescriptor, size))
	{
		return false;
	}

	for (; importDescriptor->FirstThunk; importDescriptor++)
	{
		QString dllName;

		if (!IsBadReadPtr(static_cast<char*>(view) + importDescriptor->Name, 0x1000))
		{
			dllName = QString::fromStdString(static_cast<char*>(view) + importDescriptor->Name);
		}

		PIMAGE_THUNK_DATA thunkData = MAKE_PTR(PIMAGE_THUNK_DATA, view, importDescriptor->FirstThunk);

		for (; thunkData->u1.AddressOfData; thunkData++)
		{
			auto rva = ULONG_PTR(thunkData) - ULONG_PTR(view);
			auto data = thunkData->u1.AddressOfData;

			if (data & IMAGE_ORDINAL_FLAG)
			{
				DWORD ordinal = data & ~IMAGE_ORDINAL_FLAG;
			}
			else
			{
				PIMAGE_IMPORT_BY_NAME importByName = MAKE_PTR(PIMAGE_IMPORT_BY_NAME, view, data);

				if (!IsBadReadPtr(importByName, 0x1000))
				{
					std::string mangledName = static_cast<char*>(importByName->Name);
					std::string demangledName = UndecorateName(mangledName.c_str());

					if (demangledName.length() > 0)
					{
						imports.insert(dllName, QString::fromStdString(demangledName));
					}
					else
					{
						imports.insert(dllName, QString::fromStdString(mangledName));
					}
				}
			}
		}
	}

	return true;
}

bool PEHeaderParser::ReadExportTable(QHash<QString, QString>& exports)
{
	DWORD rva = 0, size = 0;

	if (pe32Plus)
	{
		PIMAGE_OPTIONAL_HEADER64 header = reinterpret_cast<PIMAGE_OPTIONAL_HEADER64>(&imageNTHeaders->OptionalHeader);

		rva = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
		size = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
	}
	else
	{
		PIMAGE_OPTIONAL_HEADER32 header = reinterpret_cast<PIMAGE_OPTIONAL_HEADER32>(&imageNTHeaders->OptionalHeader);

		rva = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
		size = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
	}

	if (rva == 0 && size == 0)
	{
		return false;
	}

	PIMAGE_EXPORT_DIRECTORY imageExportDirectory = MAKE_PTR(PIMAGE_EXPORT_DIRECTORY, view, rva);
	PDWORD name = MAKE_PTR(PDWORD, view, imageExportDirectory->AddressOfNames);

	for (DWORD i = 0; i < imageExportDirectory->NumberOfNames; i++)
	{
		std::string name2 = UndecorateName(static_cast<char*>(view) + name[i]);

		exports.insert(QString::fromStdString(name2), "");
	}

	return true;
}

std::string PEHeaderParser::UndecorateName(const char* decoratedName)
{
	char* undecoratedName = new char[1024];

	__unDName(undecoratedName, decoratedName, 1024, malloc, free, UNDNAME_NAME_ONLY);

	if (strcmp(decoratedName, undecoratedName) == 0)
	{
		return "";
	}

	return undecoratedName;
}

CV_CPU_TYPE_e PEHeaderParser::GetMachineType()
{
	CV_CPU_TYPE_e type;

	switch (imageNTHeaders->FileHeader.Machine)
	{
	case IMAGE_FILE_MACHINE_I386:
		type = CV_CPU_TYPE_e::CV_CFL_80386;
		break;
	case IMAGE_FILE_MACHINE_IA64:
		type = CV_CPU_TYPE_e::CV_CFL_IA64;
		break;
	case IMAGE_FILE_MACHINE_AMD64:
		type = CV_CPU_TYPE_e::CV_CFL_AMD64;
		break;
	}

	return type;
}

DWORD PEHeaderParser::ConvertVAToRVA(DWORD va)
{
	return va - imageNTHeaders->OptionalHeader.ImageBase;
}

DWORD PEHeaderParser::ConvertVAToFileOffset(DWORD va)
{
	DWORD rva = va - imageNTHeaders->OptionalHeader.ImageBase;

	return ConvertRVAToFileOffset(rva);
}

DWORD PEHeaderParser::ConvertRVAToVA(DWORD rva)
{
	return rva + imageNTHeaders->OptionalHeader.ImageBase;
}

DWORD PEHeaderParser::ConvertRVAToFileOffset(DWORD rva)
{
	int sectionsCount = sectionHeaders.size();
	int index = 0;

	for (int i = 0; i < sectionsCount; i++)
	{
		if (rva >= sectionHeaders[i]->VirtualAddress)
		{
			index = i;
		}
	}

	return rva - sectionHeaders[index]->VirtualAddress + sectionHeaders[index]->PointerToRawData;
}

DWORD PEHeaderParser::ConvertFileOffsetToVA(DWORD fileOffset)
{
	DWORD rva = ConvertFileOffsetToRVA(fileOffset);

	return ConvertRVAToVA(rva);
}

DWORD PEHeaderParser::ConvertFileOffsetToRVA(DWORD fileOffset)
{
	int sectionsCount = sectionHeaders.size();
	int index = 0;

	for (int i = 0; i < sectionsCount; i++)
	{
		if (fileOffset >= sectionHeaders[i]->PointerToRawData)
		{
			index = i;
		}
	}

	return fileOffset + sectionHeaders[index]->VirtualAddress - sectionHeaders[index]->PointerToRawData;
}
