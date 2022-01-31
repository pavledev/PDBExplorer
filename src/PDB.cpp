#include "PDB.h"
#include "regs.h"

PDB::PDB(QObject* parent, Options* options, PEHeaderParser* peHeaderParser, QHash<QString, DWORD>* diaSymbols,
    std::vector<SymbolRecord>* symbolRecords, QHash<QString, DWORD>* variables, QHash<QString, DWORD>* functions,
    QHash<QString, DWORD>* publicSymbols) : QObject(parent)
{
    diaDataSource = nullptr;
    global = nullptr;
    diaSession = nullptr;
    processEnabled = false;

    this->options = options;
    this->peHeaderParser = peHeaderParser;
    this->diaSymbols = diaSymbols;
    this->symbolRecords = symbolRecords;
    this->variables = variables;
    this->functions = functions;
    this->publicSymbols = publicSymbols;

    classesCount = 0;
    structsCount = 0;
    interfacesCount = 0;
    unionsCount = 0;
    enumsCount = 0;
    vTableIndex = 0;
    destructorAdded = false;
    hasVirtualDestructor = false;
    isTypeImported = false;
    isMainUDT = false;
    belongsToMainUDT = false;

    baseTypes.insert("void", 0);
    baseTypes.insert("char", 0);
    baseTypes.insert("wchar_t", 0);
    baseTypes.insert("signed char", 0);
    baseTypes.insert("unsigned char", 0);
    baseTypes.insert("int", 0);
    baseTypes.insert("unsigned int", 0);
    baseTypes.insert("float", 0);
    baseTypes.insert("BCD", 0);
    baseTypes.insert("bool", 0);
    baseTypes.insert("short", 0);
    baseTypes.insert("unsigned short", 0);
    baseTypes.insert("long", 0);
    baseTypes.insert("unsigned long", 0);
    baseTypes.insert("__int8", 0);
    baseTypes.insert("__int16", 0);
    baseTypes.insert("__int32", 0);
    baseTypes.insert("__int64", 0);
    baseTypes.insert("__int128", 0);
    baseTypes.insert("unsigned __int8", 0);
    baseTypes.insert("unsigned __int16", 0);
    baseTypes.insert("unsigned __int32", 0);
    baseTypes.insert("unsigned __int64", 0);
    baseTypes.insert("unsigned __int128", 0);
    baseTypes.insert("CURRENCY", 0);
    baseTypes.insert("DATE", 0);
    baseTypes.insert("VARIANT", 0);
    baseTypes.insert("COMPLEX", 0);
    baseTypes.insert("BIT", 0);
    baseTypes.insert("BSTR", 0);
    baseTypes.insert("HRESULT", 0);
    baseTypes.insert("char16_t", 0);
    baseTypes.insert("char32_t", 0);
    baseTypes.insert("double", 0);
    baseTypes.insert("long double", 0);
    baseTypes.insert("unsigned long long", 0);
    baseTypes.insert("long long", 0);

    baseTypes2.insert("__int8", "char");
    baseTypes2.insert("__int16", "short");
    baseTypes2.insert("__int32", "int");
    baseTypes2.insert("__int64", "long long");
    baseTypes2.insert("unsigned __int8", "unsigned char");
    baseTypes2.insert("unsigned __int16", "unsigned short");
    baseTypes2.insert("unsigned __int32", "unsigned int");
    baseTypes2.insert("unsigned __int64", "unsigned long long");

    msvcDemangler = MSVCDemangler();

    setParent(parent);
}

PDB::~PDB()
{
    if (diaDataSource)
    {
        diaDataSource->Release();
    }

    if (global)
    {
        global->Release();
    }

    if (diaSession)
    {
        diaSession->Release();
    }

    CoUninitialize();
}

bool PDB::ReadFromFile(const QString& filePath)
{
    HRESULT hr = CoInitialize(nullptr);

    hr = CoCreateInstance(__uuidof(DiaSource), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IDiaDataSource),
        reinterpret_cast<void**>(&diaDataSource));

    if (FAILED(hr))
    {
        emit SendStatusMessage("Can't load msdia library!");

        return false;
    }

    QByteArray buffer2;
    buffer2.resize((filePath.length() + 1) * 2);
    buffer2.fill(0);

    wchar_t* filePath2 = reinterpret_cast<wchar_t*>(buffer2.data());
    filePath.toWCharArray(filePath2);

    hr = diaDataSource->loadDataFromPdb(filePath2);

    if (FAILED(hr))
    {
        emit SendStatusMessage("Can't load data from PDB!");

        return false;
    }

    hr = diaDataSource->openSession(&diaSession);

    if (FAILED(hr))
    {
        emit SendStatusMessage("Can't open session!");

        return false;
    }

    hr = diaSession->get_globalScope(&global);

    if (FAILED(hr))
    {
        emit SendStatusMessage("Can't get global scope!");

        return false;
    }

    return true;
}

void PDB::SetMachineType(CV_CPU_TYPE_e type)
{
    this->type = type;
}

void PDB::LoadPDBData()
{
    processEnabled = true;

    diaSymbols->clear();
    symbolRecords->clear();

    IDiaEnumSymbols* udtSymbols;
    IDiaEnumSymbols* enumSymbols;
    LONG udtCount;
    LONG enumCount;
    LONG count;

    if (global->findChildren(SymTagUDT, nullptr, nsNone, &udtSymbols) != S_OK ||
        global->findChildren(SymTagEnum, nullptr, nsNone, &enumSymbols) != S_OK)
    {
        return;
    }

    if (udtSymbols->get_Count(&udtCount) != S_OK || !udtCount ||
        enumSymbols->get_Count(&enumCount) != S_OK || !enumCount)
    {
        return;
    }

    count = udtCount + enumCount;

    emit SetProgressMinimum(0);
    emit SetProgressMaximum(count);

    int currentIndex = 0;
    int currentProcent = 0;
    int procent = count / 100;

    GetSymbolsFromTable(udtSymbols, &currentIndex, &currentProcent, &procent);
    GetSymbolsFromTable(enumSymbols, &currentIndex, &currentProcent, &procent);

    udtSymbols->Release();
    enumSymbols->Release();

    emit Completed();
}

/*
* If object of class / struct is not created anywhere in code but they were created for inner types
* then only members will be displayed - type_name::member_name (type_name won't be displayed)
* If Multi-processor Compilation is enabled then classes/structs which are not used will also appear
* If namespaces are used they also won't be displayed
*/
void PDB::GetSymbolsFromTable(IDiaEnumSymbols* enumSymbols, int* currentIndex, int* currentProcent, int* procent)
{
    IDiaSymbol* symbol;
    ULONG celt = 0;

    while (SUCCEEDED(enumSymbols->Next(1, &symbol, &celt)) && (celt == 1) && processEnabled)
    {
        SymbolRecord symbolRecord = {};
        BSTR bstring;
        DWORD symTag = 0;

        symbol->get_symTag(&symTag);

        if (symTag == SymTagUDT)
        {
            DWORD parentClass = 0;

            symbol->get_classParentId(&parentClass);

            if (parentClass == 0)
            {
                if (symbol->get_name(&bstring) == S_OK)
                {
                    symbolRecord.typeName = QString::fromWCharArray(bstring);

                    SysFreeString(bstring);
                }

                ULONGLONG length;

                symbol->get_length(&length);

                if ((length == 0 && options->displayEmptyUDTAndEnums || length > 0) &&
                    !diaSymbols->contains(symbolRecord.typeName) &&
                    !symbolRecord.typeName.contains("::__cta"))
                {
                    DWORD kind = 0;
                    symbol->get_udtKind(&kind);

                    UdtKind udtKind = (UdtKind)kind;

                    symbol->get_symIndexId(&symbolRecord.id);

                    if (udtKind == UdtStruct)
                    {
                        symbolRecord.type = SymbolType::structType;

                        structsCount++;
                    }
                    else if (udtKind == UdtClass)
                    {
                        symbolRecord.type = SymbolType::classType;

                        classesCount++;
                    }
                    else if (udtKind == UdtUnion)
                    {
                        symbolRecord.type = SymbolType::unionType;

                        unionsCount++;
                    }
                    else if (udtKind == UdtInterface)
                    {
                        symbolRecord.type = SymbolType::interfaceType;

                        interfacesCount++;
                    }

                    symbolRecords->push_back(symbolRecord);
                    diaSymbols->insert(symbolRecord.typeName, symbolRecord.id);
                }
            }
        }
        else if (symTag == SymTagEnum)
        {
            DWORD dwParentClass = 0;

            symbol->get_classParentId(&dwParentClass);

            if (dwParentClass == 0)
            {
                if (symbol->get_name(&bstring) == S_OK)
                {
                    symbolRecord.typeName = QString::fromWCharArray(bstring);

                    SysFreeString(bstring);
                }

                ULONGLONG length;

                symbol->get_length(&length);

                if ((length == 0 && options->displayEmptyUDTAndEnums || length > 0) &&
                    !diaSymbols->contains(symbolRecord.typeName))
                {
                    symbol->get_symIndexId(&symbolRecord.id);
                    symbolRecord.type = SymbolType::enumType;

                    symbolRecords->push_back(symbolRecord);
                    diaSymbols->insert(symbolRecord.typeName, symbolRecord.id);

                    enumsCount++;
                }
            }
        }
        else if (symTag == SymTagData)
        {
            if (symbol->get_name(&bstring) == S_OK)
            {
                QString name = QString::fromWCharArray(bstring);

                SysFreeString(bstring);

                DWORD kind;
                DataKind dataKind;

                symbol->get_dataKind(&kind);
                dataKind = static_cast<DataKind>(kind);

                if (!name.startsWith("$") &&
                    !diaSymbols->contains(name) &&
                    (dataKind == DataIsFileStatic ||
                        dataKind == DataIsGlobal ||
                        dataKind == DataIsMember ||
                        dataKind == DataIsStaticMember ||
                        dataKind == DataIsConstant))
                {
                    DWORD id;

                    symbol->get_symIndexId(&id);

                    variables->insert(name, id);
                }
            }
        }
        else if (symTag == SymTagFunction)
        {
            if (symbol->get_name(&bstring) == S_OK)
            {
                QString name = QString::fromWCharArray(bstring);

                SysFreeString(bstring);

                if (!diaSymbols->contains(name))
                {
                    DWORD id;

                    symbol->get_symIndexId(&id);
                    functions->insert(name, id);
                }
            }
        }
        else if (symTag == SymTagPublicSymbol)
        {
            DWORD id;

            symbol->get_symIndexId(&id);

			/*if (symbol->get_name(&bstring) == S_OK)
			{
				QString mangledName = QString::fromWCharArray(bstring);

				SysFreeString(bstring);

				publicSymbols2->insert(id, mangledName);
			}*/

            QString demangledName;

            if (options->useUndname)
            {
				if (symbol->get_undecoratedName(&bstring) == S_OK)
				{
					demangledName = QString::fromWCharArray(bstring);

					SysFreeString(bstring);
				}
            }
            else
            {
				if (symbol->get_name(&bstring) == S_OK)
				{
                    std::string mangledName = QString::fromWCharArray(bstring).toStdString();
                    std::string rest;

                    demangledName = QString::fromStdString(msvcDemangler.DemangleSymbol(mangledName, rest));

                    SysFreeString(bstring);
				}
            }

            publicSymbols->insert(demangledName, id);
        }

        symbol->Release();

        if (*currentIndex > *currentProcent * *procent)
        {
            ++*currentProcent;
            emit SetProgressValue(*currentIndex);
        }

        ++*currentIndex;
    }
}

int PDB::GetCountOfClasses()
{
    return classesCount;
}

int PDB::GetCountOfStructs()
{
    return structsCount;
}

int PDB::GetCountOfInterfaces()
{
    return interfacesCount;
}

int PDB::GetCountOfUnions()
{
    return unionsCount;
}

int PDB::GetCountOfEnums()
{
    return enumsCount;
}

void PDB::Stop()
{
    processEnabled = false;
}

UDT PDB::GetUDT(IDiaSymbol* symbol)
{
    UDT udt = {};
    BSTR bString = nullptr;
    DWORD udtKind = 0, access = 0, symIndexID = 0, parentClassID = 0;
    ULONGLONG length = 0;
    BOOL isNested = 0, isConst = 0, isVolatile = 0, hasCastOperator = 0;
    IDiaSymbol* parentClass;

    if (symbol->get_name(&bString) == S_OK)
    {
        udt.name = QString::fromWCharArray(bString);
        udt.originalTypeName = udt.name;

        SysFreeString(bString);
    }

    if (symbol->get_udtKind(&udtKind) == S_OK)
    {
        udt.udtKind = static_cast<UdtKind>(udtKind);

        switch (udt.udtKind)
        {
        case UdtStruct:
            udt.type = "struct";
            break;
        case UdtClass:
            udt.type = "class";
            break;
        case UdtUnion:
            udt.type = "union";
            break;
        case UdtInterface:
            udt.type = "interface";
            break;
        }
    }

    if (symbol->get_length(&length) == S_OK)
    {
        udt.length = length;
    }

    if (symbol->get_constType(&isConst) == S_OK)
    {
        udt.isConst = isConst;
    }

    if (symbol->get_volatileType(&isVolatile) == S_OK)
    {
        udt.isVolatile = isVolatile;
    }

    if (symbol->get_classParent(&parentClass) == S_OK)
    {
        if (parentClass->get_name(&bString) == S_OK)
        {
            udt.parentClassName = QString::fromWCharArray(bString);

            SysFreeString(bString);
        }

        parentClass->Release();
    }

    if (symbol->get_nested(&isNested) == S_OK)
    {
        udt.isNested = isNested;
    }

    IDiaSymbol* unmodifiedType;

    if (symbol->get_unmodifiedType(&unmodifiedType) == S_OK)
    {
        IDiaSymbol* vTableShape;

        if (unmodifiedType->get_virtualTableShape(&vTableShape) == S_OK)
        {
            DWORD count;

            if (vTableShape->get_count(&count) == S_OK)
            {
                udt.countOfVirtualFunctions = count;
            }

            vTableShape->Release();
        }

        unmodifiedType->Release();
    }

    if (symbol->get_hasCastOperator(&hasCastOperator) == S_OK)
    {
        udt.hasCastOperator = hasCastOperator;
    }

    if (udt.name.contains("<unnamed"))
    {
        udt.isUnnamed = true;
    }

    //Sometimes DIA SDK won't undecorate name
    if (udt.name.at(0) == '?')
    {
        if (options->useUndname)
        {
            std::string undecoratedName = peHeaderParser->UndecorateName(udt.name.toStdString().c_str());

            udt.name = QString::fromStdString(undecoratedName);
        }
        else
        {
            std::string mangledName = udt.name.toStdString().c_str();
            std::string rest;

            udt.name = QString::fromStdString(msvcDemangler.DemangleSymbol(mangledName, rest));
        }
    }

    FormatString(udt.name);
    FormatString(udt.parentClassName);

    //Type of access specifier is not available for udts
    udt.access = CV_access_e::CV_public;

    return udt;
}

Function PDB::GetFunction(IDiaSymbol* symbol)
{
    Function function = {};
    BSTR bString = nullptr;
    DWORD access = 0, addressSection = 0, relativeVirtualAddress = 0, virtualBaseOffset = 0, callingConvention = 0;
    ULONGLONG length = 0, virtualAddress = 0;
    BOOL isCompilerGenerated = 0, isNaked = 0, isStatic = 0, isNoInline = 0, isNotReached = 0;
    BOOL isNoReturn = 0, isPure = 0, isConst = 0, isVolatile = 0, isVirtual = 0, isIntroVirtual = 0, hasCustomCallingConvention = 0;
    BOOL hasAlloca = 0, hasEH = 0, hasEHa = 0, hasInlAsm = 0, hasLongJump = 0, hasSecurityChecks = 0, hasSEH = 0, hasSetJump = 0;
    BOOL hasInlSpec = 0, hasOptimizedCodeDebugInfo = 0, farReturn = 0, interruptReturn = 0, noStackOrdering = 0;
    IDiaSymbol* parentClass;

    if (symbol->get_access(&access) == S_OK)
    {
        function.access = static_cast<CV_access_e>(access);
    }

    if (symbol->get_addressSection(&addressSection) == S_OK)
    {
        function.addressSection = addressSection;
    }

    if (symbol->get_compilerGenerated(&isCompilerGenerated) == S_OK)
    {
        function.isCompilerGenerated = isCompilerGenerated;
    }

    if (symbol->get_isNaked(&isNaked) == S_OK)
    {
        function.isNaked = isNaked;
    }

    if (symbol->get_isStatic(&isStatic) == S_OK)
    {
        function.isStatic = isStatic;
    }

    if (symbol->get_noInline(&isNoInline) == S_OK)
    {
        function.isNoInline = isNoInline;
    }

    if (symbol->get_notReached(&isNotReached) == S_OK)
    {
        function.isNotReached = isNotReached;
    }

    if (symbol->get_noReturn(&isNoReturn) == S_OK)
    {
        function.isNoReturn = isNoReturn;
    }

    if (symbol->get_length(&length) == S_OK)
    {
        function.length = length;
    }

    if (symbol->get_name(&bString) == S_OK)
    {
        function.name = QString::fromWCharArray(bString);
        SysFreeString(bString);
    }

    if (symbol->get_pure(&isPure) == S_OK)
    {
        function.isPure = isPure;
    }

    if (symbol->get_virtualAddress(&virtualAddress) == S_OK)
    {
        function.virtualAddress = virtualAddress;
    }

    if (symbol->get_relativeVirtualAddress(&relativeVirtualAddress) == S_OK)
    {
        function.relativeVirtualAddress = relativeVirtualAddress;
    }

    if (symbol->get_virtual(&isVirtual) == S_OK)
    {
        function.isVirtual = isVirtual;
    }

    if (symbol->get_virtualBaseOffset(&virtualBaseOffset) == S_OK)
    {
        function.virtualBaseOffset = virtualBaseOffset;
    }

    if (symbol->get_intro(&isIntroVirtual) == S_OK)
    {
        function.isIntroVirtual = isIntroVirtual;
    }

    if (symbol->get_customCallingConvention(&hasCustomCallingConvention) == S_OK)
    {
        function.hasCustomCallingConvention = hasCustomCallingConvention;
    }

    if (symbol->get_hasAlloca(&hasAlloca) == S_OK)
    {
        function.hasAlloca = hasAlloca;
    }

    if (symbol->get_hasEH(&hasEH) == S_OK)
    {
        function.hasEH = hasEH;
    }

    if (symbol->get_hasEHa(&hasEHa) == S_OK)
    {
        function.hasEHa = hasEHa;
    }

    if (symbol->get_hasInlAsm(&hasInlAsm) == S_OK)
    {
        function.hasInlAsm = hasInlAsm;
    }

    if (symbol->get_hasLongJump(&hasLongJump) == S_OK)
    {
        function.hasLongJump = hasLongJump;
    }

    if (symbol->get_hasSecurityChecks(&hasSecurityChecks) == S_OK)
    {
        function.hasSecurityChecks = hasSecurityChecks;
    }

    if (symbol->get_hasSEH(&hasSEH) == S_OK)
    {
        function.hasSEH = hasSEH;
    }

    if (symbol->get_hasSetJump(&hasSetJump) == S_OK)
    {
        function.hasSetJump = hasSetJump;
    }

    if (symbol->get_hasSecurityChecks(&hasSecurityChecks) == S_OK)
    {
        function.hasSecurityChecks = hasSecurityChecks;
    }

    if (symbol->get_inlSpec(&hasInlSpec) == S_OK)
    {
        function.hasInlSpec = hasInlSpec;
    }

    if (symbol->get_optimizedCodeDebugInfo(&hasOptimizedCodeDebugInfo) == S_OK)
    {
        function.hasOptimizedCodeDebugInfo = hasOptimizedCodeDebugInfo;
    }

    if (symbol->get_farReturn(&farReturn) == S_OK)
    {
        function.farReturn = farReturn;
    }

    if (symbol->get_interruptReturn(&interruptReturn) == S_OK)
    {
        function.interruptReturn = interruptReturn;
    }

    if (symbol->get_noStackOrdering(&noStackOrdering) == S_OK)
    {
        function.noStackOrdering = noStackOrdering;
    }

    if (symbol->get_classParent(&parentClass) == S_OK)
    {
        if (parentClass->get_name(&bString) == S_OK)
        {
            function.parentClassName = QString::fromWCharArray(bString);
            function.originalParentClassName = function.parentClassName;

            FormatString(function.parentClassName);
            SysFreeString(bString);
        }

        DWORD udtKind = 0;

        if (parentClass->get_udtKind(&udtKind) == S_OK)
        {
            UdtKind udtKind2 = static_cast<UdtKind>(udtKind);

            switch (udtKind2)
            {
            case UdtStruct:
                function.parentType = "struct";
                break;
            case UdtClass:
                function.parentType = "class";
                break;
            case UdtUnion:
                function.parentType = "union";
                break;
            case UdtInterface:
                function.parentType = "interface";
                break;
            }
        }

        parentClass->Release();
    }

    if (access == 0)
    {
        function.parentType = "namespace";
    }

    QSet<QString> includes2;

    if (options->displayIncludes)
    {
        includes2 = includes;
    }

    QString parentClassName2 = parentClassName;

    parentClassName = function.parentClassName;

    RecordType recordType = GetRecordType(symbol);

    parentClassName = parentClassName2;

    function.isConst = recordType.isFunctionConst;
    function.isVolatile = recordType.isFunctionVolatile;
    function.isVariadic = recordType.isVariadicFunction;
    function.returnType1 = recordType.functionReturnType1;
    function.returnType2 = recordType.functionReturnType2;
    function.originalReturnType = recordType.originalFunctionReturnType;
    function.parametersCount = recordType.parametersCount;
    function.parameters = recordType.functionParameters;
    function.originalParameters = recordType.originalFunctionParameters;
    function.callingConvention = recordType.callingConvention;
    function.returnsFunctionPointer = recordType.functionReturnsFunctionPointer;

    //Sometimes DIA SDK won't undecorate name
    if (function.name.at(0) == '?')
    {
        if (options->useUndname)
        {
            std::string undecoratedName = peHeaderParser->UndecorateName(function.name.toStdString().c_str());

            function.name = QString::fromStdString(undecoratedName);
        }
        else
        {
            std::string mangledName = function.name.toStdString().c_str();
            std::string rest;

            function.name = QString::fromStdString(msvcDemangler.DemangleSymbol(mangledName, rest));
        }
    }

    FormatString(function.name);

    QString parentClassName = function.parentClassName;

    //Don't check options->removeScopeResolutionOperator here because scope operators always need to removed from parentClassName
    if (parentClassName.contains("::"))
    {
        RemoveScopeResolutionOperators(parentClassName, "");
    }

    if (function.name == parentClassName)
    {
        //Use contains instead of == because of const
        if (function.parameters.count() == 1 &&
            function.parameters.at(0).contains(QString("%1&").arg(parentClassName)))
        {
            function.isCopyConstructor = true;
        }
        else if (function.parameters.count() == 0)
        {
            function.isDefaultConstructor = true;
        }
        else
        {
            function.isConstructor = true;
        }
    }
    else if (function.name.at(0) == '~')
    {
        function.isDestructor = true;
    }
    else if (function.name == "operator=")
    {
        function.isCopyAssignmentOperator = true;
    }

    DWORD functionOffset = function.relativeVirtualAddress;

    if (functionOffset == 0 &&
        !options->displayNonImplementedFunctions &&
        !(function.isVirtual && function.isPure) &&
        !function.isGeneratedByApp &&
        !function.isDefaultConstructor &&
        !function.isDestructor &&
        !(function.isCopyConstructor &&
            options->applyRuleOfThree) &&
        !(function.isCopyAssignmentOperator &&
            options->applyRuleOfThree))
    {
        function.isNonImplemented = true;
    }

    if (function.isNonImplemented && options->displayIncludes && !options->displayNonImplementedFunctions)
    {
        if (includes.count() > includes2.count())
        {
            includes = includes2;
        }
    }

    return function;
}

RecordType PDB::GetRecordType(IDiaSymbol* symbol)
{
    RecordType recordType = {};
    IDiaSymbol* type = nullptr;

    if (symbol->get_type(&type) == S_OK)
    {
        recordType = GetType(type);
        type->Release();

        return recordType;
    }

    return recordType;
}

BaseType PDB::GetBaseType(IDiaSymbol* symbol)
{
    BaseType baseType = {};
    DWORD type = 0;
    ULONGLONG length = 0;
    BOOL isConst = 0, isVolatile = 0;

    if (symbol->get_baseType(&type) == S_OK)
    {
        baseType.type = type;
    }

    if (symbol->get_length(&length) == S_OK)
    {
        baseType.length = length;
    }

    if (symbol->get_constType(&isConst) == S_OK)
    {
        baseType.isConst = isConst;
    }

    if (symbol->get_volatileType(&isVolatile) == S_OK)
    {
        baseType.isVolatile = isVolatile;
    }

    return baseType;
}

PointerType PDB::GetPointerType(IDiaSymbol* symbol)
{
    PointerType pointerType = {};
    ULONGLONG length = 0;
    BOOL isReference = 0, isConst = 0, isVolatile = 0;

    if (symbol->get_length(&length) == S_OK)
    {
        pointerType.length = length;
    }

    if (symbol->get_reference(&isReference) == S_OK)
    {
        pointerType.isReference = isReference;
    }

    if (symbol->get_constType(&isConst) == S_OK)
    {
        pointerType.isConst = isConst;
    }

    if (symbol->get_volatileType(&isVolatile) == S_OK)
    {
        pointerType.isVolatile = isVolatile;
    }

    return pointerType;
}

ArrayType PDB::GetArrayType(IDiaSymbol* symbol)
{
    ArrayType arrayType = {};
    DWORD count = 0;
    ULONGLONG length = 0;
    BOOL isReference = 0, isConst = 0, isVolatile = 0;

    if (symbol->get_count(&count) == S_OK)
    {
        arrayType.count = count;
    }

    if (symbol->get_length(&length) == S_OK)
    {
        arrayType.length = length;
    }

    if (symbol->get_constType(&isConst) == S_OK)
    {
        arrayType.isConst = isConst;
    }

    if (symbol->get_volatileType(&isVolatile) == S_OK)
    {
        arrayType.isVolatile = isVolatile;
    }

    return arrayType;
}

Enum PDB::GetEnum(IDiaSymbol* symbol)
{
    Enum enum1 = {};
    BSTR bString = nullptr;
    DWORD baseType = 0, udtKind = 0, access = 0;
    ULONGLONG length = 0;
    BOOL isConst = 0, isVolatile = 0, isNested = 0;
    IDiaSymbol* parentClass;

    if (symbol->get_name(&bString) == S_OK)
    {
        enum1.name = QString::fromWCharArray(bString);
        enum1.originalTypeName = enum1.name;

        SysFreeString(bString);
    }

    if (symbol->get_baseType(&baseType) == S_OK)
    {
        enum1.baseType = baseType;
    }

    if (symbol->get_length(&length) == S_OK)
    {
        enum1.length = length;
    }

    if (symbol->get_constType(&isConst) == S_OK)
    {
        enum1.isConst = isConst;
    }

    if (symbol->get_volatileType(&isVolatile) == S_OK)
    {
        enum1.isVolatile = isVolatile;
    }

    if (symbol->get_classParent(&parentClass) == S_OK)
    {
        if (parentClass->get_name(&bString) == S_OK)
        {
            enum1.parentClassName = QString::fromWCharArray(bString);

            SysFreeString(bString);
        }

        parentClass->Release();
    }

    if (symbol->get_nested(&isNested) == S_OK)
    {
        enum1.isNested = isNested;
    }

    //Sometimes DIA SDK won't undecorate name
    if (enum1.name.at(0) == '?')
    {
        if (options->useUndname)
        {
            std::string undecoratedName = peHeaderParser->UndecorateName(enum1.name.toStdString().c_str());

            enum1.name = QString::fromStdString(undecoratedName);
        }
        else
        {
            std::string mangledName = enum1.name.toStdString().c_str();
            std::string rest;

            enum1.name = QString::fromStdString(msvcDemangler.DemangleSymbol(mangledName, rest));
        }
    }

    if (enum1.name.endsWith("-tag>"))
    {
        enum1.isAnonymous = true;
    }
    else if (enum1.name.contains("<unnamed"))
    {
        enum1.isUnnamed = true;
    }

    //Type of access specifier is not available for enums
    enum1.access = CV_access_e::CV_public;

    FormatString(enum1.parentClassName);

    return enum1;
}

FunctionType PDB::GetFunctionType(IDiaSymbol* symbol)
{
    FunctionType functionType = {};
    DWORD callingConvention = 0, parametersCount = 0;
    BOOL isConst = 0, isVolatile = 0;

    if (symbol->get_callingConvention(&callingConvention) == S_OK)
    {
        functionType.callingConvention = static_cast<CV_call_e>(callingConvention);
    }

    if (symbol->get_count(&parametersCount) == S_OK)
    {
        functionType.parametersCount = parametersCount;
    }

    IDiaSymbol* objectPointer;

    if (symbol->get_objectPointerType(&objectPointer) == S_OK)
    {
        DWORD symTag;

        if (objectPointer->get_symTag(&symTag) == S_OK)
        {
            if (symTag == SymTagPointerType)
            {
                IDiaSymbol* pointee;

                if (objectPointer->get_type(&pointee) == S_OK)
                {
                    if (pointee->get_constType(&isConst) == S_OK)
                    {
                        functionType.isConst = isConst;
                    }

                    if (pointee->get_volatileType(&isVolatile) == S_OK)
                    {
                        functionType.isVolatile = isVolatile;
                    }
                }
            }
        }
    }

    return functionType;
}

TypeDef PDB::GetTypeDef(IDiaSymbol* symbol)
{
    TypeDef typeDef = {};
    BSTR bString = nullptr;

    if (symbol->get_name(&bString) == S_OK)
    {
        typeDef.newTypeName = QString::fromWCharArray(bString);

        SysFreeString(bString);
    }

    RecordType recordType = GetRecordType(symbol);

    typeDef.oldTypeName = recordType.typeName;

    //Type of access specifier is not available for udts
    typeDef.access = CV_access_e::CV_private;

    if (options->removeScopeResolutionOperator && typeDef.oldTypeName.contains("::"))
    {
        RemoveScopeResolutionOperators(typeDef.oldTypeName, parentClassName);
    }

    return typeDef;
}

Value PDB::GetValue(IDiaSymbol* symbol)
{
    Value result = {};
    VARIANT value = {};

    value.vt = VT_EMPTY;

    if (symbol->get_value(&value) == S_OK)
    {
        result.isValid = true;

        switch (value.vt)
        {
        case VT_UI1:        
            result.value = value.bVal;
            break;
        case VT_UI2:        
            result.value = value.uiVal;  
            break;
        case VT_UI4:        
            result.value = (quint32)value.ulVal;
            break;
        case VT_UINT:       
            result.value = value.uintVal;          
            break;
        case VT_INT:        
            result.value = value.intVal;
            break;
        case VT_I1:         
            result.value = value.cVal;
            break;
        case VT_I2:         
            result.value = value.iVal;   
            break;
        case VT_I4:         
            result.value = (quint32)value.lVal;
            break;
        default:            
            emit SendStatusMessage("Unknown VARIANT.");

            break;
        }
    }

    return result;
}

Data PDB::GetData(IDiaSymbol* symbol)
{
    Data data = {};
    BSTR bString = nullptr;
    DWORD access = 0, bitOffset = 0, dataKind = 0, relativeVirtualAddress = 0, locationType = 0;
    ULONGLONG length = 0, virtualAddress = 0;
    LONG offset = 0;
    BOOL isCompilerGenerated = 0, isConst = 0, isVolatile = 0;
    IDiaSymbol* parentClass;

    if (symbol->get_access(&access) == S_OK)
    {
        data.access = static_cast<CV_access_e>(access);
    }

    if (symbol->get_bitPosition(&bitOffset) == S_OK)
    {
        data.bitOffset = bitOffset;
    }

    if (symbol->get_compilerGenerated(&isCompilerGenerated) == S_OK)
    {
        data.isCompilerGenerated = isCompilerGenerated;
    }

    if (symbol->get_dataKind(&dataKind) == S_OK)
    {
        data.dataKind = static_cast<DataKind>(dataKind);
    }

    if (symbol->get_length(&length) == S_OK)
    {
        data.length = length;
    }

    if (symbol->get_name(&bString) == S_OK)
    {
        data.name = QString::fromWCharArray(bString);
        SysFreeString(bString);
    }

    if (symbol->get_offset(&offset) == S_OK)
    {
        data.offset = offset;
    }

    if (symbol->get_virtualAddress(&virtualAddress) == S_OK)
    {
        data.virtualAddress = virtualAddress;
    }

    if (symbol->get_relativeVirtualAddress(&relativeVirtualAddress) == S_OK)
    {
        data.relativeVirtualAddress = relativeVirtualAddress;
    }

    if (symbol->get_constType(&isConst) == S_OK)
    {
        data.isTypeConst = isConst;
    }

    if (symbol->get_volatileType(&isVolatile) == S_OK)
    {
        data.isTypeVolatile = isVolatile;
    }

    if (symbol->get_classParent(&parentClass) == S_OK)
    {
        if (parentClass->get_name(&bString) == S_OK)
        {
            data.parentClassName = QString::fromWCharArray(bString);

            SysFreeString(bString);
        }

        DWORD udtKind = 0;

        if (parentClass->get_udtKind(&udtKind) == S_OK)
        {
            UdtKind udtKind2 = static_cast<UdtKind>(udtKind);

            switch (udtKind2)
            {
            case UdtStruct:
                data.parentType = "struct";

                break;
            case UdtClass:
                data.parentType = "class";

                break;
            case UdtUnion:
                data.parentType = "union";

                break;
            case UdtInterface:
                data.parentType = "interface";

                break;
            }
        }

        parentClass->Release();
    }

    if (access == 0 &&
        !(data.dataKind == DataIsGlobal ||
            data.dataKind == DataIsFileStatic ||
            data.dataKind == DataIsConstant))
    {
        data.parentType = "namespace";
    }

    if (symbol->get_locationType(&locationType) == S_OK)
    {
        data.locationType = static_cast<LocationType>(locationType);
    }

    data.location = GetLocation(symbol);
    data.value = GetValue(symbol);

    RecordType recordType = GetRecordType(symbol);

    data.baseType = recordType.baseType;

    data.isTypeConst = recordType.isTypeConst;
    data.isTypeVolatile = recordType.isTypeVolatile;
    data.isPointerConst = recordType.isPointerConst;
    data.isPointerVolatile = recordType.isPointerVolatile;
    data.originalTypeName = recordType.originalTypeName;
    data.typeName = recordType.typeName;
    data.size = recordType.size;
    data.isPointer = recordType.isPointer;
    data.isReference = recordType.isReference;
    data.pointerLevel = recordType.pointerLevel;
    data.referenceLevel = recordType.referenceLevel;
    data.isArray = recordType.isArray;
    data.arrayCount = recordType.arrayCount;
    data.functionReturnType = recordType.functionReturnType2;
    data.functionParameters = recordType.functionParameters;
    data.noType = recordType.noType;
    data.isTypeNameOfEnum = recordType.isTypeNameOfEnum;
    data.numberOfBits = data.length;
    //data.isFunctionPointer = recordType.isFunctionPointer;

    if (recordType.size > 0 && recordType.functionType)
    {
        data.isFunctionPointer = true;
    }

    //Sometimes DIA SDK won't undecorate name
    if (data.name.length() > 0 && data.name.at(0) == '?')
    {
        if (options->useUndname)
        {
            std::string undecoratedName = peHeaderParser->UndecorateName(data.name.toStdString().c_str());

            data.name = QString::fromStdString(undecoratedName);
        }
        else
        {
            std::string mangledName = data.name.toStdString().c_str();
            std::string rest;

            data.name = QString::fromStdString(msvcDemangler.DemangleSymbol(mangledName, rest));
        }
    }

    FormatString(data.typeName);
    FormatString(data.parentClassName);

    return data;
}

BaseClass PDB::GetBaseClass(IDiaSymbol* symbol)
{
    BaseClass baseClass = {};
    BSTR bString = nullptr;
    DWORD access = 0, udtKind = 0, virtualBaseDispIndex = 0;
    LONG offset = 0, virtualBasePointerOffset = 0;
    ULONGLONG length = 0;
    BOOL isVirtualBaseClass = 0, isIndirectVirtualBaseClass = 0, isConst = 0, isVolatile = 0;
    IDiaSymbol* parentClass;

    if (symbol->get_access(&access) == S_OK)
    {
        baseClass.access = static_cast<CV_access_e>(access);
    }

    if (symbol->get_name(&bString) == S_OK)
    {
        baseClass.name = QString::fromWCharArray(bString);

        SysFreeString(bString);
    }

    if (symbol->get_udtKind(&udtKind) == S_OK)
    {
        baseClass.udtKind = static_cast<UdtKind>(udtKind);
    }

    if (symbol->get_offset(&offset) == S_OK)
    {
        baseClass.offset = offset;
    }

    if (symbol->get_length(&length) == S_OK)
    {
        baseClass.length = length;
    }

    if (symbol->get_virtualBaseClass(&isVirtualBaseClass) == S_OK)
    {
        baseClass.isVirtualBaseClass = isVirtualBaseClass;
    }

    if (symbol->get_indirectVirtualBaseClass(&isIndirectVirtualBaseClass) == S_OK)
    {
        baseClass.isIndirectVirtualBaseClass = isIndirectVirtualBaseClass;
    }

    if (symbol->get_virtualBaseDispIndex(&virtualBaseDispIndex) == S_OK)
    {
        baseClass.virtualBaseDispIndex = virtualBaseDispIndex;
    }

    if (symbol->get_virtualBasePointerOffset(&virtualBasePointerOffset) == S_OK)
    {
        baseClass.virtualBasePointerOffset = virtualBasePointerOffset;
    }

    if (symbol->get_unalignedType(&isConst) == S_OK)
    {
        baseClass.isConst = isConst;
    }

    if (symbol->get_volatileType(&isVolatile) == S_OK)
    {
        baseClass.isVolatile = isVolatile;
    }

    if (symbol->get_classParent(&parentClass) == S_OK)
    {
        if (parentClass->get_name(&bString) == S_OK)
        {
            baseClass.parentClassName = QString::fromWCharArray(bString);

            SysFreeString(bString);
        }

        parentClass->Release();
    }

    //Sometimes DIA SDK won't undecorate name
    if (baseClass.name.at(0) == '?')
    {
        if (options->useUndname)
        {
            std::string undecoratedName = peHeaderParser->UndecorateName(baseClass.name.toStdString().c_str());

            baseClass.name = QString::fromStdString(undecoratedName);
        }
        else
        {
            std::string mangledName = baseClass.name.toStdString().c_str();
            std::string rest;

            baseClass.name = QString::fromStdString(msvcDemangler.DemangleSymbol(mangledName, rest));
        }
    }

    FormatString(baseClass.name);
    FormatString(baseClass.parentClassName);

    if (options->displayIncludes)
    {
        if (baseClass.parentClassName.length() == 0)
        {
            AddTypeNameToIncludesList(baseClass.name, parentClassName);
        }
        else
        {
            AddTypeNameToIncludesList(baseClass.name, baseClass.parentClassName);
        }
    }

    return baseClass;
}

PublicSymbol PDB::GetPublicSymbol(IDiaSymbol* symbol)
{
    PublicSymbol publicSymbol = {};
    BSTR bString = nullptr;
    DWORD addressOffset = 0, addressSection = 0, locationType = 0, relativeVirtualAddress = 0;
    BOOL isInCode = 0, isFunction = 0, isInManagedCode = 0, isInMSILCode = 0;
    ULONGLONG length = 0;

	if (symbol->get_name(&bString) == S_OK)
	{
        publicSymbol.decoratedName = QString::fromWCharArray(bString);

		SysFreeString(bString);
	}

    if (options->useUndname)
    {
        if (symbol->get_undecoratedName(&bString) == S_OK)
        {
            publicSymbol.undecoratedName = QString::fromWCharArray(bString);

            SysFreeString(bString);
        }
    }
    else
    {
		if (symbol->get_name(&bString) == S_OK)
		{
			std::string mangledName = QString::fromWCharArray(bString).toStdString();
			std::string rest;

			publicSymbol.undecoratedName = QString::fromStdString(msvcDemangler.DemangleSymbol(mangledName, rest));

            SysFreeString(bString);
		}
    }

	if (symbol->get_addressOffset(&addressOffset) == S_OK)
	{
        publicSymbol.addressOffset = addressOffset;
	}

	if (symbol->get_addressSection(&addressSection) == S_OK)
	{
        publicSymbol.addressSection = addressSection;
	}

	if (symbol->get_code(&isInCode) == S_OK)
	{
        publicSymbol.isInCode = isInCode;
	}

	if (symbol->get_function(&isFunction) == S_OK)
	{
        publicSymbol.isFunction = isFunction;
	}

	if (symbol->get_managed(&isInManagedCode) == S_OK)
	{
        publicSymbol.isInManagedCode = isInManagedCode;
	}

	if (symbol->get_msil(&isInMSILCode) == S_OK)
	{
        publicSymbol.isInMSILCode = isInMSILCode;
	}

	if (symbol->get_length(&length) == S_OK)
	{
        publicSymbol.length = length;
	}

	if (symbol->get_locationType(&locationType) == S_OK)
	{
        publicSymbol.locationType = static_cast<LocationType>(locationType);
	}

	if (symbol->get_relativeVirtualAddress(&relativeVirtualAddress) == S_OK)
	{
        publicSymbol.relativeVirtualAddress = relativeVirtualAddress;
	}

    return publicSymbol;
}

QString PDB::GetSourceFilePath(IDiaSymbol* symbol)
{
    QString sourceFilePath;
	ULONGLONG length = 0;

	if (symbol->get_length(&length) != S_OK)
	{
        return sourceFilePath;
	}

    DWORD relativeVirtualAddress = 0;
    IDiaEnumLineNumbers* lines = nullptr;
    bool succeeded = false;

	if (symbol->get_relativeVirtualAddress(&relativeVirtualAddress) == S_OK)
	{
		if (SUCCEEDED(diaSession->findLinesByRVA(relativeVirtualAddress, static_cast<DWORD>(length), &lines)))
		{
            succeeded = true;
		}
	}
	else
	{
		DWORD addressSection = 0;
		DWORD addressOffset = 0;

		if ((symbol->get_addressSection(&addressSection) == S_OK) &&
			(symbol->get_addressOffset(&addressOffset) == S_OK))
		{
			if (SUCCEEDED(diaSession->findLinesByAddr(addressSection, addressOffset, static_cast<DWORD>(length), &lines)))
			{
                succeeded = true;
			}
		}
	}

    if (succeeded)
    {
        IDiaLineNumber* line = nullptr;
        DWORD sourceFileID = 0;
        DWORD lastSourceFileID = (DWORD)(-1);

        if (lines->Item(0, &line) == S_OK &&
            line->get_sourceFileId(&sourceFileID) == S_OK)
		{
			if (sourceFileID != lastSourceFileID)
			{
				IDiaSourceFile* sourceFile;

				if (line->get_sourceFile(&sourceFile) == S_OK)
				{
					BSTR fileName;

					if (sourceFile->get_fileName(&fileName) == S_OK)
					{
						sourceFilePath = QString::fromWCharArray(fileName);

						SysFreeString(fileName);
					}

					sourceFile->Release();
				}
			}

            line->Release();
		}

        lines->Release();
    }

    return sourceFilePath;
}

QString PDB::GetSourceFileInfo(IDiaSourceFile* source)
{
    QString result;
    BSTR bstrSourceName;

    if (source->get_fileName(&bstrSourceName) == S_OK)
    {
        result += QString("\t%1").arg(QString::fromWCharArray(bstrSourceName));

        SysFreeString(bstrSourceName);
    }

    else
    {
        SendStatusMessage("ERROR - PrintSourceFile() get_fileName");

        return result;
    }

    BYTE checksum[256] = {};
    DWORD cbChecksum = sizeof(checksum);

    if (source->get_checksum(cbChecksum, &cbChecksum, checksum) == S_OK)
    {
        result += " (";

        DWORD checksumType;

        if (source->get_checksumType(&checksumType) == S_OK)
        {
            switch (checksumType)
            {
            case CHKSUM_TYPE_NONE:
                result += "None";

                break;

            case CHKSUM_TYPE_MD5:
                result += "MD5";

                break;

            case CHKSUM_TYPE_SHA1:
                result += "SHA1";

                break;

            default:
                result += QString("0x%1").arg(checksumType);

                break;
            }

            if (cbChecksum != 0)
            {
                result += ": ";
            }
        }

        for (DWORD ib = 0; ib < cbChecksum; ib++)
        {
            result += QString("%1").arg(checksum[ib]);
        }

        result += ")";
    }

    return result;
}

QString PDB::GetLines()
{
    QString result;
    IDiaEnumSymbols* enumSymbols;

    if (FAILED(global->findChildren(SymTagCompiland, NULL, nsNone, &enumSymbols)))
    {
        return result;
    }

    IDiaSymbol* compiland = nullptr;
    ULONG celt = 0;

    while (SUCCEEDED(enumSymbols->Next(1, &compiland, &celt)) && (celt == 1))
    {
        IDiaEnumSymbols* enumFunction = nullptr;

        if (SUCCEEDED(compiland->findChildren(SymTagFunction, NULL, nsNone, &enumFunction)))
        {
            IDiaSymbol* function;

            while (SUCCEEDED(enumFunction->Next(1, &function, &celt)) && (celt == 1))
            {
                DWORD symTag = 0, relativeVirtualAddress = 0;
                BSTR bstrName = nullptr;
                ULONGLONG length = 0;
                IDiaEnumLineNumbers* lines = nullptr;

                if ((function->get_symTag(&symTag) != S_OK) || (symTag != SymTagFunction))
                {
                    SendStatusMessage("ERROR - PrintLines() dwSymTag != SymTagFunction");

                    return result;
                }

                if (function->get_name(&bstrName) == S_OK)
                {
                    result += QString("%1\n\n").arg(QString::fromWCharArray(bstrName));

                    SysFreeString(bstrName);
                }

                if (function->get_length(&length) != S_OK)
                {
                    SendStatusMessage("ERROR - PrintLines() get_length");

                    return result;
                }

                if (function->get_relativeVirtualAddress(&relativeVirtualAddress) == S_OK)
                {
                    if (SUCCEEDED(diaSession->findLinesByRVA(relativeVirtualAddress, static_cast<DWORD>(length), &lines)))
                    {
                        result += GetLineInfo(lines);

                        lines->Release();
                    }
                }

                else
                {
                    DWORD addressSection = 0, addressOffset = 0;

                    if ((function->get_addressSection(&addressSection) == S_OK) &&
                        (function->get_addressOffset(&addressOffset) == S_OK))
                    {
                        if (SUCCEEDED(diaSession->findLinesByAddr(addressSection, addressOffset, static_cast<DWORD>(length), &lines)))
                        {
                            result += GetLineInfo(lines);

                            lines->Release();
                        }
                    }
                }

                function->Release();

                result += "\n";
            }

            enumFunction->Release();
        }

        compiland->Release();
    }

    enumSymbols->Release();

    return result;
}

QString PDB::GetLineInfo(IDiaEnumLineNumbers* lines)
{
    QString result;
    IDiaLineNumber* line = nullptr;
    DWORD celt = 0;
    DWORD relativeVirtualAddress = 0;
    DWORD addressSection = 0;
    DWORD addressOffset = 0;
    DWORD lineNumber = 0;
    DWORD srcID = 0;
    DWORD length = 0;

    DWORD lastSrcID = static_cast<DWORD>(-1);

    while (SUCCEEDED(lines->Next(1, &line, &celt)) && (celt == 1))
    {
        if ((line->get_relativeVirtualAddress(&relativeVirtualAddress) == S_OK) &&
            (line->get_addressSection(&addressSection) == S_OK) &&
            (line->get_addressOffset(&addressOffset) == S_OK) &&
            (line->get_lineNumber(&lineNumber) == S_OK) &&
            (line->get_sourceFileId(&srcID) == S_OK) &&
            (line->get_length(&length) == S_OK))
        {
            QString line2;

            line2.sprintf("\tline %u at [%08X][%04X:%08X], len = 0x%X", lineNumber, relativeVirtualAddress, addressSection,
                addressOffset, length);

            result += line2;

            if (srcID != lastSrcID)
            {
                IDiaSourceFile* source = nullptr;

                if (line->get_sourceFile(&source) == S_OK)
                {
                    result += GetSourceFileInfo(source);

                    lastSrcID = srcID;

                    source->Release();
                }
            }

            line->Release();

            result += "\n";
        }
    }

    return result;
}

QString PDB::GetLocation(IDiaSymbol* symbol)
{
    QString result;
    DWORD locationType = 0;
    DWORD relativeVirtualAddress = 0, addressSection = 0, addressOffset = 0, registerID = 0, bitPosition = 0, slot = 0;
    LONG offset = 0;
    ULONGLONG length = 0;
    VARIANT variant = { VT_EMPTY };

    if (symbol->get_locationType(&locationType) != S_OK)
    {
        // It must be a symbol in optimized code

        result = "Symbol is in optmized code.";

        return result;
    }

    LocationType locationType2 = static_cast<LocationType>(locationType);
    QString relativeVirtualAddress2, addressSection2, addressOffset2, bitPosition2, slot2, offset2, length2;

    switch (locationType2)
    {
    case LocIsStatic:
    {
        if ((symbol->get_relativeVirtualAddress(&relativeVirtualAddress) == S_OK) &&
            (symbol->get_addressSection(&addressSection) == S_OK) &&
            (symbol->get_addressOffset(&addressOffset) == S_OK))
        {
			relativeVirtualAddress2 = QString::number(relativeVirtualAddress, 16).toUpper();
			addressSection2 = QString::number(addressSection, 16).toUpper();
			addressOffset2 = QString::number(addressOffset, 16).toUpper();

            result = QString("%1, [0x%2][0x%3:0x%4]").arg(convertLocationTypeToString(locationType2)).arg(relativeVirtualAddress2)
                .arg(addressSection2).arg(addressOffset2);
        }

        break;
    }
    case LocIsTLS:
    case LocInMetaData:
    case LocIsIlRel:
    {
        if ((symbol->get_relativeVirtualAddress(&relativeVirtualAddress) == S_OK) &&
            (symbol->get_addressSection(&addressSection) == S_OK) &&
            (symbol->get_addressOffset(&addressOffset) == S_OK))
        {
			relativeVirtualAddress2 = QString::number(relativeVirtualAddress, 16).toUpper();
			addressSection2 = QString::number(addressSection, 16).toUpper();
			addressOffset2 = QString::number(addressOffset, 16).toUpper();

            result = QString("%1, [0x%2][0x%3:0x%4]").arg(convertLocationTypeToString(locationType2)).arg(relativeVirtualAddress2)
                .arg(addressSection2).arg(addressOffset2);
        }

        break;
    }
    case LocIsRegRel:
    {
        if ((symbol->get_registerId(&registerID) == S_OK) &&
            (symbol->get_offset(&offset) == S_OK))
        {
            offset2 = QString::number(offset, 16).toUpper();

            result = QString("%1 relative, [0x%2]").arg(QString::fromWCharArray(SzNameC7Reg(static_cast<USHORT>(registerID)))).arg(offset2);
        }

        break;
    }
    case LocIsThisRel:
    {
        if (symbol->get_offset(&offset) == S_OK)
        {
            offset2 = QString::number(offset, 16).toUpper();

            result = QString("this + 0x%1").arg(offset2);
        }

        break;
    }
    case LocIsBitField:
    {
        if ((symbol->get_offset(&offset) == S_OK) &&
            (symbol->get_bitPosition(&bitPosition) == S_OK) &&
            (symbol->get_length(&length) == S_OK))
        {
            offset2 = QString::number(offset, 16).toUpper();
            bitPosition2 = QString::number(bitPosition, 16).toUpper();
            length2 = QString::number(static_cast<ULONG>(length), 16).toUpper();

            result = QString("this(bf) + 0x%1:0x%2 len(0x%3)").arg(offset2).arg(bitPosition2).arg(length2);
        }

        break;
    }
    case LocIsEnregistered:
    {
        if (symbol->get_registerId(&registerID) == S_OK)
        {
            result = QString("Enregistered %1").arg(SzNameC7Reg(static_cast<USHORT>(registerID)));
        }

        break;
    }
    case LocIsSlot:
    {
        if (symbol->get_slot(&slot) == S_OK)
        {
            slot2 = QString::number(slot, 16).toUpper();

            result = QString("%1, [%2]").arg(convertLocationTypeToString(locationType2)).arg(slot2);
        }

        break;
    }
    case LocIsConstant:
    {
        result = "Constant";

        if (symbol->get_value(&variant) == S_OK)
        {
            result += convertVariantToString(variant);

            VariantClear(static_cast<VARIANTARG*>(&variant));
        }

        break;
    }
    case LocIsNull:
        break;
    default:
        result = QString("Error - invalid location type: 0x%1").arg(QString::number(locationType, 16).toUpper());

        break;
    }

    return result;
}

QString PDB::GetModules()
{
    QString modules;
    IDiaEnumSymbols* enumSymbols = nullptr;

    modules += GetModuleNames().join("");
    modules += "\n";

    if (FAILED(global->findChildren(SymTagCompiland, NULL, nsNone, &enumSymbols)))
    {
        return modules;
    }

    IDiaSymbol* compiland = nullptr;
    ULONG celt = 0;

    while (SUCCEEDED(enumSymbols->Next(1, &compiland, &celt)) && (celt == 1))
    {
        modules += "\nModule: ";

        BSTR bstrName;

        if (compiland->get_name(&bstrName) != S_OK)
        {
            modules += "(???)";
        }

        else
        {
            modules += QString::fromWCharArray(bstrName);

            SysFreeString(bstrName);
        }

        modules += "\n\n";

        IDiaEnumSymbols* enumChildren = nullptr;

        if (SUCCEEDED(compiland->findChildren(SymTagNull, NULL, nsNone, &enumChildren)))
        {
            IDiaSymbol* symbol = nullptr;
            ULONG celtChildren = 0;

            while (SUCCEEDED(enumChildren->Next(1, &symbol, &celtChildren)) && (celtChildren == 1))
            {
				DWORD symTag;

				if (symbol->get_symTag(&symTag) == S_OK)
				{
                    if (symTag == SymTagCompilandDetails)
                    {
                        modules += QString("Compiland details:\n%1\n").arg(GetCompilandDetails(symbol));
                    }
                    else if (symTag == SymTagCompilandEnv)
                    {
                        modules += QString("Compiland environment: %1\n").arg(GetCompilandEnvironment(symbol));
                    }
                    else if (symTag == SymTagExport)
                    {
                        modules += QString("Export: %1\n").arg(GetExportName(symbol));
                    }
				}

                symbol->Release();
            }

            enumChildren->Release();
        }

        compiland->Release();

        modules += "\n-------------------------------------------------";
        modules += "-------------------------------------------------\n";
    }

    enumSymbols->Release();

    return modules;
}

QStringList PDB::GetModuleNames()
{
    QStringList moduleNames;
    IDiaEnumSymbols* enumSymbols = nullptr;

    if (FAILED(global->findChildren(SymTagCompiland, NULL, nsNone, &enumSymbols)))
    {
        return moduleNames;
    }

    IDiaSymbol* compiland = nullptr;
    ULONG celt = 0, moduleNumber = 1;

    while (SUCCEEDED(enumSymbols->Next(1, &compiland, &celt)) && (celt == 1))
    {
        BSTR bstrName;

        if (compiland->get_name(&bstrName) != S_OK)
        {
            SendStatusMessage("ERROR - Failed to get the compiland's name.");

            compiland->Release();
            enumSymbols->Release();

            return moduleNames;
        }

        QString moduleNumber2 = QString("%1").arg(moduleNumber++, 4, 16).toUpper();

        moduleNames.append(QString("%1 %2\n").arg(moduleNumber2).arg(QString::fromWCharArray(bstrName)));

        SysFreeString(bstrName);

        compiland->Release();
    }

    enumSymbols->Release();

    return moduleNames;
}

QString PDB::GetCompilandDetails(IDiaSymbol* symbol)
{
    QString compilandDetails;
    DWORD language = 0, platform = 0;
    BOOL editAndContinueEnabled = 0, hasDebugInfo = 0, isLTCG = 0, isDataAligned = 0, hasManagedCode = 0, hasSecurityChecks = 0;
    BOOL isSdl, isHotpatchable = 0, isCVTCIL = 0, isMSILNetmodule = 0;
    DWORD majorVersion = 0, minorVersion = 0, buildVersion = 0, qfeVersion = 0;
    BSTR bstrCompilerName;

    if (symbol->get_language(&language) == S_OK)
    {
        compilandDetails += QString("\tLanguage: %1\n").arg(convertLanguageToString(static_cast<CV_CFL_LANG>(language)));
    }

    if (symbol->get_platform(&platform) == S_OK)
    {
        compilandDetails += QString("\tTarget processor: %1\n").arg(convertPlatformToString(static_cast<CV_CPU_TYPE_e>(platform)));
    }

    if (symbol->get_editAndContinueEnabled(&editAndContinueEnabled) == S_OK)
    {
        if (editAndContinueEnabled)
        {
            compilandDetails += "\tCompiled for edit and continue: Yes\n";
        }
        else
        {
            compilandDetails += "\tCompiled for edit and continue: No\n";
        }
    }

    if (symbol->get_hasDebugInfo(&hasDebugInfo) == S_OK)
    {
        if (hasDebugInfo)
        {
            compilandDetails += "\tCompiled without debugging info: No\n";
        }
        else
        {
            compilandDetails += "\tCompiled without debugging info: Yes\n";
        }
    }

    if (symbol->get_isLTCG(&isLTCG) == S_OK)
    {
        if (isLTCG)
        {
            compilandDetails += "\tCompiled with LTCG: Tes\n";
        }
        else
        {
            compilandDetails += "\tCompiled with LTCG: No\n";
        }
    }

    if (symbol->get_isDataAligned(&isDataAligned) == S_OK)
    {
        if (isDataAligned)
        {
            compilandDetails += "\tCompiled with /bzalign: No\n";
        }
        else
        {
            compilandDetails += "\tCompiled with /bzalign: Yes\n";
        }
    }

    if (symbol->get_hasManagedCode(&hasManagedCode) == S_OK)
    {
        if (hasManagedCode)
        {
            compilandDetails += "\tManaged code present: Yes\n";
        }

        else
        {
            compilandDetails += "\tManaged code present: No\n";
        }
    }

    if (symbol->get_hasSecurityChecks(&hasSecurityChecks) == S_OK)
    {
        if (hasSecurityChecks)
        {
            compilandDetails += "\tCompiled with /GS: Yes\n";
        }

        else
        {
            compilandDetails += "\tCompiled with /GS: No\n";
        }
    }

    if (symbol->get_isSdl(&isSdl) == S_OK)
    {
        if (isSdl)
        {
            compilandDetails += "\tCompiled with /sdl: Yes\n";
        }

        else
        {
            compilandDetails += "\tCompiled with /sdl: No\n";
        }
    }

    if (symbol->get_isHotpatchable(&isHotpatchable) == S_OK)
    {
        if (isHotpatchable)
        {
            compilandDetails += "\tCompiled with /hotpatch: Yes\n";
        }

        else
        {
            compilandDetails += "\tCompiled with /hotpatch: No\n";
        }
    }

    if (symbol->get_isCVTCIL(&isCVTCIL) == S_OK)
    {
        if (isCVTCIL)
        {
            compilandDetails += "\tConverted by CVTCIL: Yes\n";
        }

        else
        {
            compilandDetails += "\tConverted by CVTCIL: No\n";
        }
    }

    if (symbol->get_isMSILNetmodule(&isMSILNetmodule) == S_OK)
    {
        if (isMSILNetmodule)
        {
            compilandDetails += "\tMSIL module: Yes\n";
        }

        else
        {
            compilandDetails += "\tMSIL module: No\n";
        }
    }

    if ((symbol->get_frontEndMajor(&majorVersion) == S_OK) &&
        (symbol->get_frontEndMinor(&minorVersion) == S_OK) &&
        (symbol->get_frontEndBuild(&buildVersion) == S_OK))
    {
        compilandDetails += QString("\tFrontend Version: Major = %1, Minor = %2, Build = %3").arg(majorVersion)
            .arg(minorVersion).arg(buildVersion);

        if (symbol->get_frontEndQFE(&qfeVersion) == S_OK)
        {
            compilandDetails += QString(", QFE = %1").arg(qfeVersion);
        }

        compilandDetails += "\n";
    }

    if ((symbol->get_backEndMajor(&majorVersion) == S_OK) &&
        (symbol->get_backEndMinor(&minorVersion) == S_OK) &&
        (symbol->get_backEndBuild(&buildVersion) == S_OK))
    {
        compilandDetails += QString("\tBackend Version: Major = %1, Minor = %2, Build = %3").arg(majorVersion)
            .arg(minorVersion).arg(buildVersion);

        if (symbol->get_backEndQFE(&qfeVersion) == S_OK)
        {
            compilandDetails += QString(", QFE = %1").arg(qfeVersion);
        }

        compilandDetails += "\n";
    }

    if (symbol->get_compilerName(&bstrCompilerName) == S_OK)
    {
        if (bstrCompilerName)
        {
            compilandDetails += QString("\tVersion std::string: %1").arg(QString::fromWCharArray(bstrCompilerName));

            SysFreeString(bstrCompilerName);
        }
    }

    compilandDetails += "\n";

    return compilandDetails;
}

QString PDB::GetCompilandEnvironment(IDiaSymbol* symbol)
{
    QString compilandEnvironment;
    BSTR bstrName;

    if (symbol->get_name(&bstrName) == S_OK)
    {
        compilandEnvironment += QString::fromWCharArray(bstrName);

        SysFreeString(bstrName);
    }
    else
    {
        compilandEnvironment += "(none)";
    }

    compilandEnvironment += " =";

    VARIANT variant = { VT_EMPTY };

	if (symbol->get_value(&variant) == S_OK)
	{
        compilandEnvironment += convertVariantToString(variant);

        VariantClear(static_cast<VARIANTARG*>(&variant));
	}

    return compilandEnvironment;
}

QString PDB::GetExportName(IDiaSymbol* symbol)
{
    QString result;
    BSTR bstrName;
    BSTR bstrUndName;

    if (symbol->get_name(&bstrName) != S_OK)
    {
        result += "(none)";

        return result;
    }

    if (options->useUndname)
    {
        if (symbol->get_undecoratedName(&bstrUndName) == S_OK)
        {
            if (wcscmp(bstrName, bstrUndName) == 0)
            {
                result += QString("%1").arg(QString::fromWCharArray(bstrName));
            }
            else
            {
                result += QString("%1(%2)").arg(QString::fromWCharArray(bstrUndName)).arg(QString::fromWCharArray(bstrName));
            }

            SysFreeString(bstrUndName);
        }
		else
		{
			result += QString("%1").arg(QString::fromWCharArray(bstrName));
		}
    }
    else
    {
		std::string mangledName = QString::fromWCharArray(bstrName).toStdString();
		std::string rest;

        std::string demangledName = msvcDemangler.DemangleSymbol(mangledName, rest);

        if (mangledName == demangledName)
		{
			result += QString("%1").arg(QString::fromStdString(mangledName));
		}
        else
        {
            result += QString("%1(%2)").arg(QString::fromStdString(demangledName)).arg(QString::fromStdString(mangledName));
        }
    }

    SysFreeString(bstrName);

    return result;
}

RecordType PDB::GetType(IDiaSymbol* symbol)
{
    RecordType recordType = {};
    DWORD symTag;

    if (!symbol || symbol->get_symTag(&symTag) != S_OK)
    {
        return recordType;
    }

    switch (symTag)
    {
    case SymTagBaseType:
    {
        BaseType baseType = GetBaseType(symbol);

        recordType.size = baseType.length;
        recordType.isTypeConst = baseType.isConst;
        recordType.isTypeVolatile = baseType.isVolatile;
        recordType.baseType = baseType.type;

        switch (recordType.baseType)
        {
        case 0: 
            recordType.typeName = "<btNoType>";
            recordType.noType = true;

            break;
        case 1: recordType.typeName = "void"; break;
        case 2: recordType.typeName = "char"; break;
        case 3: recordType.typeName = "wchar_t"; break;
        case 4: recordType.typeName = "signed char"; break;
        case 5: recordType.typeName = "unsigned char"; break;
        case 6: recordType.typeName = "int"; break;
        case 7: recordType.typeName = "unsigned int"; break;
        case 8: recordType.typeName = "float"; break;
        case 9: recordType.typeName = "BCD"; break;
        case 10: recordType.typeName = "bool"; break;
        case 11: recordType.typeName = "short"; break;
        case 12: recordType.typeName = "unsigned short"; break;
        case 13: recordType.typeName = "long"; break;
        case 14: recordType.typeName = "unsigned long"; break;
        case 15: recordType.typeName = "__int8"; break;
        case 16: recordType.typeName = "__int16"; break;
        case 17: recordType.typeName = "__int32"; break;
        case 18: recordType.typeName = "__int64"; break;
        case 19: recordType.typeName = "__int128"; break;
        case 20: recordType.typeName = "unsigned __int8"; break;
        case 21: recordType.typeName = "unsigned __int16"; break;
        case 22: recordType.typeName = "unsigned __int32"; break;
        case 23: recordType.typeName = "unsigned __int64"; break;
        case 24: recordType.typeName = "unsigned __int128"; break;
        case 25: recordType.typeName = "CURRENCY"; break;
        case 26: recordType.typeName = "DATE"; break;
        case 27: recordType.typeName = "VARIANT"; break;
        case 28: recordType.typeName = "COMPLEX"; break;
        case 29: recordType.typeName = "BIT"; break;
        case 30: recordType.typeName = "BSTR"; break;
        case 31: recordType.typeName = "HRESULT"; break;
        case 32: recordType.typeName = "char16_t"; break;
        case 33: recordType.typeName = "char32_t"; break;
        }

        if ((recordType.baseType == 8) && (recordType.size != 4))
        {
            switch (recordType.size)
            {
            case 8: recordType.typeName = "double"; break;
            case 12: recordType.typeName = "long double"; break;
            }
        }

        if (((recordType.baseType == 7) || (recordType.baseType == 14)) && (recordType.size != 4)) // "unsigned int"
        {
            switch (recordType.size)
            {
            case 1: recordType.typeName = "unsigned char"; break;
            case 2: recordType.typeName = "unsigned short"; break;
            case 4: recordType.typeName = "unsigned int"; break;
            case 8: recordType.typeName = "unsigned long long"; break;
            }
        }

        if (((recordType.baseType == 6) || (recordType.baseType == 13)) && (recordType.size != 4)) // "int"
        {
            switch (recordType.size)
            {
            case 1: recordType.typeName = "char"; break;
            case 2: recordType.typeName = "short"; break;
            case 4: recordType.typeName = "int"; break;
            case 8: recordType.typeName = "long long"; break;
            }
        }

        break;
    }
    case SymTagUDT:
    {
        UDT udt = GetUDT(symbol);

        recordType.size = udt.length;
        recordType.isTypeConst = udt.isConst;
        recordType.isTypeVolatile = udt.isVolatile;
        recordType.originalTypeName = udt.originalTypeName;
        recordType.typeName = udt.name;
        recordType.noType = false;

        IDiaSymbol* parentClass;
        BSTR bString;

		if (symbol->get_classParent(&parentClass) == S_OK)
		{
			if (parentClass->get_name(&bString) == S_OK)
			{
				udt.parentClassName = QString::fromWCharArray(bString);

				SysFreeString(bString);
			}

			parentClass->Release();
		}

        break;
    }
    case SymTagPointerType:
    {
        PointerType pointerType = GetPointerType(symbol);
        IDiaSymbol* type;

        if (symbol->get_type(&type) == S_OK)
        {
            recordType = GetType(type);

            /*
            * Don't check if data type is const or volatile in case of reference
            * because value of reference can't be changed so references are already treated as const
            */
            if (pointerType.isReference)
            {
                recordType.isReference = true;
                recordType.referenceLevel++;
            }
            else
            {
                recordType.isPointer = true;
                recordType.isPointerConst = pointerType.isConst;
                recordType.isPointerVolatile = pointerType.isVolatile;
                recordType.pointerLevel++;
            }

            recordType.size = pointerType.length;

            type->Release();
        }

        break;
    }
    case SymTagArrayType:
    {
        ArrayType arrayType = GetArrayType(symbol);
        IDiaSymbol* type;

        if (symbol->get_type(&type) == S_OK)
        {
            recordType = GetType(type);
            recordType.isArray = true;
            recordType.arrayCount.append(arrayType.count);
            recordType.size *= arrayType.count;

            recordType.isTypeConst = arrayType.isConst;
            recordType.isTypeVolatile = arrayType.isVolatile;

            type->Release();
        }

        break;
    }
    case SymTagEnum:
    {
        Enum enumType = GetEnum(symbol);

        recordType.size = enumType.length;
        recordType.isTypeConst = enumType.isConst;
        recordType.isTypeVolatile = enumType.isVolatile;
        recordType.originalTypeName = enumType.originalTypeName;
        recordType.typeName = enumType.name;
        recordType.noType = false;
        recordType.isTypeNameOfEnum = true;

        break;
    }
    case SymTagFunctionType:
    {
        RecordType returnType = GetRecordType(symbol);
        FunctionType functionType = GetFunctionType(symbol);

        returnType.callingConvention = functionType.callingConvention;

        if (returnType.size > 0 && returnType.functionType)
        {
            returnType.isFunctionPointer = true;
        }

        Data data = ConvertRecordTypeToData(const_cast<RecordType*>(&returnType));
        static const DataOptions dataOptions = { false, true };

        recordType.functionReturnType1 = data;
        recordType.functionReturnType2 = DataTypeToString(const_cast<Data*>(&data), &dataOptions);

        if (returnType.originalTypeName.length() > 0)
        {
            bool removeScopeOperator = options->removeScopeResolutionOperator;

            options->removeScopeResolutionOperator = false;
            data.typeName = returnType.originalTypeName;

            recordType.originalFunctionReturnType = DataTypeToString(const_cast<Data*>(&data), &dataOptions);

            options->removeScopeResolutionOperator = removeScopeOperator;
        }
        else
        {
            recordType.originalFunctionReturnType = recordType.functionReturnType2;
        }

        recordType.isFunctionConst = functionType.isConst;
        recordType.isFunctionVolatile = functionType.isVolatile;
        recordType.functionType = true;
        recordType.callingConvention = functionType.callingConvention;
        recordType.parametersCount = functionType.parametersCount;

        if (recordType.size > 0)
        {
            recordType.isFunctionPointer = true;
        }

        if (returnType.isFunctionPointer)
        {
            recordType.functionReturnsFunctionPointer = true;
        }

        IDiaEnumSymbols* enumSymbols;

        if (symbol->findChildren(SymTagNull, nullptr, nsNone, &enumSymbols) == S_OK)
        {
            LONG count;

            if (enumSymbols->get_Count(&count) == S_OK)
            {
                if (count)
                {
                    IDiaSymbol* symbol;
                    ULONG celt = 0;

                    while (SUCCEEDED(enumSymbols->Next(1, &symbol, &celt)) && (celt == 1))
                    {
                        RecordType recordType2 = GetRecordType(symbol);

                        symbol->Release();

                        if (recordType2.size > 0 && recordType2.functionType)
                        {
                            recordType2.isFunctionPointer = true;
                        }

                        if (recordType2.noType)
                        {
                            recordType.isVariadicFunction = true;
                        }

                        Data data2 = ConvertRecordTypeToData(const_cast<RecordType*>(&recordType2));
                        static const DataOptions dataOptions = { true, false };
                        QString parameter = DataTypeToString(const_cast<Data*>(&data2), &dataOptions);

                        recordType.functionParameters.append(parameter);

                        if (recordType2.originalTypeName.length() > 0)
                        {
                            bool removeScopeOperator = options->removeScopeResolutionOperator;

                            options->removeScopeResolutionOperator = false;
                            data2.typeName = recordType2.originalTypeName;

                            recordType.originalFunctionParameters.append(DataTypeToString(const_cast<Data*>(&data2), &dataOptions));

                            options->removeScopeResolutionOperator = removeScopeOperator;
                        }
                        else
                        {
                            recordType.originalFunctionParameters.append(parameter);
                        }
                    }
                }
            }

            enumSymbols->Release();
        }

        break;
    }
    default:
        emit SendStatusMessage("Unknown Type.");

        break;
    }

    return recordType;
}

QString PDB::DataTypeToString(const Data* data, const DataOptions* dataOptions)
{
    QString result = "";

    if (data->dataKind == DataIsStaticMember)
    {
        if (options->declareStaticVariablesWithInlineKeyword)
        {
            result += "inline ";
        }

        result += "static ";
    }

    if (data->isTypeConst && options->includeConstKeyword)
    {
        result += "const ";
    }

    if (data->isTypeVolatile && options->includeVolatileKeyword)
    {
        result += "volatile ";
    }

    if (!data->unnamedType)
    {
        QString typeName = data->typeName;

        if (options->displayIncludes && !data->isVTablePointer && !data->noType)
        {
            AddTypeNameToIncludesList(typeName, parentClassName);
        }

        if (options->removeScopeResolutionOperator && typeName.contains("::"))
        {
            if (data->parentClassName.length() == 0)
            {
                RemoveScopeResolutionOperators(typeName, parentClassName);
            }
            else
            {
                RemoveScopeResolutionOperators(typeName, data->parentClassName);
            }
        }

        result += typeName;
    }

    if (data->isFunctionPointer)
    {
        result += QString("%1 (").arg(data->functionReturnType);

        if (options->displayCallingConventionForFunctionPointers)
        {
            result += convertCallingConventionToString(data->callingConvention);
        }
    }

    if (data->isPointer)
    {
        for (int i = 0; i < data->pointerLevel; i++)
        {
            result += "*";
        }
    }

    if (data->isReference)
    {
        for (int i = 0; i < data->referenceLevel; i++)
        {
            result += "&";
        }
    }

    if (data->isPointerConst && options->includeConstKeyword)
    {
        result += " const";
    }

    if (data->isPointerVolatile && options->includeVolatileKeyword)
    {
        result += " volatile";
    }

    if (!data->isFunctionPointer &&
        !dataOptions->isParameter &&
        !dataOptions->isReturnType &&
        result.length() > 0)
    {
        result += " ";
    }

    QString name = data->name;

    if (options->modifyVariableNames)
    {
        name = ModifyNamingCovention(name, false, false, true);
    }

    if (data->isFunctionPointer)
    {
        result += QString("%1)").arg(name);
    }
    else
    {
        if (!dataOptions->isParameter && !dataOptions->isReturnType)
        {
            if (result.length() == 0 && name.length() > 0)
            {
                result += " ";
            }

            result += name;
        }
    }

    if (data->isArray)
    {
        int arrayCount = data->arrayCount.count();

        for (int i = arrayCount - 1; i >= 0; i--)
        {
            result += QString("[%1]").arg(QString::number(data->arrayCount.at(i)));
        }
    }
    else if (data->isFunctionPointer)
    {
        result += "(";

        int count = data->functionParameters.count();

        if (data->isVariadicFunction)
        {
            count--;
        }

        for (int i = 0; i < count; i++)
        {
            QString parameterType = data->functionParameters.at(i);
            bool isParameterFunctionPointer = false;

            if (!options->includeConstKeyword)
            {
                if (parameterType.contains("<") && parameterType.contains(" const"))
                {
                    parameterType.replace(" const", "");
                }

                if (parameterType.startsWith("const "))
                {
                    parameterType.remove(0, 6);
                }
            }

            if (parameterType.contains("(*)"))
            {
                isParameterFunctionPointer = true;
            }
            else
            {
                result += parameterType;
            }

            QString parameterName = GenerateCustomParameterName(parameterType, i);

            functionParameterNames.append(parameterName);

            if (isParameterFunctionPointer)
            {
                parameterType.insert(parameterType.indexOf("(*)") + 2, parameterName);

                result += parameterType;
            }
            else
            {
                result += QString(" %1").arg(parameterName);
            }

            if (i != count - 1)
            {
                result += ", ";
            }
        }

        if (data->isVariadicFunction)
        {
            if (count > 0)
            {
                result += ", ";
            }

            result += "...";
            //result += "va_list argumentsList";
        }

        result += ")";

        functionParameterNames.clear();
    }

    if (data->numberOfBits > 0)
    {
        result += QString(" : %1").arg(data->numberOfBits);
    }

    return result;
}

Element PDB::GetElement(SymbolRecord* symbolRecord, bool addToPrototypesList)
{
    Element element = {};
    Element* element2 = nullptr;
    IDiaSymbol* symbol = nullptr;
    bool isInCache = elements.contains(symbolRecord->id);

    if (isInCache)
    {
        element2 = &elements[symbolRecord->id];

        if (addToPrototypesList && element2->udt.hasBaseClass)
        {
            vTables.clear();
            vTableNames.clear();

            //Always call GetVTables if addToPrototypesList is true because sometimes element can be saved when addToPrototypesList is false
            GetVTables(element2, addToPrototypesList);
        }
    }
    else
    {
        GetSymbolByID(symbolRecord->id, &symbol);

        if (symbol)
        {
            GetElement(symbol);
            symbol->Release();

            element2 = &elements[symbolRecord->id];
        }
    }

    if (element2)
    {
        if (!isInCache)
        {
            isMainUDT = false;

            CheckIfDefaultCtorAndDtorAdded(element2);

            if (options->applyRuleOfThree)
            {
                CheckIfCopyCtorAndCopyAssignmentOpAdded(element2);
            }

            if (element2->udt.hasBaseClass)
            {
                vTables.clear();
                vTableNames.clear();

                GetVTables(element2, addToPrototypesList);
            }

            int udtChildrenCount = element2->udtChildren.count();

            for (int i = 0; i < udtChildrenCount; i++)
            {
                if (element2->udtChildren.at(i).udt.hasBaseClass)
                {
                    QHash<QString, QHash<QString, int>> vTables2;

                    if (vTables.count() > 0)
                    {
                        vTables2 = vTables;

                        vTables.clear();
                    }

                    GetVTables(&element2->udtChildren[i]);

                    if (vTables.count() > 0)
                    {
                        vTables = vTables2;
                    }
                }
            }

            if (element2->elementType == ElementType::udtType)
            {
                CheckIfUnionsAreMissing(element2);
            }

            if (options->declareFunctionsForStaticVariables)
            {
                DeclareFunctionsForStaticVariables(element2);
            }
        }

        if (!options->includeOnlyPublicAccessSpecifier)
        {
            JoinLists(element2);

            *element2 = OrderUDTChildrenByAccessSpecifiers(*element2);
        }
    }

    return *element2;
}

Element PDB::GetElement(IDiaSymbol* symbol)
{
    Element parentElement = {};
    bool children = true;
    qint64 currentOffset = 0;
    DWORD id = 0, symTag = 0;
    bool hasValidType = true;

    if (symbol->get_symIndexId(&id) != S_OK ||
        symbol->get_symTag(&symTag) != S_OK)
    {
        return parentElement;
    }

    switch (symTag)
    {
    case SymTagUDT:
    {
        parentElement.elementType = ElementType::udtType;
        parentElement.udt = GetUDT(symbol);
        parentElement.udt.id = id;
        parentElement.size = parentElement.udt.length;

        if (options->removeScopeResolutionOperator)
        {
            parentClassName2 = parentClassName;

            if (parentElement.udt.parentClassName.length() > 0)
            {
                parentClassName = parentElement.udt.parentClassName;
            }
            else
            {
                parentClassName = parentElement.udt.name;
            }
        }

        if (!isMainUDT)
        {
            parentElement.udt.isMainUDT = true;
            isMainUDT = true;
        }
        else if (belongsToMainUDT)
        {
            parentElement.udt.belongsToMainUDT = true;
            belongsToMainUDT = false;
        }

        break;
    }
    case SymTagFunction:
    {
        parentElement.elementType = ElementType::functionType;
        parentElement.function = GetFunction(symbol);
        parentElement.size = parentElement.function.length;

        break;
    }
    case SymTagTypedef:
    {
        parentElement.elementType = ElementType::typedefType;

        if (options->displayTypedefs)
        {
            parentElement.typeDef = GetTypeDef(symbol);
        }

        children = false;

        break;
    }
    case SymTagData:
    {
        parentElement.elementType = ElementType::dataType;
        parentElement.data = GetData(symbol);
        parentElement.size = parentElement.data.size;
        parentElement.offset = parentElement.data.offset;
        parentElement.bitOffset = parentElement.data.bitOffset;
        parentElement.numberOfBits = parentElement.data.length;

        if (parentElement.data.isPointer || parentElement.data.isReference)
        {
            children = false;
        }

        break;
    }
    case SymTagEnum:
    {
        parentElement.elementType = ElementType::enumType;
        parentElement.enum1 = GetEnum(symbol);
        parentElement.enum1.id = id;

        break;
    }
    case SymTagBaseClass:
    {
        //Members of base class aren't display so it's includes are not needed
        displayIncludes = options->displayIncludes;
        options->displayIncludes = false;

        parentElement.elementType = ElementType::baseClassType;
        parentElement.baseClass = GetBaseClass(symbol);
        parentElement.size = parentElement.baseClass.length;
        parentElement.offset = parentElement.baseClass.offset;

        break;
    }
    case SymTagVTable:
    {
        parentElement.elementType = ElementType::vTableType;

        children = false;

        break;
    }
    case SymTagFriend:
        children = false;

        break;
    default:
        children = false;
        hasValidType = false;

        break;
    }

    if (!isTypeImported && hasValidType && elements.contains(id))
    {
        return elements[id];
    }

    if (children)
    {
        IDiaEnumSymbols* enumSymbols;

        if (symbol->findChildren(SymTagNull, nullptr, nsNone, &enumSymbols) == S_OK)
        {
            LONG count;

            if (enumSymbols->get_Count(&count) == S_OK)
            {
                if (count)
                {
                    IDiaSymbol* symbol2;
                    ULONG celt = 0;

                    int alignCount = 0;
                    DWORD childSize = 0;

                    while (SUCCEEDED(enumSymbols->Next(1, &symbol2, &celt)) && (celt == 1))
                    {
                        if (symbol2->get_symTag(&symTag) == S_OK &&
                            symTag == SymTagUDT &&
                            parentElement.udt.isMainUDT)
                        {
                            belongsToMainUDT = true;
                        }

                        Element childElement = GetElement(symbol2);
                        bool add = true;

                        HandleChildElement(&parentElement, &childElement, add, currentOffset, childSize);

                        if (add)
                        {
                            bool addPadding = false;

                            if ((childElement.offset) && (childElement.offset > currentOffset))
                            {
                                addPadding = true;
                            }

                            if (parentElement.elementType == ElementType::functionType)
                            {
                                addPadding = false;
                            }

                            if (childElement.elementType == ElementType::dataType &&
                                childElement.data.dataKind == DataIsStaticMember)
                            {
                                addPadding = false;
                            }

                            if (addPadding)
                            {
                                AddPaddingToUDT(&parentElement, &childElement, currentOffset, alignCount);
                            }

                            InsertElement(&parentElement, &childElement);

                            if (childElement.size > 0 &&
                                (childElement.elementType == ElementType::dataType &&
                                    childElement.data.dataKind != DataIsStaticMember ||
                                    childElement.elementType == ElementType::baseClassType))
                            {
                                bool modifyOffset = true;

                                if (childElement.elementType == ElementType::baseClassType)
                                {
                                    if (childElement.dataChildren.count() == 1)
                                    {
                                        //Empty base class optimization (EBCO) sets size of empty base class to be 0
                                        if (childElement.dataChildren.at(0).data.isEndPadding)
                                        {
                                            modifyOffset = false;
                                        }
                                    }
                                }

                                if (modifyOffset)
                                {
                                    currentOffset = childElement.offset + childElement.size;
                                    childSize = qMax(childSize, childElement.offset + childElement.size);
                                }
                            }
                        }

                        symbol2->Release();
                    }

                    // If UDT or Base Class parent size is greater then child size add ending aligment
                    if (parentElement.elementType == ElementType::udtType ||
                        parentElement.elementType == ElementType::baseClassType)
                    {
                        long sizeDifference = parentElement.size - childSize;

                        if (sizeDifference > 0)
                        {
                            AddEndPaddingToUDT(&parentElement, childSize, sizeDifference);
                        }
                    }

                    if (parentElement.elementType == ElementType::udtType &&
                        !parentElement.udt.hasBaseClass &&
                        parentElement.udt.hasVTable)
                    {
                        parentElement.udt.numOfVTables++;
                    }

                    if (parentElement.elementType == ElementType::udtType &&
                        parentElement.udt.isMainUDT)
                    {
                        /*if (options->generateBoth)
                        {
                            includes.insert(QString("#include \"BaseAddresses.h\""));
                            includes.insert(QString("#include \"Function.h\""));
                        }*/

                        parentElement.udt.includes = includes;
                        parentElement.typedefChildren.append(typedefChildren);

                        includes.clear();
                        typedefChildren.clear();
                    }

                    /*if (!isTypeImported && hasValidType && !elements.contains(id))
                    {
                        elements.insert(id, element);
                    }*/
                }
            }

            if (!isTypeImported && hasValidType && !elements.contains(id))
            {
                elements.insert(id, parentElement);
            }

            enumSymbols->Release();
        }
    }

    return parentElement;
}

void PDB::InsertElement(Element* element, const Element* childElement)
{
    switch (childElement->elementType)
    {
    case ElementType::baseClassType:
        element->baseClassChildren.append(*childElement);

        break;
    case ElementType::enumType:
        element->enumChildren.append(*childElement);

        break;
    case ElementType::udtType:
        element->udtChildren.append(*childElement);

        break;
    case ElementType::typedefType:
        element->typedefChildren.append(*childElement);

        break;
    case ElementType::dataType:
    {
        if (childElement->data.dataKind == DataIsStaticMember)
        {
            element->staticDataChildren.append(*childElement);
        }
        else
        {
            element->dataChildren.append(*childElement);
        }

        break;
    }
    case ElementType::functionType:
    {
        if (childElement->function.isVirtual)
        {
            element->virtualFunctionChildren.append(*childElement);
        }
        else
        {
            element->nonVirtualFunctionChildren.append(*childElement);
        }

        break;
    }
    }
}

void PDB::JoinLists(Element* element)
{
    element->children.append(element->baseClassChildren);
    element->children.append(element->enumChildren);
    element->children.append(element->udtChildren);
    element->children.append(element->typedefChildren);
    element->children.append(element->dataChildren);
    element->children.append(element->staticDataChildren);
    element->children.append(element->virtualFunctionChildren);
    element->children.append(element->nonVirtualFunctionChildren);
}

void PDB::HandleChildElement(Element* parentElement, Element* childElement, bool& add, qint64& currentOffset, DWORD& childSize)
{
    switch (childElement->elementType)
    {
    case ElementType::baseClassType:
    {
        HandleBaseClassChild(parentElement, childElement);

        break;
    }
    case ElementType::udtType:
    {
        HandleUDTChild(parentElement, childElement, add);

        break;
    }
    case ElementType::enumType:
    {
        HandleEnumChild(childElement, add);

        break;
    }
    case ElementType::vTableType:
    {
        HandleVTableChild(parentElement, add, currentOffset, childSize);

        break;
    }
    case ElementType::typedefType:
    {
        HandleTypeDefChild(childElement, add);

        break;
    }
	case ElementType::dataType:
	{
		HandleDataChild(parentElement, childElement, add);

		break;
	}
	case ElementType::functionType:
	{
		HandleFunctionChild(parentElement, childElement, add);

		break;
	}
    case ElementType::funcDebugStartType:
    case ElementType::funcDebugEndType:
    case ElementType::blockType:
    case ElementType::callSiteType:
    case ElementType::labelType:
    case ElementType::unknownType:
        add = false;

        break;
    }
}

void PDB::HandleBaseClassChild(Element* parentElement, Element* childElement)
{
	if (parentElement->elementType == ElementType::udtType)
	{
		if (!parentElement->udt.hasBaseClass)
		{
            parentElement->udt.hasBaseClass = true;
		}
	}

	if (childElement->baseClass.hasVTable)
	{
		if (parentElement->elementType == ElementType::udtType)
		{
			if (childElement->baseClass.numOfVTables > 1)
			{
                parentElement->udt.numOfVTables += childElement->baseClass.numOfVTables;
			}
			else
			{
                parentElement->udt.numOfVTables++;
			}
		}
		else if (parentElement->elementType == ElementType::baseClassType)
		{
			if (childElement->baseClass.numOfVTables > 1)
			{
                parentElement->baseClass.numOfVTables += childElement->baseClass.numOfVTables;
			}
			else
			{
                parentElement->baseClass.numOfVTables++;
			}
		}
	}

	if (childElement->size > 1 &&
		GetChildrenSize(const_cast<Element*>(childElement)) > childElement->size)
	{
		FixOffsets(childElement);
	}

    if (parentElement->udt.isMainUDT || parentElement->udt.belongsToMainUDT)
	{
		//Don't set displayIncludes to true if it wasn't set to true by checkbox
		if (displayIncludes)
		{
			options->displayIncludes = true;
		}
	}

    parentElement->baseClassNames.append(childElement->baseClass.name);
    parentElement->baseClassNames.append(childElement->baseClassNames);
}

void PDB::HandleUDTChild(Element* parentElement, Element* childElement, bool& add)
{
	if (childElement->udt.name.startsWith("__cta"))
	{
		add = false;
	}

	if (childElement->udt.isUnnamed)
	{
		add = false;
	}

	//Sometimes inner udt that doesn't belong to main udt is added
	if (!CheckIfInnerUDTBelongsToMainUDT(parentElement, childElement))
	{
		add = false;
	}

	if (options->removeScopeResolutionOperator)
	{
		parentClassName = parentClassName2;
	}
}

void PDB::HandleEnumChild(Element* childElement, bool& add)
{
    if (childElement->enum1.isUnnamed)
    {
        add = false;
    }
}

void PDB::HandleVTableChild(Element* parentElement, bool& add, qint64& currentOffset, DWORD& childSize)
{
	add = false;

    AddVTablePointerToUDT(parentElement, currentOffset, childSize);
}

void PDB::HandleTypeDefChild(Element* childElement, bool& add)
{
	if (options->displayTypedefs)
	{
		QString oldTypeName = childElement->typeDef.oldTypeName;
		QString newTypeName = childElement->typeDef.newTypeName;

		if (options->useTypedefKeyword)
		{
			childElement->typeDef.declaration = QString("typedef %1 %2").arg(oldTypeName).arg(newTypeName);
		}
		else
		{
			childElement->typeDef.declaration = QString("using %1 = %2").arg(newTypeName).arg(oldTypeName);
		}
	}
	else
	{
		add = false;
	}
}

void PDB::HandleDataChild(Element* parentElement, Element* childElement, bool& add)
{
    if (childElement->data.dataKind == DataIsLocal ||
        childElement->data.dataKind == DataIsStaticLocal)
    {
        add = false;

        parentElement->localVariables.append(*childElement);
    }

    if (childElement->data.typeName.contains("<unnamed"))
    {
        CreateUnnamedType(childElement);
    }

    //Declaration is not needed for base class since it's members are not displayed
    if (add &&
        (parentElement->elementType == ElementType::udtType ||
            parentElement->elementType == ElementType::enumType))
    {
        static const DataOptions dataOptions;

        childElement->data.declaration = DataTypeToString(const_cast<Data*>(&childElement->data), &dataOptions);
    }
}

void PDB::HandleFunctionChild(Element* parentElement, Element* childElement, bool& add)
{
    if (childElement->function.name == "__vecDelDtor")
    {
        add = false;
    }

    if (childElement->function.isVirtual)
    {
        if (parentElement->elementType == ElementType::udtType && !parentElement->udt.hasVTable)
        {
            parentElement->udt.hasVTable = true;
        }
        else if (parentElement->elementType == ElementType::baseClassType && !parentElement->baseClass.hasVTable)
        {
            parentElement->baseClass.hasVTable = true;
        }
    }

    if (!childElement->function.returnType1.isPointer &&
        !childElement->function.returnType1.isReference)
    {
        IDiaSymbol* symbol2;
        DWORD id = diaSymbols->value(childElement->function.returnType1.originalTypeName);

        if (GetSymbolByID(id, &symbol2))
        {
            childElement->function.isRVOApplied = CheckIfRVOIsAppliedToFunction(symbol2);
        }
    }

    if (parentElement->elementType == ElementType::udtType)
    {
        if (parentElement->udt.hasCastOperator)
        {
            if (childElement->function.name.startsWith("operator"))
            {
                QString name = childElement->function.name.mid(9);

                if (baseTypes2.contains(name))
                {
                    name = baseTypes2[name];
                }

                if (name == childElement->function.returnType2)
                {
                    childElement->function.name = QString("operator %1").arg(name);
                    childElement->function.isCastOperator = true;
                }
            }
        }

        if (childElement->function.isDefaultConstructor)
        {
            parentElement->udt.hasDefaultConstructor = true;
        }

        if (childElement->function.isCopyConstructor)
        {
            parentElement->udt.hasCopyConstructor = true;
        }

        if (childElement->function.isCopyAssignmentOperator)
        {
            parentElement->udt.hasCopyAssignmentOperator = true;
        }

        if (childElement->function.isDestructor)
        {
            if (childElement->function.isVirtual)
            {
                parentElement->udt.hasVirtualDestructor = true;
            }
            else
            {
                parentElement->udt.hasDestructor = true;
            }
        }

        if (childElement->function.isVirtual &&
            childElement->function.isPure &&
            parentElement->elementType == ElementType::udtType &&
            !parentElement->udt.isAbstract)
        {
            parentElement->udt.isAbstract = true;
        }

        if (options->applyReturnValueOptimization && childElement->function.isRVOApplied)
        {
            ApplyReturnValueOptimization(childElement);
        }
    }

    /*
    * Prototypes for virtual functions shouldn't be created here because it's not known which
    * virtual function is overriden.
    * If udt doesn't have base class then prototypes can be created here because there are no
    * overriden functions in that case
    */
    if (add)
    {
        if (childElement->function.isVirtual)
        {
            if (parentElement->elementType == ElementType::udtType && !parentElement->udt.hasBaseClass)
            {
                static const FunctionOptions functionOptions;

                childElement->function.prototype = FunctionTypeToString(const_cast<Element*>(childElement),
                    &functionOptions);
            }
        }
        else
        {
            if (parentElement->elementType == ElementType::udtType)
            {
                static const FunctionOptions functionOptions;

                childElement->function.prototype = FunctionTypeToString(const_cast<Element*>(childElement),
                    &functionOptions);
            }
        }
    }
}

void PDB::AddVTablePointerToUDT(Element* element, qint64& currentOffset, DWORD& childSize)
{
	Element ptrElement = {};

	ptrElement.elementType = ElementType::dataType;
	ptrElement.data.name = "__vfptr";

	if (element->elementType == ElementType::udtType)
	{
		ptrElement.data.typeName = QString("%1Vtbl").arg(element->udt.name);

		if (element->udt.udtKind == UdtClass)
		{
			ptrElement.data.access = CV_private;
		}
		else
		{
			ptrElement.data.access = CV_public;
		}

		element->udt.hasVTablePointer = true;
	}
	else
	{
		ptrElement.data.typeName = QString("%1Vtbl").arg(element->baseClass.name);

		if (element->baseClass.udtKind == UdtClass)
		{
			ptrElement.data.access = CV_private;
		}
		else
		{
			ptrElement.data.access = CV_public;
		}

		element->baseClass.hasVTablePointer = true;
	}

	ptrElement.data.isPointer = true;
	ptrElement.data.pointerLevel = 1;
	ptrElement.data.dataKind = DataIsMember;
	ptrElement.data.isVTablePointer = true;
	ptrElement.data.isCompilerGenerated = true;
	ptrElement.offset = 0;

	if (type == CV_CFL_80386)
	{
		ptrElement.size = 4;
		currentOffset += 4;
		childSize += 4;
	}
	else
	{
		ptrElement.size = 8;
		currentOffset += 8;
		childSize += 8;
	}

	static const DataOptions dataOptions;

	ptrElement.data.declaration = DataTypeToString(const_cast<Data*>(&ptrElement.data), &dataOptions);

	if (element->dataChildren.count() > 0 && element->dataChildren.at(0).data.isPadding)
	{
		element->dataChildren.replace(0, ptrElement);
	}
	else
	{
        InsertElement(element, &ptrElement);
	}
}

void PDB::CreateUnnamedType(Element* childElement)
{
	childElement->data.unnamedType = true;

	QString typeName = childElement->data.originalTypeName;
	QHash<QString, DWORD>::const_iterator it = diaSymbols->find(typeName);

	DWORD id = it.value();
	IDiaSymbol* symbol;

	if (GetSymbolByID(id, &symbol))
	{
		Element childElement2 = GetElement(symbol);
		symbol->Release();

		childElement->baseClassChildren.append(childElement2.baseClassChildren);
		childElement->enumChildren.append(childElement2.enumChildren);
		childElement->udtChildren.append(childElement2.udtChildren);
		childElement->dataChildren.append(childElement2.dataChildren);
		childElement->staticDataChildren.append(childElement2.staticDataChildren);
		childElement->virtualFunctionChildren.append(childElement2.virtualFunctionChildren);
		childElement->nonVirtualFunctionChildren.append(childElement2.nonVirtualFunctionChildren);

		childElement->data.hasChildren = true;

		if (childElement2.elementType == ElementType::udtType)
		{
			childElement->udt = childElement2.udt;
		}
		else
		{
			childElement->enum1 = childElement2.enum1;
		}
	}
}

void PDB::AddPaddingToUDT(Element* parentElement, Element* childElement, qint64& currentOffset, int& alignCount)
{
	Element paddingElement = {};

	paddingElement.elementType = ElementType::dataType;
	paddingElement.offset = currentOffset;
	paddingElement.size = childElement->offset - currentOffset;
	paddingElement.data.isArray = true;
	paddingElement.data.isEndPadding = false;
	paddingElement.data.name = QString("__padding%1").arg(alignCount);
	paddingElement.data.arrayCount.append(paddingElement.size);
	paddingElement.data.typeName = "unsigned char";
	paddingElement.data.baseType = 5;
	paddingElement.data.isCompilerGenerated = true;
	paddingElement.data.isPadding = true;

	if (parentElement->elementType == ElementType::udtType)
	{
		if (parentElement->udt.udtKind == UdtClass)
		{
			paddingElement.data.access = CV_private;
		}
		else
		{
			paddingElement.data.access = CV_public;
		}
	}
	else
	{
		if (parentElement->baseClass.udtKind == UdtClass)
		{
			paddingElement.data.access = CV_private;
		}
		else
		{
			paddingElement.data.access = CV_public;
		}
	}

	static const DataOptions dataOptions;

	paddingElement.data.declaration = DataTypeToString(const_cast<Data*>(&paddingElement.data), &dataOptions);

    InsertElement(parentElement, &paddingElement);

	alignCount++;
}

void PDB::AddEndPaddingToUDT(Element* parentElement, DWORD& childSize, long& sizeDifference)
{
	Element paddingElement = {};

	paddingElement.elementType = ElementType::dataType;
	paddingElement.offset = childSize;
	paddingElement.size = sizeDifference;

	paddingElement.data.isArray = true;
	paddingElement.data.isPadding = true;
	paddingElement.data.isEndPadding = true;
	paddingElement.data.name = "__endPadding";
	paddingElement.data.arrayCount.append(paddingElement.size);
	paddingElement.data.typeName = "unsigned char";
	paddingElement.data.baseType = 5;
	paddingElement.data.isCompilerGenerated = true;

	if (parentElement->elementType == ElementType::udtType)
	{
		if (parentElement->udt.udtKind == UdtClass)
		{
			paddingElement.data.access = CV_private;
		}
		else
		{
			paddingElement.data.access = CV_public;
		}
	}
	else
	{
		if (parentElement->baseClass.udtKind == UdtClass)
		{
			paddingElement.data.access = CV_private;
		}
		else
		{
			paddingElement.data.access = CV_public;
		}
	}

	static const DataOptions dataOptions;

	paddingElement.data.declaration = DataTypeToString(const_cast<Data*>(&paddingElement.data), &dataOptions);

    InsertElement(parentElement, &paddingElement);
}

QString PDB::GetNameOfFirstVTable(const Element* element)
{
    int baseClassChildrenCount = element->baseClassChildren.count();
    int virtualFunctionChildrenCount = element->virtualFunctionChildren.count();

    for (int i = 0; i < baseClassChildrenCount; i++)
    {
        if (element->elementType == ElementType::baseClassType)
        {
            if (element->baseClass.numOfVTables > 1)
            {
                destructorAdded = false;

                vTableIndices.clear();
                virtualFunctionPrototypes.clear();
            }
        }

        QString vTableName = GetNameOfFirstVTable(&element->baseClassChildren[i]);

        if (i == 0 && vTableName.length() > 0)
        {
            return vTableName;
        }
    }

    for (int i = 0; i < virtualFunctionChildrenCount; i++)
    {
        //getVTableIndices is only for updating virtual base offsets of base classes
        if (element->elementType != ElementType::baseClassType ||
            element->elementType == ElementType::baseClassType &&
            (element->baseClass.numOfVTables > 1))
        {
            continue;
        }

        int vTableIndex = element->virtualFunctionChildren.at(i).function.virtualBaseOffset >> 2;
        static const FunctionOptions functionOptions = { false, false, false, false };
        QString functionPrototype = FunctionTypeToString(const_cast<Element*>(&element->virtualFunctionChildren.at(i)),
            &functionOptions);

        QHash<QString, int>::const_iterator it = vTableIndices.find(functionPrototype);

        if (it != vTableIndices.end())
        {
            return element->baseClass.name;
        }
        else
        {
            if (element->virtualFunctionChildren.at(i).function.isDestructor)
            {
                if (destructorAdded)
                {
                    QHash<QString, int>::const_iterator it2;

                    for (it2 = vTableIndices.begin(); it2 != vTableIndices.end(); it2++)
                    {
                        if (it2.key().at(0) == '~')
                        {
                            break;
                        }
                    }

                    continue;
                }

                vTableIndices.insert(functionPrototype, vTableIndex);
                destructorAdded = true;

                continue;
            }

            vTableIndices.insert(functionPrototype, vTableIndex);
        }
    }

    return "";
}

void PDB::GetVTables(Element* element, bool addToPrototypesList)
{
    if (element->elementType == ElementType::udtType)
    {
        QString firstVTableName = GetNameOfFirstVTable(const_cast<Element*>(element));

        if (firstVTableName.length() == 0)
        {
            firstVTableName = element->udt.name;
        }

        element->udt.vTableNames.insert(0, firstVTableName);

        vTableIndices.clear();
        virtualFunctionPrototypes.clear();

        destructorAdded = false;

        vTableNames.insert(0, firstVTableName);
    }

    int baseClassChildrenCount = element->baseClassChildren.count();
    int virtualFunctionChildrenCount = element->virtualFunctionChildren.count();

    for (int i = 0; i < baseClassChildrenCount; i++)
    {
        if (element->elementType == ElementType::baseClassType &&
            element->baseClass.numOfVTables > 1 &&
            i == 0)
        {
            element->baseClass.vTableNames.insert(0, vTableNames[0]);
        }

        GetVTables(&element->baseClassChildren[i], addToPrototypesList);

        if (element->elementType == ElementType::baseClassType)
        {
            element->baseClass.vTableNames.insert(element->baseClassChildren.at(i).baseClass.vTableNames);
        }

        if (i == 0)
        {
            QHash<QString, int>::const_iterator it;
            QString functionPrototype;
            int vTableIndex = -1;

            for (it = vTables[vTableNames[0]].begin(); it != vTables[vTableNames[0]].end(); it++)
            {
                if (it.key().at(0) == '~')
                {
                    vTables[vTableNames[0]].remove(it.key());

                    break;
                }
            }

            vTables[vTableNames[0]].insert(vTableIndices);

            if (addToPrototypesList)
            {
                virtualFunctionPrototypes2[vTableNames[0]].insert(virtualFunctionPrototypes);
            }
        }
        else
        {
            if (element->baseClassChildren.at(i).baseClass.hasVTable &&
                vTableNames2.find(element->baseClassChildren.at(i).baseClass.name) == vTableNames2.end())
            {
                vTables.insert(element->baseClassChildren.at(i).baseClass.name, vTableIndices);
                vTableNames.insert(vTableNames.count(), element->baseClassChildren.at(i).baseClass.name);

                if (element->elementType == ElementType::baseClassType)
                {
                    if (element->baseClass.numOfVTables > 1)
                    {
                        int index = element->baseClass.vTableNames.count();

                        element->baseClass.vTableNames.insert(index, element->baseClassChildren.at(i).baseClass.name);
                    }
                    else
                    {
                        element->baseClass.vTableNames.insert(element->baseClass.vTableNames.count(),
                            element->baseClassChildren.at(i).baseClass.name);
                    }
                }

                if (addToPrototypesList)
                {
                    virtualFunctionPrototypes2.insert(element->baseClassChildren.at(i).baseClass.name, virtualFunctionPrototypes);
                }
            }
        }

        if (element->elementType == ElementType::baseClassType)
        {
            if (element->baseClass.numOfVTables > 1)
            {
                destructorAdded = false;

                vTableIndices.clear();
                virtualFunctionPrototypes.clear();
            }
        }
        else if (element->elementType == ElementType::udtType)
        {
            destructorAdded = false;

            vTableIndices.clear();
            virtualFunctionPrototypes.clear();
        }
    }

    if (element->elementType == ElementType::baseClassType &&
        element->baseClass.numOfVTables > 1)
    {
        UpdateVTables(element, &element->baseClass.vTableNames, addToPrototypesList);
    }

    for (int i = 0; i < virtualFunctionChildrenCount; i++)
    {
        //getVTableIndices is only for updating virtual base offsets of base classes
        if (element->elementType != ElementType::baseClassType ||
            element->elementType == ElementType::baseClassType &&
            (element->baseClass.numOfVTables > 1))
        {
            continue;
        }

        int vTableIndex = element->virtualFunctionChildren.at(i).function.virtualBaseOffset >> 2;
        static const FunctionOptions functionOptions = { false, false, false, false };
        QString functionPrototype = FunctionTypeToString(const_cast<Element*>(&element->virtualFunctionChildren.at(i)), &functionOptions);

        QString functionName;
        QString value;

        if (addToPrototypesList)
        {
            functionName = element->virtualFunctionChildren.at(i).function.name;
            value = QString("%1::%2").arg(element->baseClass.name).arg(functionName);
        }

        QHash<QString, int>::const_iterator it = vTableIndices.find(functionPrototype);

        if (it != vTableIndices.end())
        {
            /*
            * There are two ways to handle case with same pure virtual function in both parent and child class
            * (that case happens when pure virtual function from base class is not implemented in child class).
            * First way is to assign isOverridden to true only if virtual function is not pure.
            * Second way is to assign for example isPureVirtualFunctionFromBaseClass to true instead of assigning isOverridden
            * to true and then just don't display virtual function if isPureVirtualFunctionFromBaseClass is true
            */

            element->virtualFunctionChildren[i].function.virtualBaseOffset = it.value() << 2;

            if (!element->virtualFunctionChildren[i].function.isPure)
            {
                element->virtualFunctionChildren[i].function.isOverridden = true;
            }
        }
        else
        {
            if (element->virtualFunctionChildren.at(i).function.isDestructor)
            {
                if (destructorAdded)
                {
                    element->virtualFunctionChildren[i].function.isOverridden = true;

                    QHash<QString, int>::const_iterator it2;

                    for (it2 = vTableIndices.begin(); it2 != vTableIndices.end(); it2++)
                    {
                        if (it2.key().at(0) == '~')
                        {
                            element->virtualFunctionChildren[i].function.virtualBaseOffset = it2.value() << 2;

                            vTableIndex = it2.value();
                            vTableIndices.remove(it2.key());

                            break;
                        }
                    }

                    vTableIndices.insert(functionPrototype, vTableIndex);

                    if (addToPrototypesList)
                    {
                        QHash<QString, QString>::const_iterator it2;

                        for (it2 = virtualFunctionPrototypes.begin(); it2 != virtualFunctionPrototypes.end(); it2++)
                        {
                            if (it2.key().at(0) == '~')
                            {
                                virtualFunctionPrototypes.remove(it2.key());

                                break;
                            }
                        }

                        virtualFunctionPrototypes.insert(functionPrototype, value);
                    }

                    continue;
                }

                vTableIndices.insert(functionPrototype, vTableIndex);
                destructorAdded = true;

                if (addToPrototypesList)
                {
                    virtualFunctionPrototypes.insert(functionPrototype, value);
                }

                continue;
            }
            else
            {
                //Check for covariant return type
                if (element->virtualFunctionChildren.at(i).function.returnType1.typeName == element->baseClass.name)
                {
                    Element element2 = element->virtualFunctionChildren.at(i);
                    Data data = element->virtualFunctionChildren.at(i).function.returnType1;
                    int baseClassNamesCount = element->baseClassNames.count();
                    static const DataOptions dataOptions = { false, true };
                    QString returnType, functionPrototype2;
                    bool found = false;

                    for (int j = 0; j < baseClassNamesCount; j++)
                    {
						bool removeScopeOperator = options->removeScopeResolutionOperator;

						options->removeScopeResolutionOperator = false;
                        data.typeName = element->baseClassNames.at(j);

                        returnType = DataTypeToString(&data, &dataOptions);

                        options->removeScopeResolutionOperator = removeScopeOperator;
                        element2.function.originalReturnType = returnType;

                        functionPrototype2 = FunctionTypeToString(const_cast<Element*>(&element2), &functionOptions);

                        it = vTableIndices.find(functionPrototype2);

                        if (it != vTableIndices.end())
                        {
                            if (!element->virtualFunctionChildren[i].function.isPure)
                            {
                                element->virtualFunctionChildren[i].function.isOverridden = true;
                            }

                            element->virtualFunctionChildren[i].function.virtualBaseOffset = it.value() << 2;

                            vTableIndex = it.value();
                            vTableIndices.remove(it.key());

                            vTableIndices.insert(functionPrototype, vTableIndex);

                            if (addToPrototypesList)
                            {
                                QHash<QString, QString>::const_iterator it2;

                                for (it2 = virtualFunctionPrototypes.begin(); it2 != virtualFunctionPrototypes.end(); it2++)
                                {
                                    if (it2.key() == functionPrototype2)
                                    {
                                        virtualFunctionPrototypes.remove(it2.key());

                                        break;
                                    }
                                }

                                virtualFunctionPrototypes.insert(functionPrototype, value);
                            }

                            found = true;

                            break;
                        }
                    }

                    if (found)
                    {
                        continue;
                    }
                }
            }

            vTableIndices.insert(functionPrototype, vTableIndex);
        }

        if (addToPrototypesList)
        {
            virtualFunctionPrototypes.insert(functionPrototype, value);
        }
    }

    if (element->elementType == ElementType::udtType)
    {
        element->udt.vTableNames.insert(vTableNames);
        UpdateVTables(element, &element->udt.vTableNames, addToPrototypesList);

        QList<Element> virtualFunctionChildren;

        for (int i = 0; i < element->udt.numOfVTables; i++)
        {
            QMap<int, Element> virtualFunctions;

            for (int j = 0; j < virtualFunctionChildrenCount; j++)
            {
                static const FunctionOptions functionOptions;
                QString prototype = FunctionTypeToString(const_cast<Element*>(&element->virtualFunctionChildren.at(j)),
                    &functionOptions);

                element->virtualFunctionChildren[j].function.prototype = prototype;

                if (element->virtualFunctionChildren.at(j).function.indexOfVTable == i)
                {
                    int offset = element->virtualFunctionChildren.at(j).function.virtualBaseOffset;

                    virtualFunctions.insert(offset, element->virtualFunctionChildren.at(j));
                }
            }

            virtualFunctionChildren.append(virtualFunctions.values());
        }

        if (virtualFunctionChildren.count() > 0)
        {
            element->virtualFunctionChildren = virtualFunctionChildren;
        }
        else
        {
            static const FunctionOptions functionOptions;
            QString prototype;

            for (int i = 0; i < virtualFunctionChildrenCount; i++)
            {
                prototype = FunctionTypeToString(const_cast<Element*>(&element->virtualFunctionChildren.at(i)), &functionOptions);

                element->virtualFunctionChildren[i].function.prototype = prototype;
            }
        }
    }
}

QHash<QString, int> PDB::GetVTable(QString vTableName)
{
    QHash<QString, int> vTable;
    QHash<QString, QHash<QString, int>>::const_iterator it;

    for (it = vTables.begin(); it != vTables.end(); it++)
    {
        if (it.key() == vTableName)
        {
            vTable = it.value();

            break;
        }
    }

    return vTable;
}

void PDB::UpdateVTable(Element* element, int indexOfVTable, QString vTableName, QMap<int, QString>* vTableNames,
    QHash<QString, int>* vTableIndices, bool addToPrototypesList,
    QHash<QString, QString>* virtualFunctionPrototypes)
{
    int virtualFunctionChildrenCount = element->virtualFunctionChildren.count();

    for (int i = 0; i < virtualFunctionChildrenCount; i++)
    {
        int vTableIndex = element->virtualFunctionChildren.at(i).function.virtualBaseOffset >> 2;
        static const FunctionOptions functionOptions = { false, false, false, false };
        QString functionPrototype = FunctionTypeToString(const_cast<Element*>(&element->virtualFunctionChildren.at(i)),
            &functionOptions);

        QString functionName;
        QString value;

        if (addToPrototypesList)
        {
            functionName = element->virtualFunctionChildren.at(i).function.name;

            if (element->elementType == ElementType::udtType)
            {
                value = QString("%1::%2").arg(element->udt.name).arg(functionName);
            }
            else
            {
                value = QString("%1::%2").arg(element->baseClass.name).arg(functionName);
            }
        }

        if (element->virtualFunctionChildren.at(i).function.isDestructor)
        {
            QHash<QString, int>::const_iterator it2;

            for (it2 = vTableIndices->begin(); it2 != vTableIndices->end(); it2++)
            {
                if (it2.key().at(0) == '~')
                {
                    element->virtualFunctionChildren[i].function.isOverridden = true;
                    element->virtualFunctionChildren[i].function.virtualBaseOffset = it2.value() << 2;

                    vTableIndex = it2.value();
                    vTableIndices->remove(it2.key());

                    break;
                }
            }

            vTableIndices->insert(functionPrototype, vTableIndex);

            if (addToPrototypesList)
            {
                QHash<QString, QString>::const_iterator it3;

                for (it3 = virtualFunctionPrototypes->begin(); it3 != virtualFunctionPrototypes->end(); it3++)
                {
                    if (it3.key().at(0) == '~')
                    {
                        virtualFunctionPrototypes->remove(it3.key());

                        break;
                    }
                }

                virtualFunctionPrototypes->insert(functionPrototype, value);
            }
        }
        else
        {
            QHash<QString, int>::const_iterator it = vTableIndices->find(functionPrototype);

            if (it != vTableIndices->end())
            {
                element->virtualFunctionChildren[i].function.virtualBaseOffset = it.value() << 2;

                if (!element->virtualFunctionChildren[i].function.isPure)
                {
                    element->virtualFunctionChildren[i].function.isOverridden = true;
                }

                /*
                * Update indexOfVTable also in case of main table because value of indexOfVTable of funtion can change if function
                * is declared in multiple vtables
                */

                QMap<int, QString>::const_iterator it2;

                for (it2 = vTableNames->begin(); it2 != vTableNames->end(); it2++)
                {
                    if (vTableName == it2.value())
                    {
                        element->virtualFunctionChildren[i].function.indexOfVTable = it2.key();
                        element->virtualFunctionChildren[i].function.vTableName = it2.value();

                        break;
                    }
                }

                if (addToPrototypesList)
                {
                    virtualFunctionPrototypes->insert(functionPrototype, value);
                }
            }
            else
            {
                if (indexOfVTable == 0 && element->virtualFunctionChildren.at(i).function.indexOfVTable == 0)
                {
                    QString name;

                    if (element->elementType == ElementType::udtType)
                    {
                        name = element->udt.name;
                    }
                    else
                    {
                        name = element->baseClass.name;
                    }

                    //Check for covariant return type
                    if (element->virtualFunctionChildren.at(i).function.returnType1.typeName == name)
                    {
                        Element element2 = element->virtualFunctionChildren.at(i);
                        Data data = element->virtualFunctionChildren.at(i).function.returnType1;
                        int baseClassNamesCount = element->baseClassNames.count();
                        static const DataOptions dataOptions = { false, true };
                        QString returnType, functionPrototype2;
                        bool found = false;

                        for (int j = 0; j < baseClassNamesCount; j++)
                        {
							bool removeScopeOperator = options->removeScopeResolutionOperator;

							options->removeScopeResolutionOperator = false;
                            data.typeName = element->baseClassNames.at(j);

                            returnType = DataTypeToString(&data, &dataOptions);

                            options->removeScopeResolutionOperator = removeScopeOperator;
                            element2.function.originalReturnType = returnType;

                            functionPrototype2 = FunctionTypeToString(const_cast<Element*>(&element2), &functionOptions);

                            it = vTableIndices->find(functionPrototype2);

                            if (it != vTableIndices->end())
                            {
                                if (!element->virtualFunctionChildren[i].function.isPure)
                                {
                                    element->virtualFunctionChildren[i].function.isOverridden = true;
                                }

                                element->virtualFunctionChildren[i].function.virtualBaseOffset = it.value() << 2;

                                vTableIndex = it.value();
                                vTableIndices->remove(it.key());

                                vTableIndices->insert(functionPrototype, vTableIndex);

                                if (addToPrototypesList)
                                {
                                    QHash<QString, QString>::const_iterator it2;

                                    for (it2 = virtualFunctionPrototypes->begin(); it2 != virtualFunctionPrototypes->end(); it2++)
                                    {
                                        if (it2.key() == functionPrototype2)
                                        {
                                            virtualFunctionPrototypes->remove(it2.key());

                                            break;
                                        }
                                    }

                                    virtualFunctionPrototypes->insert(functionPrototype, value);
                                }

                                found = true;

                                break;
                            }
                        }

                        if (found)
                        {
                            continue;
                        }
                    }

                    vTableIndices->insert(functionPrototype, vTableIndex);

                    QMap<int, QString>::const_iterator it2;

                    for (it2 = vTableNames->begin(); it2 != vTableNames->end(); it2++)
                    {
                        if (vTableName == it2.value())
                        {
                            element->virtualFunctionChildren[i].function.indexOfVTable = it2.key();
                            element->virtualFunctionChildren[i].function.vTableName = it2.value();

                            break;
                        }
                    }

                    if (addToPrototypesList)
                    {
                        virtualFunctionPrototypes->insert(functionPrototype, value);
                    }
                }
            }
        }
    }

    vTables[vTableName] = *vTableIndices;
    virtualFunctionPrototypes2[vTableName] = *virtualFunctionPrototypes;
}

void PDB::UpdateVTables(Element* element, QMap<int, QString>* vTableNames, bool addToPrototypesList,
    QHash<QString, QString>* virtualFunctionPrototypes)
{
    /*
    * If vTables are looped using QHash then they wouldn't be always visited in same order so if multiple base classes
    * with exactly same virtual functions are inherited it can happen that sometimes function will be declared as part of
    * third vTable for example and sometimes as part of first vTable. Because of that it's better to iterate QMap instead
    * in ascending or descending order
    */

    auto it = vTableNames->end(), end = vTableNames->begin();

    while (it != end)
    {
        --it;

        int indexOfVTable = it.key();
        QString vTableName = it.value();

        QHash<QString, QHash<QString, QString>>::const_iterator it2;
        QHash<QString, QString> virtualFunctionPrototypes;

        for (it2 = virtualFunctionPrototypes2.begin(); it2 != virtualFunctionPrototypes2.end(); it2++)
        {
            if (it.value() == it2.key())
            {
                virtualFunctionPrototypes = it2.value();

                break;
            }
        }

        QHash<QString, int> vTableIndices = vTables.value(it.value());

        UpdateVTable(element, indexOfVTable, vTableName, vTableNames, &vTableIndices, addToPrototypesList,
            &virtualFunctionPrototypes);
    }
}

QHash<QString, QString> PDB::getFunctionPrototypes(QString vTableName)
{
    QHash<QString, QString> virtualFunctionPrototypes;
    QHash<QString, QHash<QString, QString>>::const_iterator it;

    for (it = virtualFunctionPrototypes2.begin(); it != virtualFunctionPrototypes2.end(); it++)
    {
        if (it.key() == vTableName)
        {
            virtualFunctionPrototypes = it.value();

            break;
        }
    }

    return virtualFunctionPrototypes;
}

Element PDB::OrderUDTElementChildren(Element element)
{
    Element newElement = element;
    newElement.children.clear();

    int count = element.children.count();
    int lastVariableIndex = -1;

    /*
    * Order:
    * 1. Base Classes
    * 2. Enums
    * 3. Inner Types
    * 4. Typedefs
    * 5. Variables
    * 6. Static Variables
    * 7. Virtual Functions
    * 8. Non Virtual Functions
    */

    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < count; j++)
        {
            switch (i)
            {
            case 0:
            {
                if (element.children.at(j).elementType == ElementType::baseClassType)
                {
                    Element newElement2 = OrderUDTElementChildren(element.children.at(j));
                    newElement.children.append(newElement2);
                }

                break;
            }
            case 1:
            {
                if (element.children.at(j).elementType == ElementType::enumType)
                {
                    newElement.children.append(element.children.at(j));
                }

                break;
            }
            case 2:
            {
                if (element.children.at(j).elementType == ElementType::udtType)
                {
                    if (element.children.at(j).udt.hasBaseClass)
                    {
                        QHash<QString, QHash<QString, int>> vTables2;

                        if (vTables.count() > 0)
                        {
                            vTables2 = vTables;

                            vTables.clear();
                        }

                        GetVTables(&element.children[j]);

                        if (vTables.count() > 0)
                        {
                            vTables = vTables2;
                        }
                    }

                    Element newElement2 = OrderUDTElementChildren(element.children.at(j));
                    newElement.children.append(newElement2);
                }

                break;
            }
            case 3:
            {
                if (element.children.at(j).elementType == ElementType::typedefType)
                {
                    newElement.children.append(element.children.at(j));
                }

                break;
            }
            case 4:
            {
                if (element.children.at(j).elementType == ElementType::dataType &&
                    element.children.at(j).data.dataKind != DataIsStaticMember)
                {
                    if (element.children.at(j).data.hasChildren)
                    {
                        if (element.children.at(j).data.isTypeNameOfEnum)
                        {
                            newElement.children.append(element.children.at(j));
                        }
                        else
                        {
                            element.children[j].elementType = ElementType::udtType;

                            Element newElement2 = OrderUDTElementChildren(element.children.at(j));

                            newElement2.elementType = ElementType::dataType;
                            newElement.children.append(newElement2);
                        }
                    }
                    else
                    {
                        newElement.children.append(element.children.at(j));

                        if (element.udt.numOfVTables > 1)
                        {
                            lastVariableIndex = newElement.children.count() - 1;
                        }
                    }
                }

                break;
            }
            case 5:
            {
                if (element.children.at(j).elementType == ElementType::dataType &&
                    element.children.at(j).data.dataKind == DataIsStaticMember)
                {
                    if (element.children.at(j).data.hasChildren)
                    {
                        if (element.children.at(j).data.isTypeNameOfEnum)
                        {
                            newElement.children.append(element.children.at(j));
                        }
                        else
                        {
                            element.children[j].elementType = ElementType::udtType;

                            Element newElement2 = OrderUDTElementChildren(element.children.at(j));

                            newElement2.elementType = ElementType::dataType;
                            newElement.children.append(newElement2);
                        }
                    }
                    else
                    {
                        newElement.children.append(element.children.at(j));

                        if (element.udt.numOfVTables > 1)
                        {
                            lastVariableIndex = newElement.children.count() - 1;
                        }
                    }
                }

                break;
            }
            case 6:
            {
                if (element.udt.numOfVTables == 1 &&
                    element.children.at(j).elementType == ElementType::functionType &&
                    element.children.at(j).function.isVirtual)
                {
                    int offset = element.children.at(j).function.virtualBaseOffset;

                    virtualFunctions.insert(offset, element.children.at(j));
                }

                break;
            }
            case 7:
            {
                if (element.children.at(j).elementType == ElementType::functionType &&
                    !element.children.at(j).function.isVirtual)
                {
                    newElement.children.append(element.children.at(j));
                }

                break;
            }
            }
        }

        if (element.udt.numOfVTables == 1 && i == 5)
        {
            newElement.children.append(virtualFunctions.values());
            virtualFunctions.clear();
        }
    }

    if (element.udt.numOfVTables > 1)
    {
        int k = lastVariableIndex + 1;

        for (int i = 0; i < element.udt.numOfVTables; i++)
        {
            for (int j = 0; j < count; j++)
            {
                if (element.children.at(j).elementType == ElementType::functionType &&
                    element.children.at(j).function.isVirtual &&
                    element.children.at(j).function.indexOfVTable == i)
                {
                    int offset = element.children.at(j).function.virtualBaseOffset;

                    virtualFunctions.insert(offset, element.children.at(j));
                }
            }

            if (lastVariableIndex == -1)
            {
                newElement.children.append(virtualFunctions.values());
            }
            else
            {
                QMultiMap<int, Element>::const_iterator it;

                for (it = virtualFunctions.begin(); it != virtualFunctions.end(); it++)
                {
                    newElement.children.insert(k++, it.value());
                }
            }

            virtualFunctions.clear();
        }
    }

    return newElement;
}

Element PDB::OrderUDTChildrenByAccessSpecifiers(Element element)
{
    Element newElement = element;
    newElement.children.clear();

    int count = element.children.count();

    /*
    * Order:
    * 1. Base Classes
    * 2. Enums
    * 3. Inner Types
    * 4. Typedefs
    * 5. Variables
    * 6. Static Variables
    * 7. Virtual Functions
    * 8. Non Virtual Functions
    */

    /*
    * Order members by access specfiers in this order: public, protected, private
    * Public should be first because inner udts and enums are public
    */

    for (int i = 3; i >= 1; i--)
    {
        for (int j = 0; j < 8; j++)
        {
            for (int k = 0; k < count; k++)
            {
                switch (j)
                {
                case 0:
                {
                    if (element.children.at(k).elementType == ElementType::baseClassType &&
                        element.children.at(k).baseClass.access == (CV_access_e)i)
                    {
                        JoinLists(&element.children[k]);

                        Element newElement2 = OrderUDTElementChildren(element.children.at(k));
                        newElement.children.append(newElement2);
                    }

                    break;
                }
                case 1:
                {
                    if (element.children.at(k).elementType == ElementType::enumType &&
                        element.children.at(k).enum1.access == (CV_access_e)i)
                    {
                        JoinLists(&element.children[k]);

                        newElement.children.append(element.children.at(k));
                    }

                    break;
                }
                case 2:
                {
                    if (element.children.at(k).elementType == ElementType::udtType &&
                        element.children.at(k).udt.access == (CV_access_e)i)
                    {
                        JoinLists(&element.children[k]);

                        Element newElement2 = OrderUDTChildrenByAccessSpecifiers(element.children.at(k));
                        newElement.children.append(newElement2);
                    }

                    break;
                }
                case 3:
                {
                    if (element.children.at(k).elementType == ElementType::typedefType &&
                        element.children.at(k).udt.access == (CV_access_e)i)
                    {
                        newElement.children.append(element.children.at(k));
                    }

                    break;
                }
                case 4:
                {
                    if (element.children.at(k).elementType == ElementType::dataType &&
                        element.children.at(k).data.dataKind != DataIsStaticMember &&
                        element.children.at(k).data.access == (CV_access_e)i)
                    {
                        if (element.children.at(k).data.hasChildren)
                        {
                            JoinLists(&element.children[k]);

                            if (element.children.at(k).data.isTypeNameOfEnum)
                            {
                                newElement.children.append(element.children.at(k));
                            }
                            else
                            {
                                element.children[k].elementType = ElementType::udtType;

                                Element newElement2 = OrderUDTChildrenByAccessSpecifiers(element.children.at(k));

                                newElement2.elementType = ElementType::dataType;
                                newElement.children.append(newElement2);
                            }
                        }
                        else
                        {
                            newElement.children.append(element.children.at(k));
                        }
                    }

                    break;
                }
                case 5:
                {
                    if (element.children.at(k).elementType == ElementType::dataType &&
                        element.children.at(k).data.dataKind == DataIsStaticMember &&
                        element.children.at(k).data.access == (CV_access_e)i)
                    {
                        if (element.children.at(k).data.hasChildren)
                        {
                            JoinLists(&element.children[k]);

                            if (element.children.at(k).data.isTypeNameOfEnum)
                            {
                                newElement.children.append(element.children.at(k));
                            }
                            else
                            {
                                element.children[k].elementType = ElementType::udtType;

                                Element newElement2 = OrderUDTChildrenByAccessSpecifiers(element.children.at(k));

                                newElement2.elementType = ElementType::dataType;
                                newElement.children.append(newElement2);
                            }
                        }
                        else
                        {
                            newElement.children.append(element.children.at(k));
                        }
                    }

                    break;
                }
                case 6:
                {
                    if (element.children.at(k).elementType == ElementType::functionType &&
                        element.children.at(k).function.access == (CV_access_e)i)
                    {
                        if (element.children.at(k).function.isVirtual)
                        {
                            newElement.children.append(element.children.at(k));
                        }
                    }

                    break;
                }
                case 7:
                {
                    if (element.children.at(k).elementType == ElementType::functionType &&
                        element.children.at(k).function.access == (CV_access_e)i)
                    {
                        if (!element.children.at(k).function.isVirtual)
                        {
                            newElement.children.append(element.children.at(k));
                        }
                    }

                    break;
                }
                }
            }
        }
    }

    return newElement;
}

bool PDB::GetSymbolByID(DWORD id, IDiaSymbol** symbol)
{
    if (diaSession->symbolById(id, symbol) == S_OK)
    {
        return true;
    }

    return false;
}

bool PDB::GetSymbolByTypeName(enum SymTagEnum symTag, QString typeName, IDiaSymbol** symbol)
{
    IDiaEnumSymbols* enumSymbols;

    if (global->findChildren(symTag, typeName.toStdWString().c_str(), nsNone, &enumSymbols) == S_OK)
    {
        if (enumSymbols->Item(0, symbol) != S_OK)
        {
            return false;
        }

        enumSymbols->Release();

        return true;
    }

    return false;
}

quint32 PDB::GetChildrenSize(const Element* element)
{
    quint32 size = 0;
    int count = element->baseClassChildren.count() + element->dataChildren.count();
    int bitSum = 0;
    QList<Element> children;

    children.append(element->baseClassChildren);
    children.append(element->dataChildren);

    for (int i = 0; i < count; i++)
    {
        if (children.at(i).elementType == ElementType::baseClassType)
        {
            size += children.at(i).baseClass.length;
        }
        else if (children.at(i).elementType == ElementType::dataType)
        {
            if (children.at(i).numberOfBits > 0)
            {
                unsigned long numberOfBits = children.at(i).size << 3;
                unsigned long bitsSize = 0;
                int countOfNewVariables = 1;

                while (i < count &&
                    children.at(i).elementType == ElementType::dataType &&
                    children.at(i).numberOfBits != 0)
                {
                    bitsSize += children.at(i).numberOfBits;

                    if (i < count - 1 &&
                        children.at(i + 1).elementType == ElementType::dataType &&
                        children.at(i + 1).numberOfBits + bitsSize > numberOfBits)
                    {
                        countOfNewVariables++;
                        bitsSize = 0;
                    }

                    i++;
                }

                i--; //i++ in while loop and i++ in main for loop will skip one member
                size += countOfNewVariables * children.at(i).size;
            }
            else
            {
                size += children.at(i).size;
            }
        }
    }

    return size;
}

bool PDB::CheckIfChildrenSizesAreCorrect(const Element* element, QString& message)
{
    int dataChildrenCount = element->dataChildren.count();

    for (int i = 0; i < dataChildrenCount; i++)
    {
        if (element->dataChildren.at(i).size == 0)
        {
            message = QString("<font size = 5>Output is not correct because size of %1 is 0 and that causes padding to be created!!!</font>")
                .arg(element->dataChildren.at(i).data.name);

            return false;
        }
    }

    return true;
}

void PDB::FixOffsets(Element* element)
{
    Element newElement = *element;

    newElement.dataChildren.clear();

    int count = element->dataChildren.count();

    AppendElement(&newElement, &element->dataChildren, 0, count);

    *element = newElement;
}

void PDB::AppendElement(Element* element, QList<Element>* children, int startPosition, int endPosition)
{
    for (int i = startPosition; i < endPosition; i++)
    {
        if (children->at(i).size)
        {
            quint32 offset = children->at(i).offset;
            quint32 bitOffset = children->at(i).bitOffset;
            quint32 size = children->at(i).size;
            quint32 size2 = size;
            quint32 count = 1;
            quint32 position = i;
            quint32 startOffset = offset;

            QList<quint32> sizes;
            QList<quint32> counts;
            QList<quint32> positions;

            for (int j = i + 1; j < endPosition; j++)
            {
                const Element& element2 = children->at(j);
                quint32 elementRange = 0;

                if (!ShouldIncludeElement(element2))
                {
                    elementRange = element2.offset + element2.size;
                }

                if ((element2.offset == offset) &&
                    (element2.bitOffset == bitOffset) &&
                    (element2.size) && !ShouldIncludeElement(element2))
                {
                    sizes.append(size2);
                    counts.append(count);
                    positions.append(position);

                    size2 = 0;
                    count = 0;
                    position = j;
                    startOffset = element2.offset;
                }

                const quint32 prevRange = startOffset + size2;

                if (elementRange <= prevRange)
                {
                    size2 += 0;
                }
                else
                {
                    size2 += elementRange - prevRange; //element2.size;
                }

                count++;
            }

            int count2 = sizes.count();

            if (count2)
            {
                // The last position
                quint32 maxSize = 0;

                for (int j = 0; j < count2; j++)
                {
                    maxSize = qMax(maxSize, sizes.at(j));
                }

                size2 = 0;
                count = 0;
                quint32 totalrange = offset;

                for (int j = position; j < endPosition; j++)
                {
                    const Element& element2 = children->at(j);
                    quint32 elementRange = 0;

                    if (!ShouldIncludeElement(element2))
                    {
                        elementRange = element2.offset + element2.size;
                    }

                    if ((size2 >= maxSize) && (elementRange > totalrange))
                    {
                        break;
                    }

                    if (ShouldIncludeElement(element2))
                    {
                        break;
                    }

                    if (element2.elementType == ElementType::dataType &&
                        element2.data.isPadding)
                    {
                        break;
                    }

                    /*if (element2.data.type.isEndAlign)
                    {
                        break;
                    }*/

                    if (elementRange <= totalrange)
                    {
                        size2 += 0;
                    }
                    else
                    {
                        size2 += elementRange - totalrange; // element2.size;
                        totalrange = elementRange;
                    }

                    count++;
                }

                sizes.append(size2);
                counts.append(count);
                positions.append(position);

                count2++;

                bool newUnion = true;

                if ((element->udt.udtKind == UdtUnion) && (element->size == size2))
                {
                    newUnion = false;
                }

                Element unionElem = {};
                Element* unionElement;

                if (newUnion)
                {
                    unionElement = &unionElem;
                    unionElement->elementType = ElementType::dataType;
                    unionElement->size = qMax(size2, maxSize);
                    unionElement->udt.type = "union";
                    unionElement->udt.isAnonymousUnion = true;
                    unionElement->udt.udtKind = UdtUnion;
                    unionElement->data.hasChildren = true;

                    if (element->elementType == ElementType::udtType)
                    {
                        if (element->udt.udtKind == UdtClass)
                        {
                            unionElement->data.access = CV_private;
                        }
                        else
                        {
                            unionElement->data.access = CV_public;
                        }
                    }
                    else
                    {
                        if (element->baseClass.udtKind == UdtClass)
                        {
                            unionElement->data.access = CV_private;
                        }
                        else
                        {
                            unionElement->data.access = CV_public;
                        }
                    }
                }
                else
                {
                    unionElement = element;
                }

                for (int j = 0; j < count2; j++)
                {
                    if (counts.at(j) > 1)
                    {
                        Element structElem = {};
                        Element* structElement = &structElem;
                        structElement->elementType = ElementType::dataType;
                        structElement->size = sizes.at(j);
                        structElement->udt.type = "struct";
                        structElement->udt.isAnonymousStruct = true;
                        structElement->data.hasChildren = true;

                        if (element->elementType == ElementType::udtType)
                        {
                            if (element->udt.udtKind == UdtClass)
                            {
                                structElement->data.access = CV_private;
                            }
                            else
                            {
                                structElement->data.access = CV_public;
                            }
                        }
                        else
                        {
                            if (element->baseClass.udtKind == UdtClass)
                            {
                                structElement->data.access = CV_private;
                            }
                            else
                            {
                                structElement->data.access = CV_public;
                            }
                        }

                        AppendElement(structElement, children, positions.at(j), positions.at(j) + counts.at(j));

                        unionElement->dataChildren.append(*structElement);
                    }
                    else
                    {
                        AppendElement(unionElement, children, positions.at(j), positions.at(j) + counts.at(j));
                    }

                    i += counts.at(j);
                }

                i--;

                if (newUnion)
                {
                    element->dataChildren.append(*unionElement);
                }
            }
            else
            {
                element->dataChildren.append(children->at(i));
            }
        }
        else
        {
            element->dataChildren.append(children->at(i));
        }
    }
}

bool PDB::ShouldIncludeElement(Element element)
{
    return element.elementType == ElementType::functionType ||
        element.elementType == ElementType::baseClassType ||
        element.elementType == ElementType::udtType ||
        element.elementType == ElementType::enumType ||
        (element.elementType == ElementType::dataType &&
            (element.data.dataKind == DataIsStaticMember));
}

void PDB::CheckIfUnionsAreMissing(Element* element)
{
    if (!element->udt.isAnonymousUnion &&
        !element->udt.isAnonymousStruct &&
        element->size > 1 &&
        GetChildrenSize(const_cast<Element*>(element)) > element->size)
    {
        FixOffsets(element);
    }

    int udtChildrenCount = element->udtChildren.count();
    int dataChildrenCount = element->dataChildren.count();
    int staticDataChildrenCount = element->staticDataChildren.count();

    for (int i = 0; i < udtChildrenCount; i++)
    {
        CheckIfUnionsAreMissing(&element->udtChildren[i]);
    }

    for (int i = 0; i < dataChildrenCount; i++)
    {
        if (element->dataChildren.at(i).data.hasChildren && !element->dataChildren.at(i).data.isTypeNameOfEnum)
        {
            CheckIfUnionsAreMissing(&element->dataChildren[i]);
        }
    }

    for (int i = 0; i < staticDataChildrenCount; i++)
    {
        if (element->staticDataChildren.at(i).data.hasChildren && !element->dataChildren.at(i).data.isTypeNameOfEnum)
        {
            CheckIfUnionsAreMissing(&element->staticDataChildren[i]);
        }
    }
}

QString PDB::convertAccessSpecifierToString(CV_access_e accessSpecifier)
{
    QString result;

    switch (accessSpecifier)
    {
    case CV_private:
        result = "private"; 

        break;
    case CV_protected:
        result = "protected"; 

        break;
    case CV_public:
        result = "public"; 

        break;
    }

    return result;
}

QString PDB::convertCallingConventionToString(CV_call_e callingConvention)
{
    QString result;

    switch (callingConvention)
    {
    case CV_CALL_THISCALL:
		result = "__thiscall";

		break;
    case CV_CALL_NEAR_FAST:
    case CV_CALL_FAR_FAST:
        result = "__fastcall";

        break;
    case CV_CALL_NEAR_STD:
    case CV_CALL_FAR_STD:
        result = "__stdcall";

        break;
    case CV_CALL_NEAR_C:
    case CV_CALL_FAR_C:
        result = "__cdecl";

        break;
    case CV_CALL_NEAR_VECTOR:
        result = "__vectorcall";

        break;
    }

    return result;
}

QString PDB::convertDataKindToString(DataKind dataKind)
{
    QString result;

    switch (dataKind)
    {
    case DataIsUnknown:
        result = "Unknown";

        break;
    case DataIsLocal:
		result = "Local";

		break;
    case DataIsStaticLocal:
		result = "Static local";

		break;
    case DataIsParam:
		result = "Parameter";

		break;
    case DataIsObjectPtr:
		result = "Object pointer";

		break;
    case DataIsFileStatic:
		result = "File static";

		break;
    case DataIsGlobal:
		result = "Global";

		break;
    case DataIsMember:
		result = "Member";

		break;
    case DataIsStaticMember:
		result = "Static member";

		break;
    case DataIsConstant:
		result = "Constant";

		break;
    }

    return result;
}

QString PDB::convertLocationTypeToString(LocationType locationType)
{
    QString type;

    switch (locationType)
    {
    case LocIsNull:
        type = "Null";

        break;
    case LocIsStatic:
        type = "Static";

        break;
    case LocIsTLS:
        type = "Thread local storage";

        break;
    case LocIsRegRel:
        type = "Registry relative";

        break;
    case LocIsThisRel:
        type = "This relative";

        break;
    case LocIsEnregistered:
        type = "Enregistered";

        break;
    case LocIsBitField:
        type = "Bit field";

        break;
    case LocIsSlot:
        type = "Microsoft Intermediate Language (MSIL) slot";

        break;
    case LocIsIlRel:
        type = "MSIL relative";

        break;
    case LocInMetaData:
        type = "Metadata";

        break;
    case LocIsConstant:
        type = "Constant";

        break;
    }

    return type;
}

QString PDB::convertVariantToString(VARIANT variant)
{
    QString result;

    switch (variant.vt)
    {
    case VT_UI1:
    case VT_I1:
        result = QString(" 0x%1").arg(variant.bVal);

        break;
    case VT_I2:
    case VT_UI2:
    case VT_BOOL:
        result = QString(" 0x%1").arg(variant.iVal);

        break;
    case VT_I4:
    case VT_UI4:
    case VT_INT:
    case VT_UINT:
    case VT_ERROR:
        result = QString(" 0x%1").arg(variant.lVal);

        break;
    case VT_R4:
        result = QString(" %g").arg(variant.fltVal);

        break;
    case VT_R8:
        result = QString(" %g").arg(variant.dblVal);

        break;
    case VT_BSTR:
        result = QString(" \"%1\"").arg(variant.bstrVal);

        break;
    default:
        result = QString(" ??");

        break;
    }

    return result;
}

QString PDB::convertLanguageToString(CV_CFL_LANG language)
{
    QString result;

    switch (language)
    {
    case CV_CFL_C:
        result = "C";

        break;
    case CV_CFL_CXX:
        result = "C++";

        break;
    case CV_CFL_FORTRAN:
        result = "Fortran";

        break;
    case CV_CFL_MASM:
        result = "MASM";

        break;
    case CV_CFL_PASCAL:
        result = "Pascal";

        break;
    case CV_CFL_BASIC:
        result = "Basic";

        break;
    case CV_CFL_COBOL:
        result = "Cobol";

        break;
    case CV_CFL_LINK:
        result = "Link";

        break;
    case CV_CFL_CVTRES:
        result = "CvtRes";

        break;
    case CV_CFL_CVTPGD:
        result = "CvtPdg";

        break;
    case CV_CFL_CSHARP:
        result = "C#";

        break;
    case CV_CFL_VB:
        result = "Visual Basic";

        break;
    case CV_CFL_ILASM:
        result = "ILASM";

        break;
    case CV_CFL_JAVA:
        result = "Java";

        break;
    case CV_CFL_JSCRIPT:
        result = "Javascript";

        break;
    case CV_CFL_MSIL:
        result = "MSIL";

        break;
    case CV_CFL_HLSL:
        result = "HLSL";

        break;
    case CV_CFL_OBJC:
        result = "Objective C";

        break;
    case CV_CFL_OBJCXX:
        result = "Objective C++";

        break;
    }

    return result;
}

QString PDB::convertPlatformToString(CV_CPU_TYPE_e cpuType)
{
    QString result;

    switch (cpuType)
    {
    case 0:
        result = "8080";

        break;
    case 1:
        result = "8086";

        break;
    case 2:
        result = "80286";

        break;
    case 3:
        result = "80386";

        break;
    case 4:
        result = "80486";

        break;
    case 5:
        result = "Pentium";

        break;
    case 6:
        result = "Pentium II/Pentium Pro";

        break;
    case 7:
        result = "Pentium III";

        break;
    case 16:
        result = "MIPS (Generic)";

        break;
    case 17:
        result = "MIPS16";

        break;
    case 18:
        result = "MIPS32";

        break;
    case 19:
        result = "MIPS64";

        break;
    case 20:
        result = "MIPS I";

        break;
    case 21:
        result = "MIPS II";

        break;
    case 22:
        result = "MIPS III";

        break;
    case 23:
        result = "MIPS IV";

        break;
	case 24:
		result = "MIPS V";

		break;
    case 32:
        result = "M68000";

        break;
	case 33:
		result = "M68010";

		break;
    case 34:
        result = "M68020";

        break;
    case 35:
        result = "M68030";

        break;
    case 36:
        result = "M68040";

        break;
    case 48:
        result = "Alpha 21064";

        break;
    case 49:
        result = "Alpha 21164";

        break;
    case 50:
        result = "Alpha 21164A";

        break;
    case 51:
        result = "Alpha 21264";

        break;
    case 52:
        result = "Alpha 21364";

        break;
    case 64:
        result = "PPC 601";

        break;
    case 65:
        result = "PPC 603";

        break;
    case 66:
        result = "PPC 604";

        break;
    case 67:
        result = "PPC 620";

        break;
    case 68:
        result = "PPC w/FP";

        break;
    case 69:
        result = "PPC (Big Endian)";

        break;
    case 80:
        result = "SH3";

        break;
    case 81:
        result = "SH3E";

        break;
    case 82:
        result = "SH3DSP";

        break;
    case 83:
        result = "SH4";

        break;
    case 84:
        result = "SHmedia";

        break;
    case 96:
        result = "ARM3";

        break;
    case 97:
        result = "ARM4";

        break;
    case 98:
        result = "ARM4T";

        break;
    case 99:
        result = "ARM5";

        break;
    case 100:
        result = "ARM5T";

        break;
    case 101:
        result = "ARM6";

        break;
    case 102:
        result = "ARM (XMAC)";

        break;
    case 103:
        result = "ARM (WMMX)";

        break;
    case 104:
        result = "ARM 7";

        break;
    case 112:
        result = "Omni";

        break;
    case 128:
        result = "Itanium";

        break;
    case 129:
        result = "Itanium (McKinley)";

        break;
    case 144:
        result = "CEE";

        break;
    case 160:
        result = "AM33";

        break;
    case 176:
        result = "M32R";

        break;
    case 192:
        result = "TriCore";

        break;
    case 208:
        result = "x64";

        break;
    case 224:
        result = "EBC";

        break;
    case 240:
        result = "Thumb (CE)";

        break;
    case 244:
        result = "ARM NT";

        break;
	case 246:
		result = "ARM 64";

		break;
    case 256:
        result = "D3D11_SHADER";

        break;
    }

    return result;
}

QString PDB::GetTab(int level)
{
    QString result = "";

    for (int i = 0; i < level; i++)
    {
        result += "\t";
    }

    return result;
}

QString PDB::GetBaseClassesInfo(const Element* element, bool addKeywords)
{
    QString elementInfo;
    int childrenCount = element->baseClassChildren.count();

    for (int i = 0; i < childrenCount; i++)
    {
        if (addKeywords)
        {
            if (i == 0)
            {
                elementInfo += " : ";
            }

            QString accessSpecifier;
            bool accessSpecifierAdded = false;

            if (options->includeOnlyPublicAccessSpecifier)
            {
                if (element->elementType == ElementType::udtType && element->udt.udtKind == UdtClass)
                {
                    accessSpecifier += "public";
                    accessSpecifierAdded = true;
                }
                else if (element->elementType == ElementType::baseClassType && element->baseClass.udtKind == UdtClass)
                {
                    accessSpecifier += "public";
                    accessSpecifierAdded = true;
                }
            }
            else
            {
                accessSpecifier = convertAccessSpecifierToString(element->baseClassChildren.at(i).baseClass.access);
                accessSpecifierAdded = true;
            }

            if (accessSpecifierAdded)
            {
                elementInfo += QString("%1 ").arg(accessSpecifier);
            }

            if (element->baseClassChildren.at(i).baseClass.isVirtualBaseClass ||
                element->baseClassChildren.at(i).baseClass.isIndirectVirtualBaseClass)
            {
                elementInfo += "virtual ";
            }
        }

        QString baseClassName = element->baseClassChildren.at(i).baseClass.name;

        if (options->removeScopeResolutionOperator && baseClassName.contains("::"))
        {
            if (element->baseClassChildren.at(i).baseClass.parentClassName.length() == 0)
            {
                RemoveScopeResolutionOperators(baseClassName, parentClassName);
            }
            else
            {
                RemoveScopeResolutionOperators(baseClassName, element->baseClassChildren.at(i).baseClass.parentClassName);
            }
        }

        elementInfo += baseClassName;

        if (i != childrenCount - 1)
        {
            elementInfo += ", ";
        }
    }

    return elementInfo;
}

QString PDB::GetEnumInfo(const Element* element, int level)
{
    QString elementInfo = "";

    if (element->enum1.isAnonymous || element->enum1.isUnnamed)
    {
        elementInfo += GetTab(level) + "enum\r\n";
    }
    else
    {
        QString enumName = element->enum1.name;

        if (options->removeScopeResolutionOperator && enumName.contains("::"))
        {
            if (element->enum1.parentClassName.length() == 0)
            {
                RemoveScopeResolutionOperators(enumName, parentClassName);
            }
            else
            {
                RemoveScopeResolutionOperators(enumName, element->enum1.parentClassName);
            }
        }

        elementInfo += GetTab(level) + QString("enum %1\r\n").arg(enumName);
    }

    elementInfo += GetTab(level) + "{\r\n";

    int count = element->dataChildren.count();

    for (int i = 0; i < count; i++)
    {
        elementInfo += GetTab(level + 1) + QString("%1 = %2").arg(element->dataChildren.at(i).data.name)
            .arg(element->dataChildren.at(i).data.value.value.toString());

        if (i != count - 1)
        {
            elementInfo += ",";
        }

        elementInfo += "\r\n";
    }

    elementInfo += GetTab(level) + "}";

    if (element->enum1.isUnnamed)
    {
        elementInfo += GetDataInfo(&element->data, level);
    }

    return elementInfo;
}

QString PDB::GetUDTInfo(Element* element, int level)
{
    QString text;
    quint32 alignment = 0;
    quint32 correctAlignment = 0;
    bool privateSpecifierAdded = false;
    bool protectedSpecifierAdded = false;
    bool publicSpecifierAdded = false;

    /*
    * When option to add all access specifiers is used elements are already added to main children list
    * so there JoinLists shoudn't be called again
    */
    if (options->includeOnlyPublicAccessSpecifier)
    {
        JoinLists(element);
    }

    if (element->udt.name.startsWith("m128") ||
        element->udt.name.startsWith("_m128") ||
        element->udt.name.startsWith("__m128"))
	{
        //__m128 is declared with 16 byte alignment __declspec(align(16))
        alignment = 16;
	}
    else
    {
        unsigned int greatestPadding = GetGreatestPaddingInUDT(element);

        if (greatestPadding > 1)
        {
            /*
			* This method of calculating alignment is used first because it's faster and because there can be case
			* where alignment is higher then default
            * If padding is for example 8 that means that alignment is higher then 8 bytes
            * so + 1 makes sure that returned number is greater then 8.
            */
            alignment = NextPowerOf2(greatestPadding + 1);
        }
        else
        {
            QHash<QString, int> checkedTypes;
            QString parentClassName2 = parentClassName;

            alignment = CalculateDefaultAlignment(element, checkedTypes);

            parentClassName = parentClassName2;
        }
    }

    correctAlignment = GetCorrectAlignment(alignment, element->size);

    element->udt.defaultAlignment = alignment;
    element->udt.correctAlignment = correctAlignment;

    if (correctAlignment < alignment)
    {
        if (level > 0)
        {
            text += GetTab(level) + QString("#pragma pack(push, %1)\r\n\r\n").arg(correctAlignment);
        }
    }

    if (options->specifyTypeAlignment)
    {
        if (correctAlignment < alignment)
        {
            if (element->udt.isUnnamed)
            {
                text += GetTab(level) + QString("%1").arg(element->udt.type);

                /*
                * Add __declspec(novtable) to save space
                * https://docs.microsoft.com/en-us/archive/msdn-magazine/2000/march/c-q-a-atl-virtual-functions-and-vtables
                */
                if (element->udt.isAbstract && options->addNoVTableKeyword)
                {
                    text += "__declspec(novtable) ";
                }

                if (options->applyEmptyBaseClassOptimization && CheckIfHasAnyEmptyBaseClass(element))
				{
                    text += "__declspec(empty_bases) ";
				}
            }
            else
            {
                QString name = element->udt.name;

                if (options->removeScopeResolutionOperator && name.contains("::"))
                {
                    if (element->udt.parentClassName.length() == 0)
                    {
                        RemoveScopeResolutionOperators(name, parentClassName);
                    }
                    else
                    {
                        RemoveScopeResolutionOperators(name, element->udt.parentClassName);
                    }
                }

                if (options->removeHungaryNotationFromUDTAndEnums)
                {
                    name = ModifyNamingCovention(name, false, false, false);
                }

                text += GetTab(level) + QString("%1 ").arg(element->udt.type);

                if (element->udt.isAbstract && options->addNoVTableKeyword)
                {
                    text += "__declspec(novtable) ";
                }

				if (options->applyEmptyBaseClassOptimization && CheckIfHasAnyEmptyBaseClass(element))
				{
					text += "__declspec(empty_bases) ";
				}

                text += QString("%1").arg(name);
            }
        }
        else
        {
            if (element->udt.isUnnamed)
            {
                text += GetTab(level) + QString("%1 ").arg(element->udt.type);

                if (element->udt.isAbstract && options->addNoVTableKeyword)
                {
                    text += "__declspec(novtable) ";
                }

				if (options->applyEmptyBaseClassOptimization && CheckIfHasAnyEmptyBaseClass(element))
				{
					text += "__declspec(empty_bases) ";
				}

                text += QString("alignas(%1)").arg(QString::number(alignment));
            }
            else
            {
                QString name = element->udt.name;

                if (options->removeScopeResolutionOperator && name.contains("::"))
                {
                    if (element->udt.parentClassName.length() == 0)
                    {
                        RemoveScopeResolutionOperators(name, parentClassName);
                    }
                    else
                    {
                        RemoveScopeResolutionOperators(name, element->udt.parentClassName);
                    }
                }

                if (options->removeHungaryNotationFromUDTAndEnums)
                {
                    name = ModifyNamingCovention(name, false, false, false);
                }

                text += GetTab(level) + QString("%1 ").arg(element->udt.type);

                if (element->udt.isAbstract && options->addNoVTableKeyword)
                {
                    text += "__declspec(novtable) ";
                }

				if (options->applyEmptyBaseClassOptimization && CheckIfHasAnyEmptyBaseClass(element))
				{
					text += "__declspec(empty_bases) ";
				}

                text += QString("alignas(%1) %2").arg(QString::number(alignment)).arg(name);
            }
        }
    }
    else
    {
        if (element->udt.isUnnamed)
        {
            text += GetTab(level) + QString("%1").arg(element->udt.type);

            if (element->udt.isAbstract && options->addNoVTableKeyword)
            {
                text += " __declspec(novtable) ";
            }

			if (options->applyEmptyBaseClassOptimization && CheckIfHasAnyEmptyBaseClass(element))
			{
				text += "__declspec(empty_bases) ";
			}
        }
        else
        {
            QString name = element->udt.name;

            if (options->removeScopeResolutionOperator && name.contains("::"))
            {
                if (element->udt.parentClassName.length() == 0)
                {
                    RemoveScopeResolutionOperators(name, parentClassName);
                }
                else
                {
                    RemoveScopeResolutionOperators(name, element->udt.parentClassName);
                }
            }

            if (options->removeHungaryNotationFromUDTAndEnums)
            {
                name = ModifyNamingCovention(name, false, false, false);
            }

            text += GetTab(level) + QString("%1 ").arg(element->udt.type);

            if (element->udt.isAbstract && options->addNoVTableKeyword)
            {
                text += "__declspec(novtable) ";
            }

			if (options->applyEmptyBaseClassOptimization && CheckIfHasAnyEmptyBaseClass(element))
			{
				text += "__declspec(empty_bases) ";
			}

            text += QString("%1").arg(name);
        }
    }

    QString baseClassesInfo = GetBaseClassesInfo(element);

    text += baseClassesInfo;

    if (options->displayComments)
    {
        text += QString(" //Size = 0x%1").arg(element->size, 0, 16);
    }

    text += "\r\n";
    text += GetTab(level) + "{\r\n";

    if (element->udt.udtKind == UdtClass &&
        options->includeOnlyPublicAccessSpecifier &&
        CheckIfPublicKeywordShouldBeAdded(element))
    {
        text += GetTab(level) + "public:\r\n";
    }

    int childrenCount = element->children.count();

    for (int i = 0; i < childrenCount; i++)
    {
        if (element->children.at(i).elementType != ElementType::baseClassType)
        {
            if (element->children.at(i).elementType == ElementType::functionType &&
                element->children.at(i).function.isNonImplemented &&
                !options->displayNonImplementedFunctions)
            {
                AddAdditionalNewLine(element, text, i);

                continue;
            }

            if ((element->children.at(i).elementType == ElementType::dataType &&
                element->children.at(i).data.isCompilerGenerated) &&
                (!options->displayVTablePointerIfExists &&
                    element->children.at(i).data.isVTablePointer ||
                    !element->children.at(i).data.isVTablePointer &&
                    !options->displayPaddingBytes))
            {
                AddAdditionalNewLine(element, text, i);

                continue;
            }

            if (!options->includeOnlyPublicAccessSpecifier)
            {
                CV_access_e access = CV_private;
                ElementType elementType = element->children.at(i).elementType;
                bool isAnonymousUDT = element->udt.isAnonymousUnion || element->udt.isAnonymousStruct;

                if (elementType == ElementType::udtType)
                {
                    access = element->children.at(i).udt.access;
                }
                else if (elementType == ElementType::dataType)
                {
                    access = element->children.at(i).data.access;
                }
                else if (elementType == ElementType::functionType)
                {
                    access = element->children.at(i).function.access;
                }
                else if (elementType == ElementType::enumType)
                {
                    access = element->children.at(i).enum1.access;
                }

                switch (access)
                {
                case CV_private:
                    if (!privateSpecifierAdded && !isAnonymousUDT)
                    {
                        privateSpecifierAdded = true;

                        if (protectedSpecifierAdded || publicSpecifierAdded)
                        {
                            text += "\r\n";
                        }

                        text += GetTab(level) + "private:\r\n";
                    }

                    break;
                case CV_protected:
                    if (!protectedSpecifierAdded && !isAnonymousUDT)
                    {
                        protectedSpecifierAdded = true;

                        if (privateSpecifierAdded || publicSpecifierAdded)
                        {
                            text += "\r\n";
                        }

                        text += GetTab(level) + "protected:\r\n";
                    }

                    break;
                case CV_public:
                    if (!publicSpecifierAdded && !isAnonymousUDT)
                    {
                        publicSpecifierAdded = true;

                        if (privateSpecifierAdded || protectedSpecifierAdded)
                        {
                            text += "\r\n";
                        }

                        if (element->udt.udtKind == UdtStruct || element->udt.udtKind == UdtUnion)
                        {
                            /*
                            * Only add public access specifier to struct or union if there is already some other
                            * access specifier because by default struct and union members are public
                            */
                            if (privateSpecifierAdded || protectedSpecifierAdded)
                            {
                                text += GetTab(level) + "public:\r\n";
                            }
                        }
                        else
                        {
                            text += GetTab(level) + "public:\r\n";
                        }
                    }

                    break;
                }
            }

            text += GetElementInfo(&element->children[i], level + 1);

            if (!(element->children.at(i).elementType == ElementType::functionType &&
                element->children.at(i).function.isVariadic &&
                options->useTemplateFunction))
            {
                text += ";";
            }

            AddComments(element, text, i);

            if (element->children[i].elementType == ElementType::udtType &&
                element->children[i].udt.correctAlignment < element->children[i].udt.defaultAlignment)
            {
                text += QString("\r\n\r\n%1#pragma pack(pop)").arg(GetTab(level + 1));
            }

            AddAdditionalNewLine(element, text, i);

            text += "\r\n";
        }
    }

    text += GetTab(level) + "}";

    if (element->elementType == ElementType::dataType &&
        element->data.hasChildren &&
        element->data.name.length() > 0)
    {
        text += GetDataInfo(&element->data, level);
    }

    return text;
}

QString PDB::GetElementInfo(Element* element, int level)
{
    QString text;
    ElementType elementType = element->elementType;
    bool isParentClass = false, isParentNamespace = false;

    if (level == 0)
    {
        QString typeName;
        bool isNested = false;

        if (element->elementType == ElementType::udtType)
        {
            typeName = element->udt.name;
            isNested = element->udt.isNested;
        }
        else
        {
            typeName = element->enum1.name;
            isNested = element->enum1.isNested;
        }

        if (typeName.contains("::"))
        {
            QString parentClassName = GetParentClassName(typeName);

            if (parentClassName.length() > 0)
            {
                auto it = diaSymbols->find(parentClassName);

                if (it == diaSymbols->end())
                {
                    if (isNested)
                    {
                        text += QString("class %1\r\n{\r\npublic:\r\n").arg(parentClassName);

                        isParentClass = true;
                    }
                    else
                    {
                        text += QString("namespace %1\r\n{\r\n").arg(parentClassName);

                        isParentNamespace = true;
                    }
                }
            }
        }
    }

    if (element->elementType == ElementType::dataType &&
        element->data.hasChildren)
    {
        if (element->data.isTypeNameOfEnum)
        {
            elementType = ElementType::enumType;
        }
        else
        {
            elementType = ElementType::udtType;
        }
    }

    switch (elementType)
    {
    case ElementType::enumType:
    {
        if (level == 0 && (isParentClass || isParentNamespace))
        {
            text += GetEnumInfo(const_cast<Element*>(element), level + 1);
        }
        else
        {
            text += GetEnumInfo(const_cast<Element*>(element), level);
        }

        break;
    }
    case ElementType::udtType:
    {
		/*if (options->declareFunctionsForStaticVariables)
		{
			declareFunctionsForStaticVariables(element);
		}*/

        if (level == 0 && (isParentClass || isParentNamespace))
        {
            text += GetUDTInfo(element, level + 1);
        }
        else
        {
            text += GetUDTInfo(element, level);
        }

        break;
    }
    case ElementType::dataType:
    {
        text += GetDataInfo(&element->data, level);

        break;
    }
    case ElementType::functionType:
    {
        text += GetFunctionInfo(const_cast<Element*>(element), level);

        break;
    }
    case ElementType::typedefType:
    {
        text += GetTab(level) + element->typeDef.declaration;

        break;
    }
    }

    if (level == 0)
    {
        text += ";\r\n";

        if (isParentClass)
        {
            text += "};\r\n";
        }
        else if (isParentNamespace)
        {
            text += "}\r\n";
        }

        bool isPragmaPackNeeded = element->udt.correctAlignment < element->udt.defaultAlignment;

        if (isPragmaPackNeeded)
        {
            text += "\r\n#pragma pack(pop)\r\n";
        }

        if (element->elementType == ElementType::udtType && element->udt.includes.count() > 0)
        {
            text.insert(0, QString("%1\r\n\r\n").arg(element->udt.includes.values().join("\r\n")));
        }

        if (isPragmaPackNeeded)
        {
            text.insert(0, QString("#pragma once\r\n\r\n#pragma pack(push, %1)\r\n\r\n").arg(element->udt.correctAlignment));
        }
        else
        {
            text.insert(0, QString("#pragma once\r\n\r\n"));
        }

        vTableIndices.clear();
        parentClassName.clear();
    }

    return text;
}

QString PDB::GetDataInfo(const Data* data, int level)
{
    QString text;
    QString declaration = data->declaration;

    if (!options->includeConstKeyword)
    {
        //This checks are needed in case of templates

        if (declaration.contains(", const"))
        {
            declaration.replace(" const", " ");
        }
        else if (declaration.contains(" const"))
        {
            declaration.replace(" const", "");
        }
        else if (declaration.contains("const "))
        {
            declaration.replace("const ", "");
        }
    }

	if (!options->includeVolatileKeyword)
	{
        //This checks are needed in case of templates

		if (declaration.contains(", volatile"))
		{
			declaration.replace(" volatile", " ");
		}
		else if (declaration.contains(" volatile"))
		{
			declaration.replace(" volatile", "");
		}
		else if (declaration.contains("volatile "))
		{
			declaration.replace("volatile ", "");
		}
	}

    if (data->hasChildren)
    {
        text += declaration;
    }
    else
    {
        text += GetTab(level) + declaration;
    }

    if (data->value.isValid)
    {
        text += QString(" = %1").arg(data->value.value.toString());
    }

    return text;
}

QString PDB::GetFunctionInfo(const Element* element, int level)
{
    QString text;

    if (element->function.isVariadic && options->useTemplateFunction)
    {
        text += ImplementVariadicTemplateFunction(element, level);
    }
    else
    {
        text += GetTab(level) + element->function.prototype;
    }

    return text;
}

Data PDB::ConvertRecordTypeToData(const RecordType* recordType)
{
    Data data = {};

    data.baseType = recordType->baseType;
    data.isTypeConst = recordType->isTypeConst;
    data.isTypeVolatile = recordType->isTypeVolatile;
    data.isPointerConst = recordType->isPointerConst;
    data.isPointerVolatile = recordType->isPointerVolatile;
    data.typeName = recordType->typeName;
    data.originalTypeName = recordType->originalTypeName;
    data.size = recordType->size;
    data.isPointer = recordType->isPointer;
    data.isReference = recordType->isReference;
    data.pointerLevel = recordType->pointerLevel;
    data.referenceLevel = recordType->referenceLevel;
    data.isArray = recordType->isArray;
    data.arrayCount = recordType->arrayCount;
    data.functionReturnType = recordType->functionReturnType2;
    data.functionParameters = recordType->functionParameters;
    data.isVariadicFunction = recordType->isVariadicFunction;
    data.callingConvention = recordType->callingConvention;
    data.isFunctionPointer = recordType->isFunctionPointer;
    data.noType = recordType->noType;

    return data;
}

void PDB::GetTemplateIncludes(QString templateTypeName, const QString& parentClassName)
{
    int length = templateTypeName.length();
    int startIndex = 0;
    int endIndex = length - 1;
    QString include = "";

    if (templateTypeName.contains(", const"))
    {
        templateTypeName.replace(" const", " ");
    }
    else if (templateTypeName.contains(" const"))
    {
        templateTypeName.replace(" const", "");
    }
    else if (templateTypeName.contains("const "))
    {
        templateTypeName.replace("const ", "");
    }

    if (templateTypeName.contains("*"))
    {
        templateTypeName.replace("*", "");
    }

    if (templateTypeName.contains("&"))
    {
        templateTypeName.replace("&", "");
    }

    templateTypeName.replace("class ", "");
    templateTypeName.replace("struct ", "");
    templateTypeName.replace("union ", "");
    templateTypeName.replace("enum ", "");
    templateTypeName.replace(">", "");

    QStringList names = templateTypeName.split("<");
    int count = names.count();

    for (int i = 0; i < count; i++)
    {
        if (names.at(i).contains("("))
        {
            if (!names.at(i).endsWith(")"))
            {
                names[i].append(")");
            }

            int startIndex = names.at(i).indexOf("(");
            int endIndex = names.at(i).indexOf(")");
            QString functionPrototype;
            bool isFirstName = true;

            for (int j = endIndex; j >= 0; j--)
            {
                if (j < startIndex && names.at(i).at(j) == ',')
                {
                    isFirstName = false;

                    break;
                }

                functionPrototype.insert(0, names.at(i).at(j));
            }

            startIndex = functionPrototype.indexOf("(");
            endIndex = functionPrototype.indexOf(")");

            QStringList names2 = functionPrototype.mid(startIndex + 1, endIndex - startIndex - 1).split(", ");
            int count2 = names2.count();

            for (int j = 0; j < count2; j++)
            {
                if (names2.at(j).length() == 1 && names2.at(j).at(0).isDigit())
                {
                    continue;
                }

                AddTypeNameToIncludesList(names2.at(j), parentClassName);
            }

            if (!isFirstName)
            {
                functionPrototype.insert(0, ",");
            }

            names[i].replace(functionPrototype, "");

            names2 = names.at(i).split(", ");
            count2 = names2.count();

            for (int j = 0; j < count2; j++)
            {
                if (names2.at(j).length() == 1 && names2.at(j).at(0).isDigit())
                {
                    continue;
                }

                AddTypeNameToIncludesList(names2.at(j), parentClassName);
            }
        }
        else
        {
            if (names.at(i).endsWith(")"))
            {
                names[i].remove(names[i].length() - 1, 1);
            }

            QStringList names2 = names.at(i).split(", ");
            int count2 = names2.count();

            for (int j = 0; j < count2; j++)
            {
                if (names2.at(j).length() > 1 && !names2.at(j).at(0).isDigit())
                {
                    AddTypeNameToIncludesList(names2.at(j), parentClassName);
                }
            }
        }
    }
}

bool PDB::CheckIfNameOfMainOrInnerUDT(const QString& typeName, const QString& parentClassName)
{
    bool isNameOfMainOrInnerUDT = false;

    if (parentClassName.contains("::"))
    {
        if (parentClassName.contains("<"))
        {
            int length = parentClassName.length();
            int indexOfOperator = 0;
            int temp = 0;

            for (int i = 0; i < length; i++)
            {
                temp = parentClassName.indexOf("::", temp + 1);

                if (temp == -1)
                {
                    break;
                }

                QString leftSide = parentClassName.mid(0, temp);
                QString rightSide = parentClassName.mid(temp);

                if (leftSide.count('<') == leftSide.count('>') && rightSide.count('<') == rightSide.count('>'))
                {
                    indexOfOperator = temp;

                    if (typeName.startsWith(parentClassName.mid(0, indexOfOperator)))
                    {
                        isNameOfMainOrInnerUDT = true;

                        break;
                    }
                }
            }
        }
        else
        {
            QList<QString> names = parentClassName.split("::");
            int count = names.count();

            for (int i = 0; i < count; i++)
            {
                if (typeName.startsWith(names.at(i)))
                {
                    isNameOfMainOrInnerUDT = true;

                    break;
                }
            }
        }
    }
    else
    {
        if (typeName.contains("::") && typeName.startsWith(parentClassName))
        {
            isNameOfMainOrInnerUDT = true;
        }
        else if (typeName == parentClassName)
        {
            isNameOfMainOrInnerUDT = true;
        }
    }

    return isNameOfMainOrInnerUDT;
}

void PDB::AddTypeNameToIncludesList(const QString& typeName, const QString& parentClassName)
{
    QString name = typeName;

    if (name.contains("<"))
    {
        GetTemplateIncludes(name, parentClassName);
    }
    else
    {
        if (name.contains("::"))
        {
            name = name.mid(0, name.indexOf("::"));
        }

        QHash<QString, quint32>::const_iterator it = baseTypes.find(name);

        if (name.length() > 1 &&
            it == baseTypes.end() &&
            !CheckIfNameOfMainOrInnerUDT(name, parentClassName))
        {
            QString include = QString("#include \"%1.h\"").arg(name);

            includes.insert(include);
        }
    }
}

void PDB::AddComments(const Element* element, QString& text, int i)
{
    if (!options->displayComments)
    {
        return;
    }

    if (element->children.at(i).elementType == ElementType::dataType)
    {
		DWORD relativeVirtualAddress, fileOffset;
		QString relativeVirtualAddress2, fileOffset2;

        if (element->children.at(i).data.dataKind == DataIsStaticMember)
        {
            relativeVirtualAddress = element->children.at(i).data.relativeVirtualAddress;
            fileOffset = peHeaderParser->ConvertRVAToFileOffset(relativeVirtualAddress);

            relativeVirtualAddress2 = QString::number(relativeVirtualAddress, 16).toUpper();
            fileOffset2 = QString::number(fileOffset, 16).toUpper();

            text += QString(" //Offset = 0x%1, File Offset = 0x%2").arg(relativeVirtualAddress2).arg(fileOffset2);
        }
        else
        {
            relativeVirtualAddress = element->children.at(i).offset;
            relativeVirtualAddress2 = QString::number(relativeVirtualAddress, 16).toUpper();

            text += QString(" //Offset = 0x%1").arg(relativeVirtualAddress2);
        }

        DWORD size = element->children.at(i).size;
        QString size2 = QString::number(size, 16).toUpper();

        text += QString(" Size = 0x%2").arg(size2);

		if (element->children.at(i).numberOfBits > 0)
		{
            DWORD bitOffset = element->children.at(i).bitOffset;
			QString bitOffset2 = QString::number(bitOffset, 16).toUpper();

            DWORD numberOfBits = element->children.at(i).numberOfBits;
			QString numberOfBits2 = QString::number(numberOfBits, 16).toUpper();

            text += QString(" BitOffset = 0x%1, Number Of Bits = 0x%2").arg(bitOffset2).arg(numberOfBits2);
		}
    }
    else if (element->children.at(i).elementType == ElementType::functionType)
    {
        DWORD relativeVirtualAddress = element->children.at(i).function.relativeVirtualAddress;
        QString relativeVirtualAddress2 = QString::number(relativeVirtualAddress, 16).toUpper();

        DWORD fileOffset = peHeaderParser->ConvertRVAToFileOffset(relativeVirtualAddress);
        QString fileOffset2 = QString::number(fileOffset, 16).toUpper();

        if (element->children.at(i).function.isVirtual)
        {
            int vTableIndex = element->children.at(i).function.virtualBaseOffset >> 2;
            QString indexOfVTable = QString::number(element->children.at(i).function.indexOfVTable);

            if (element->children.at(i).function.indexOfVTable == 0)
            {
                indexOfVTable.append(" (Main VTable)");
            }
            else
            {
                QString vTableName = element->udt.vTableNames[element->children.at(i).function.indexOfVTable];

                indexOfVTable.append(QString(" (%1)").arg(vTableName));
            }

            text += QString(" //Index = %1, Index Of VTable = %2, Offset = 0x%3, File Offset = 0x%4").arg(vTableIndex)
                .arg(indexOfVTable).arg(relativeVirtualAddress2).arg(fileOffset2);
        }
        else
        {
            text += QString(" //Offset = 0x%1, File Offset = 0x%2").arg(relativeVirtualAddress2).arg(fileOffset2);
        }
    }
}

void PDB::AddAdditionalNewLine(const Element* element, QString& text, int i)
{
    bool addNewLine = false;
    int childrenCount = element->children.count();

    if (element->children.at(i).elementType == ElementType::udtType ||
        element->children.at(i).elementType == ElementType::enumType)
    {
        if (i + 1 < childrenCount)
        {
            if (element->children.at(i + 1).elementType == ElementType::udtType ||
                element->children.at(i + 1).elementType == ElementType::enumType ||
                element->children.at(i + 1).elementType == ElementType::typedefType &&
                options->displayTypedefs)
            {
                addNewLine = true;
            }
            else if (element->children.at(i + 1).elementType == ElementType::dataType)
            {
                for (int j = i + 1; j < childrenCount; j++)
                {
                    if (element->children.at(j).elementType == ElementType::dataType)
                    {
                        if (element->children.at(j).data.isCompilerGenerated)
                        {
                            if (options->displayVTablePointerIfExists &&
                                element->children.at(j).data.isVTablePointer ||
                                element->children.at(j).data.isPadding &&
                                options->displayPaddingBytes)
                            {
                                addNewLine = true;

                                break;
                            }
                        }
                        else
                        {
                            addNewLine = true;

                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }

                //In case there is only padding between inner type and function which shouldn't be displayed
                if (!addNewLine)
                {
                    addNewLine = AddAdditionalNewLineBeforeFunc(element, i);
                }
            }
            else if (element->children.at(i + 1).elementType == ElementType::functionType)
            {
                addNewLine = AddAdditionalNewLineBeforeFunc(element, i);
            }
        }
    }
    else if (element->children.at(i).elementType == ElementType::typedefType)
    {
        if (options->displayTypedefs && i < childrenCount - 1)
        {
            if (element->children.at(i + 1).elementType == ElementType::dataType)
            {
                for (int j = i + 1; j < childrenCount; j++)
                {
                    if (element->children.at(j).elementType == ElementType::dataType)
                    {
                        if (element->children.at(j).data.isCompilerGenerated)
                        {
                            if (options->displayVTablePointerIfExists &&
                                element->children.at(j).data.isVTablePointer ||
                                element->children.at(j).data.isPadding &&
                                options->displayPaddingBytes)
                            {
                                addNewLine = true;

                                break;
                            }
                        }
                        else
                        {
                            addNewLine = true;

                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }

                if (!addNewLine)
                {
                    addNewLine = AddAdditionalNewLineBeforeFunc(element, i);
                }
            }
            else if (element->children.at(i + 1).elementType == ElementType::functionType)
            {
                addNewLine = AddAdditionalNewLineBeforeFunc(element, i);
            }
        }
    }
    else if (element->children.at(i).elementType == ElementType::dataType &&
        element->children.at(i).data.dataKind != DataIsStaticMember)
    {
        if (i == 0 && element->children.at(i).data.isCompilerGenerated)
        {
            if (options->displayVTablePointerIfExists &&
                element->children.at(i).data.isVTablePointer ||
                element->children.at(i).data.isPadding &&
                options->displayPaddingBytes) //Only end align can appear as first data member
            {
                addNewLine = true;
            }
        }
        else if (i > 0 &&
            element->children.at(i - 1).elementType == ElementType::baseClassType &&
            element->children.at(i).data.isCompilerGenerated)
        {
            if (options->displayVTablePointerIfExists &&
                element->children.at(i).data.isVTablePointer ||
                element->children.at(i).data.isPadding &&
                options->displayPaddingBytes)
            {
                addNewLine = true;
            }
        }
        else if (i < childrenCount - 1)
        {
            if (element->children.at(i + 1).elementType == ElementType::dataType &&
                element->children.at(i + 1).data.dataKind == DataIsStaticMember)
            {
                /*if (element->children.at(i).data.isCompilerGenerated)
                {
                    if (options->displayVTablePointerIfExists &&
                        element->children.at(i).data.isVTablePointer ||
                        element->children.at(i).data.isPadding &&
                        options->displayPaddingBytes)
                    {
                        addNewLine = true;
                    }
                }
                else
                {
                    addNewLine = true;
                }*/

                if (i > 0 &&
                    (element->children.at(i - 1).elementType == ElementType::dataType &&
                        element->children.at(i - 1).data.hasChildren ||
                        element->children.at(i - 1).elementType != ElementType::dataType) &&
                    element->children.at(i).data.isCompilerGenerated)
                {
                    if (options->displayVTablePointerIfExists &&
                        element->children.at(i).data.isVTablePointer ||
                        element->children.at(i).data.isPadding &&
                        options->displayPaddingBytes)
                    {
                        addNewLine = true;
                    }
                }
                else
                {
                    addNewLine = true;
                }
            }
            else if (element->children.at(i + 1).elementType == ElementType::functionType)
            {
                /*
                * In this case new line is already added after udt or enum type so another new line shouldn't be added
                * if option for displaying padding isn't enabled
                */
                if (i > 0 &&
                    (element->children.at(i - 1).elementType == ElementType::dataType &&
                        element->children.at(i - 1).data.hasChildren ||
                        element->children.at(i - 1).elementType != ElementType::dataType) &&
                    element->children.at(i).data.isCompilerGenerated)
                {
                    if (options->displayVTablePointerIfExists &&
                        element->children.at(i).data.isVTablePointer ||
                        element->children.at(i).data.isPadding &&
                        options->displayPaddingBytes)
                    {
                        addNewLine = AddAdditionalNewLineBeforeFunc(element, i);
                    }
                }
                else
                {
                    addNewLine = AddAdditionalNewLineBeforeFunc(element, i);
                }

                //addNewLine = AddAdditionalNewLineBeforeFunc(element, i);
            }
            /*else if (element->children.at(i + 1).elementType == ElementType::udtType)
            {
                if (element->children.at(i).data.isCompilerGenerated)
                {
                    if (options->displayVTablePointerIfExists &&
                        element->children.at(i).data.isVTablePointer ||
                        element->children.at(i).data.isPadding &&
                        options->displayPaddingBytes)
                    {
                        addNewLine = true;
                    }
                }
                else
                {
                    addNewLine = true;
                }
            }*/
            else if (element->children.at(i).data.hasChildren &&
                element->children.at(i + 1).elementType == ElementType::dataType)
            {
                for (int j = i + 1; j < childrenCount; j++)
                {
                    if (element->children.at(j).elementType == ElementType::dataType)
                    {
                        if (element->children.at(j).data.isCompilerGenerated)
                        {
                            if (options->displayVTablePointerIfExists &&
                                element->children.at(j).data.isVTablePointer ||
                                element->children.at(j).data.isPadding &&
                                options->displayPaddingBytes)
                            {
                                addNewLine = true;

                                break;
                            }
                        }
                        else
                        {
                            addNewLine = true;

                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }

                if (!addNewLine)
                {
                    addNewLine = AddAdditionalNewLineBeforeFunc(element, i);
                }
            }
            else if (element->children.at(i + 1).elementType == ElementType::dataType &&
                element->children.at(i + 1).data.hasChildren)
            {
                addNewLine = true;
            }
        }
    }
    else if (element->children.at(i).elementType == ElementType::dataType &&
        element->children.at(i).data.dataKind == DataIsStaticMember)
    {
        if (i < childrenCount - 1)
        {
            if (element->children.at(i + 1).elementType == ElementType::functionType)
            {
                addNewLine = AddAdditionalNewLineBeforeFunc(element, i);
            }
            else if (element->children.at(i + 1).elementType == ElementType::dataType &&
                element->children.at(i + 1).data.hasChildren)
            {
                addNewLine = true;
            }
        }
    }
    else if (element->children.at(i).elementType == ElementType::functionType)
    {
        if (element->children.at(i).function.isVirtual)
        {
            if (i < childrenCount - 1)
            {
                if (element->children.at(i + 1).elementType == ElementType::functionType &&
                    element->children.at(i + 1).function.isVirtual &&
                    !element->children.at(i + 1).function.isNonImplemented &&
                    element->children.at(i + 1).function.indexOfVTable >
                    element->children.at(i).function.indexOfVTable)
                {
                    addNewLine = true;
                }
                else if (element->children.at(i + 1).elementType == ElementType::functionType &&
                    !element->children.at(i + 1).function.isVirtual)
                {
                    addNewLine = AddAdditionalNewLineBeforeFunc(element, i);
                }
            }
        }
        else if (i < childrenCount - 1 &&
            element->children.at(i + 1).function.isStatic &&
            element->children.at(i + 1).function.isGeneratedByApp &&
            !(element->children.at(i).function.isStatic &&
                element->children.at(i).function.isGeneratedByApp))
        {
            addNewLine = true;
        }
    }

    if (addNewLine)
    {
        text += "\r\n";
    }
}

bool PDB::AddAdditionalNewLineBeforeFunc(const Element* element, int i)
{
    bool addNewLine = true;

    if (element->children.at(i + 1).function.isNonImplemented && !options->displayNonImplementedFunctions)
    {
        addNewLine = false;

        /*
        * In case there is function that comes right after last variable but it's offset is 0 and it shouldn't be displayed
        * then check if there is any other function below with offset different then 0 and
        * if it there is any then add new line
        */

        int count = element->children.count();

        for (int j = i + 2; j < count; j++)
        {
            if (element->children.at(j).elementType != ElementType::functionType)
            {
                break;
            }
            else
            {
                if (!element->children.at(j).function.isNonImplemented)
                {
                    addNewLine = true;

                    break;
                }
            }
        }
    }

    return addNewLine;
}

void PDB::RemoveScopeResolutionOperators(QString& text, const QString& parentClassName)
{
    if (text.contains("<"))
    {
        int length = text.length();
        int indexOfOperator = 0;
        int temp = 0;

        for (int i = 0; i < length; i++)
        {
            temp = text.indexOf("::", temp + 1);

            if (temp == -1)
            {
                break;
            }

            QString leftSide = text.mid(0, temp);
            QString rightSide = text.mid(temp);

            if (leftSide.count('<') == leftSide.count('>') && rightSide.count('<') == rightSide.count('>'))
            {
                indexOfOperator = temp;
            }
        }

        bool removeScopeOperator = false;

        if (parentClassName.length() > 0)
        {
            /*if (parentClassName.contains(text.mid(0, indexOfOperator)))
            {
                removeScopeOperator = true;
            }*/

            if (parentClassName.startsWith(text.mid(0, indexOfOperator)))
            {
                removeScopeOperator = true;
            }
        }
        else
        {
            removeScopeOperator = true;
        }

        if (removeScopeOperator)
        {
            if (indexOfOperator > 0)
            {
                text.remove(0, indexOfOperator + 2);
            }

            if (text.contains("::"))
            {
                if (parentClassName.length() == 0)
                {
                    if (!text.contains("<"))
                    {
                        text = text.mid(text.lastIndexOf("::") + 2);
                    }
                }
                else
                {
                    if (parentClassName.contains("<"))
                    {
                        int length = parentClassName.length();
                        int indexOfOperator = 0;
                        int temp = 0;

                        for (int i = 0; i < length; i++)
                        {
                            temp = parentClassName.indexOf("::", temp + 1);

                            if (temp == -1)
                            {
                                break;
                            }

                            QString leftSide = parentClassName.mid(0, temp);
                            QString rightSide = parentClassName.mid(temp);

                            if (leftSide.count('<') == leftSide.count('>') && rightSide.count('<') == rightSide.count('>'))
                            {
                                indexOfOperator = temp;
                            }
                        }

                        if (indexOfOperator > 0)
                        {
                            text.replace(parentClassName.mid(0, indexOfOperator + 2), "");
                            text.replace(QString("%1::").arg(parentClassName.mid(indexOfOperator + 2)), "");

                            QStringList list = text.split("::");
                            int count = list.count();

                            for (int i = 0; i < count; i++)
                            {
                                if (!list.at(i).contains("<"))
                                {
                                    text.replace(QString("%1::").arg(list.at(i)), "");
                                }
                            }
                        }
                    }
                    else
                    {
                        QList<QString> names = parentClassName.split("::");
                        int count = names.count();

                        for (int i = 0; i < count; i++)
                        {
                            text.replace(QString("%1::").arg(names.at(i)), "");
                        }
                    }
                }
            }
        }
    }
    else
    {
        if (parentClassName.length() == 0)
        {
            text = text.mid(text.lastIndexOf("::") + 2);
        }
        else
        {
            if (parentClassName.contains("<"))
            {
                int length = parentClassName.length();
                int indexOfOperator = 0;
                int temp = 0;

                for (int i = 0; i < length; i++)
                {
                    temp = parentClassName.indexOf("::", temp + 1);

                    if (temp == -1)
                    {
                        break;
                    }

                    QString leftSide = parentClassName.mid(0, temp);
                    QString rightSide = parentClassName.mid(temp);

                    if (leftSide.count('<') == leftSide.count('>') && rightSide.count('<') == rightSide.count('>'))
                    {
                        indexOfOperator = temp;
                    }
                }

                if (indexOfOperator > 0)
                {
                    text.replace(parentClassName.mid(0, indexOfOperator + 2), "");
                    text.replace(QString("%1::").arg(parentClassName.mid(indexOfOperator + 2)), "");

                    QStringList list = text.split("::");
                    int count = list.count();

                    for (int i = 0; i < count; i++)
                    {
                        if (!list.at(i).contains("<"))
                        {
                            text.replace(QString("%1::").arg(list.at(i)), "");
                        }
                    }
                }
            }
            else
            {
                QList<QString> names = parentClassName.split("::");
                int count = names.count();

                for (int i = 0; i < count; i++)
                {
                    text.replace(QString("%1::").arg(names.at(i)), "");
                }
            }
        }
    }
}

bool PDB::CheckIfInnerUDTBelongsToMainUDT(Element* element, Element* innerUDT)
{
    bool belongsToMainUDT = false;
    int baseClassChildrenCount = innerUDT->baseClassChildren.count();
    int enumChildrenCount = innerUDT->enumChildren.count();
    int udtClassChildrenCount = innerUDT->udtChildren.count();
    int dataClassChildrenCount = innerUDT->dataChildren.count();
    int staticDataClassChildrenCount = innerUDT->staticDataChildren.count();
    int virtualFunctionChildrenCount = innerUDT->virtualFunctionChildren.count();
    int nonVirtualFunctionChildrenCount = innerUDT->nonVirtualFunctionChildren.count();

    /*
    * Use contains instead of startsWith because when startsWith is used then
    * checkIfInnerUDTBelongsToMainUDT function won't always return correct result
    */

    for (int i = 0; i < baseClassChildrenCount; i++)
    {
        if (innerUDT->baseClassChildren.at(i).baseClass.parentClassName.contains(element->udt.name))
        {
            belongsToMainUDT = true;

            break;
        }
    }

    for (int i = 0; i < enumChildrenCount; i++)
    {
        if (innerUDT->enumChildren.at(i).enum1.parentClassName.contains(element->udt.name))
        {
            belongsToMainUDT = true;

            break;
        }
    }

    for (int i = 0; i < udtClassChildrenCount; i++)
    {
        if (innerUDT->udtChildren.at(i).udt.parentClassName.contains(element->udt.name))
        {
            belongsToMainUDT = true;

            break;
        }
    }

    for (int i = 0; i < dataClassChildrenCount; i++)
    {
        if (innerUDT->dataChildren.at(i).data.parentClassName.contains(element->udt.name))
        {
            belongsToMainUDT = true;

            break;
        }
    }

    for (int i = 0; i < staticDataClassChildrenCount; i++)
    {
        if (innerUDT->staticDataChildren.at(i).data.parentClassName.contains(element->udt.name))
        {
            belongsToMainUDT = true;

            break;
        }
    }

    for (int i = 0; i < virtualFunctionChildrenCount; i++)
    {
        if (innerUDT->virtualFunctionChildren.at(i).function.parentClassName.contains(element->udt.name))
        {
            belongsToMainUDT = true;

            break;
        }
    }

    for (int i = 0; i < nonVirtualFunctionChildrenCount; i++)
    {
        if (innerUDT->nonVirtualFunctionChildren.at(i).function.parentClassName.contains(element->udt.name))
        {
            belongsToMainUDT = true;

            break;
        }
    }

    if (baseClassChildrenCount == 0 &&
        enumChildrenCount == 0 &&
        udtClassChildrenCount == 0 &&
        dataClassChildrenCount == 0 &&
        staticDataClassChildrenCount == 0 &&
        virtualFunctionChildrenCount == 0 &&
        nonVirtualFunctionChildrenCount == 0)
    {
        if (innerUDT->udt.parentClassName.contains(element->udt.name))
        {
            belongsToMainUDT = true;
        }
    }

    return belongsToMainUDT;
}

QString PDB::GetParentClassName(const QString& typeName)
{
    QString parentClassName;

    if (typeName.contains("<"))
    {
        int length = typeName.length();
        int indexOfOperator = 0;
        int temp = 0;

        for (int i = 0; i < length; i++)
        {
            temp = typeName.indexOf("::", temp + 1);

            if (temp == -1)
            {
                break;
            }

            QString leftSide = typeName.mid(0, temp);
            QString rightSide = typeName.mid(temp);

            if (leftSide.count('<') == leftSide.count('>') && rightSide.count('<') == rightSide.count('>'))
            {
                indexOfOperator = temp;
            }
        }

        if (indexOfOperator > 0)
        {
            parentClassName = typeName.mid(0, indexOfOperator);
        }
    }
    else
    {
        parentClassName = typeName.mid(0, typeName.lastIndexOf("::"));
    }

    return parentClassName;
}

QString PDB::GetParentClassName(Element* element)
{
    QString parentClassName;
    int baseClassChildrenCount = element->baseClassChildren.count();
    int enumChildrenCount = element->enumChildren.count();
    int udtClassChildrenCount = element->udtChildren.count();
    int dataClassChildrenCount = element->dataChildren.count();
    int staticDataClassChildrenCount = element->staticDataChildren.count();
    int virtualFunctionChildrenCount = element->virtualFunctionChildren.count();
    int nonVirtualFunctionChildrenCount = element->nonVirtualFunctionChildren.count();

    if (baseClassChildrenCount > 0)
    {
        parentClassName = element->baseClassChildren.at(0).baseClass.parentClassName;
    }

    if (enumChildrenCount > 0)
    {
        parentClassName = element->enumChildren.at(0).enum1.parentClassName;
    }

    if (udtClassChildrenCount > 0)
    {
        parentClassName = element->udtChildren.at(0).udt.parentClassName;
    }

    if (dataClassChildrenCount > 0)
    {
        parentClassName = element->dataChildren.at(0).data.parentClassName;
    }

    if (staticDataClassChildrenCount > 0)
    {
        parentClassName = element->staticDataChildren.at(0).data.parentClassName;
    }

    if (virtualFunctionChildrenCount > 0)
    {
        parentClassName = element->virtualFunctionChildren.at(0).function.parentClassName;
    }

    if (nonVirtualFunctionChildrenCount > 0)
    {
        parentClassName = element->nonVirtualFunctionChildren.at(0).function.parentClassName;
    }

    return parentClassName;
}

bool PDB::CheckIfPublicKeywordShouldBeAdded(const Element* element)
{
    bool addPublicKeyword = false;
    int dataChildrenCount = element->dataChildren.count();
    int virtualFunctionChildrenCount = element->virtualFunctionChildren.count();
    int nonVirtualFunctionChildrenCount = element->nonVirtualFunctionChildren.count();

    if (element->enumChildren.count() > 0 ||
        element->udtChildren.count() ||
        element->typedefChildren.count() > 0 &&
        options->displayTypedefs ||
        element->staticDataChildren.count() > 0)
    {
        addPublicKeyword = true;
    }
    else
    {
        for (int i = 0; i < dataChildrenCount; i++)
        {
            if (element->dataChildren.at(i).data.isCompilerGenerated)
            {
                if (!options->displayVTablePointerIfExists &&
                    element->dataChildren.at(i).data.isVTablePointer ||
                    !element->dataChildren.at(i).data.isPadding &&
                    options->displayPaddingBytes)
                {
                    addPublicKeyword = false;
                }
            }
            else
            {
                addPublicKeyword = true;

                break;
            }
        }

        for (int i = 0; i < virtualFunctionChildrenCount; i++)
        {
            if (element->virtualFunctionChildren.at(i).function.isNonImplemented)
            {
                if (!options->displayNonImplementedFunctions)
                {
                    addPublicKeyword = false;
                }
            }
            else
            {
                addPublicKeyword = true;

                break;
            }
        }

        for (int i = 0; i < nonVirtualFunctionChildrenCount; i++)
        {
            if (element->nonVirtualFunctionChildren.at(i).function.isNonImplemented)
            {
                if (!options->displayNonImplementedFunctions)
                {
                    addPublicKeyword = false;
                }
            }
            else
            {
                addPublicKeyword = true;

                break;
            }
        }
    }

    return addPublicKeyword;
}

bool PDB::CheckIfHasAnyEmptyBaseClass(const Element* element)
{
    int baseClassChildrenCount = element->baseClassChildren.count();

    for (int i = 0; i < baseClassChildrenCount; i++)
    {
        if (element->baseClassChildren.at(i).dataChildren.count() == 1)
		{
            if (element->baseClassChildren.at(i).dataChildren.at(0).data.isEndPadding)
			{
                return true;
			}
		}
    }

    return false;
}

bool PDB::CheckIfRVOIsAppliedToFunction(IDiaSymbol* symbol)
{
    bool isRVOApplied = false;
    DWORD symTag = 0;

    if (symbol->get_symTag(&symTag) == S_OK && symTag == SymTagUDT)
    {
		ULONGLONG size;

		if (symbol->get_length(&size) == S_OK)
		{
			isRVOApplied = size > 8;
		}

		if (!isRVOApplied)
		{
            /*
            * This is case of small UDT which possibly fits into a register (or two)
            * but it has to be a POD for that (it shouldn't have constructor or assignment operators)
            */
            BOOL hasConstructor = 0, hasAssignmentOperator = 0, hasCastOperator = 0;

			if (symbol->get_constructor(&hasConstructor) == S_OK && hasConstructor ||
				symbol->get_hasAssignmentOperator(&hasAssignmentOperator) == S_OK && hasAssignmentOperator ||
				symbol->get_hasCastOperator(&hasCastOperator) == S_OK && hasCastOperator)
			{
				isRVOApplied = true;
			}
		}
    }

    return isRVOApplied;
}

void PDB::DeclareFunctionsForStaticVariables(Element* element)
{
    int staticDataChildrenCount = element->staticDataChildren.count();
    Element newElement;

    for (int i = 0; i < staticDataChildrenCount; i++)
    {
        newElement = CreateGetter(element, i);

        element->nonVirtualFunctionChildren.append(newElement);
    }

	for (int i = 0; i < staticDataChildrenCount; i++)
	{
        newElement = CreateSetter(element, i);

		element->nonVirtualFunctionChildren.append(newElement);
	}
}

void PDB::ImplementFunctionsForStaticVariables(const Element* element, QString& cppCode)
{
    int nonVirtualFunctionChildrenCount = element->nonVirtualFunctionChildren.count();
    static const DataOptions dataOptions = { true, false };
    static const FunctionOptions functionOptions = { true, false, false, true };
    int n = 0;
    Element element2;

    for (int i = 0; i < nonVirtualFunctionChildrenCount; i++)
    {
        if (element->nonVirtualFunctionChildren.at(i).function.isStatic &&
            element->nonVirtualFunctionChildren.at(i).function.isGeneratedByApp)
        {
            element2 = element->nonVirtualFunctionChildren.at(i);

            QString type;
            QString offset;
			QString functionPrototype = FunctionTypeToString(const_cast<Element*>(&element2), &functionOptions);
			QString baseAddressVariableName = GetBaseAddressVariableName();

            //DWORD offset = element->staticDataChildren.at(n++).data.relativeVirtualAddress;

            //CHANGE OFFSET NUMBERS TO BE UPPERCASE

            if (n == element->staticDataChildren.count())
            {
                n = 0;
            }

            cppCode += QString("%1\r\n{\r\n\t").arg(functionPrototype);

            if (element2.function.returnType1.baseType == 1)
            {
                Data dataType = element2.dataChildren.at(0).data;

                if (!element->staticDataChildren.at(n++).data.isTypeConst)
                {
                    dataType.isTypeConst = false;
                }

                type = DataTypeToString(&dataType, &dataOptions);
                offset = QString::number(element2.dataChildren.at(0).data.relativeVirtualAddress, 16).toUpper();

                cppCode += "*";

				if (element2.dataChildren.at(0).data.isPointer)
				{
					cppCode += QString("reinterpret_cast<%1>").arg(type);
				}
				else
				{
					cppCode += QString("reinterpret_cast<%1*>").arg(type);
				}

                cppCode += QString("(BaseAddresses::%2 + 0x%3) = ").arg(baseAddressVariableName).arg(offset);

                if (element2.dataChildren.at(0).data.isPointer)
                {
                    cppCode += "*";
                }

                cppCode += QString("%1").arg(element2.dataChildren.at(0).data.name);
            }
            else
            {
                type = element->nonVirtualFunctionChildren.at(i).function.returnType2;
                offset = QString::number(element->nonVirtualFunctionChildren.at(i).function.returnType1.relativeVirtualAddress, 16)
                    .toUpper();

                cppCode += "return ";

                if (element2.function.returnType1.isPointer)
                {
                    cppCode += QString("reinterpret_cast<%1>").arg(type);
                }
                else
                {
                    cppCode += QString("*reinterpret_cast<%1*>").arg(type);
                }

                cppCode += QString("(BaseAddresses::%2 + 0x%3)").arg(baseAddressVariableName).arg(offset);
            }

            cppCode += ";\r\n}\r\n\r\n";
        }
    }
}

Element PDB::CreateGetter(Element* element, const int i)
{
    static const DataOptions dataOptions = { true, false };
    static const FunctionOptions functionOptions;

    QString name = RemoveHungarianNotationFromVariable(element->staticDataChildren.at(i).data.name);
	Element newElement = {};

    newElement.elementType = ElementType::functionType;
    newElement.function.returnType1 = element->staticDataChildren.at(i).data;
    newElement.function.returnType1.dataKind = DataIsUnknown;

	if (newElement.function.returnType1.isArray)
	{
		newElement.function.returnType1.isArray = false;
		newElement.function.returnType1.isPointer = true;
		newElement.function.returnType1.pointerLevel = 1;
	}

    QString type = DataTypeToString(&newElement.function.returnType1, &dataOptions);

    newElement.function.returnType2 = type;

	if (name.contains("_"))
	{
		newElement.function.name = QString("Get%1").arg(ConvertSnakeCaseToPascalCase(name));
	}
	else
	{
		newElement.function.name = QString("Get%1").arg(ConvertCamelCaseToPascalCase(name));
	}

	newElement.function.isStatic = true;
	newElement.function.callingConvention = CV_CALL_THISCALL;
	newElement.function.isGeneratedByApp = true;
	newElement.function.access = CV_public;
	newElement.function.parentClassName = GetParentClassName(element);
	newElement.function.prototype = FunctionTypeToString(const_cast<Element*>(&newElement), &functionOptions);

    return newElement;
}

Element PDB::CreateSetter(Element* element, const int i)
{
    static const DataOptions dataOptions = { true, false };
    static const FunctionOptions functionOptions;

    QString name = RemoveHungarianNotationFromVariable(element->staticDataChildren.at(i).data.name);
    Element newElement = {};

    newElement.elementType = ElementType::functionType;
    newElement.function.returnType1.baseType = 1;
    newElement.function.returnType2 = "void";

    if (name.contains("_"))
    {
        newElement.function.name = QString("Set%1").arg(ConvertSnakeCaseToPascalCase(name));
    }
    else
    {
        newElement.function.name = QString("Set%1").arg(ConvertCamelCaseToPascalCase(name));
    }

    newElement.function.isStatic = true;
    newElement.function.callingConvention = CV_CALL_THISCALL;
    newElement.function.isGeneratedByApp = true;
    newElement.function.access = CV_public;
    newElement.function.parentClassName = GetParentClassName(element);

    Element parameterElement = {};

    parameterElement.elementType = ElementType::dataType;
    parameterElement.data = element->staticDataChildren.at(i).data;
    //parameterElement.data.name = element->staticDataChildren.at(i).data.name;
    //parameterElement.data.typeName = element->staticDataChildren.at(i).data.typeName;
    //parameterElement.data.isPointer = true;
    //parameterElement.data.pointerLevel = 1;
    parameterElement.data.isTypeConst = true;
    parameterElement.data.dataKind = DataIsParam;

    if (name.contains("_"))
    {
        parameterElement.data.name = ConvertSnakeCaseToCamelCase(name);
    }
    else
    {
        parameterElement.data.name = ConvertPascalCaseToCamelCase(name);
    }

    if (element->staticDataChildren.at(i).data.isArray)
    {
        parameterElement.data.isArray = false;
        parameterElement.data.isPointer = true;
        parameterElement.data.pointerLevel = 1;
    }

    QString type = DataTypeToString(&parameterElement.data, &dataOptions);

    newElement.dataChildren.append(parameterElement);
    newElement.function.parameters.append(type);

    newElement.function.prototype = FunctionTypeToString(const_cast<Element*>(&newElement), &functionOptions);

    return newElement;
}

ULONGLONG PDB::GetFunctionSize(DWORD relativeVirtualAddress)
{
    ULONGLONG size = 0;
    IDiaEnumSymbolsByAddr* enumSymbolsByAddr = nullptr;
    IDiaSymbol* symbol = nullptr;

    if (SUCCEEDED(diaSession->getSymbolsByAddr(&enumSymbolsByAddr)))
    {
		if (SUCCEEDED(enumSymbolsByAddr->symbolByRVA(relativeVirtualAddress, &symbol)))
		{
            symbol->get_length(&size);
		}
    }

    return size;
}

void PDB::CheckIfDefaultCtorAndDtorAdded(Element* element)
{
    /*
    * Compiler doesn't generate virtual destructor for base class.
    * If base class has virtual destructor but child class doesn't have it then it will be generated.
    * Compiler generated virtual destructor is always placed at the end of vTable.
    * __vecDelDtor is not counted in countOfVirtualFunctions
    */

    if (element->elementType == ElementType::enumType ||
        element->elementType == ElementType::udtType &&
        element->udt.isUnnamed)
    {
        return;
    }

    if (options->addDefaultCtorAndDtorToUDT)
    {
        if (!element->udt.hasDefaultConstructor)
        {
            AddDefaultConstructor(element);
        }

        if (element->udt.hasVTable)
        {
            if (!element->udt.hasVirtualDestructor)
            {
                AddVirtualDestructor(element);
            }
        }
        else
        {
            if (!element->udt.hasDestructor)
            {
                AddDestructor(element);
            }
        }
    }
}

void PDB::AddDefaultConstructor(Element* element)
{
    Element newElement = {};

    newElement.function.name = element->udt.name;
    newElement.function.callingConvention = CV_CALL_THISCALL;
    newElement.function.isGeneratedByApp = true;
    newElement.function.isDefaultConstructor = true;
    newElement.function.access = CV_public;
    newElement.function.returnType1.baseType = 1;
    newElement.function.parentClassName = GetParentClassName(element);
    newElement.elementType = ElementType::functionType;

    static const FunctionOptions functionOptions;

    newElement.function.prototype = FunctionTypeToString(const_cast<Element*>(&newElement), &functionOptions);

    if (element->nonVirtualFunctionChildren.count() == 0)
    {
        element->nonVirtualFunctionChildren.append(newElement);
    }
    else
    {
        element->nonVirtualFunctionChildren.insert(0, newElement);
    }
}

void PDB::AddDestructor(Element* element)
{
    Element newElement = {};

    newElement.function.name = QString("~%1").arg(element->udt.name);
    newElement.function.callingConvention = CV_CALL_THISCALL;
    newElement.function.isGeneratedByApp = true;
    newElement.function.isDestructor = true;
    newElement.function.access = CV_public;
    newElement.function.returnType1.baseType = 1;
    newElement.function.parentClassName = GetParentClassName(element);
    newElement.elementType = ElementType::functionType;

    int lastConstructorIndex = -1;
    int nonVirtualFunctionChildrenCount = element->nonVirtualFunctionChildren.count();

    for (int i = 0; i < nonVirtualFunctionChildrenCount; i++)
    {
        if (element->nonVirtualFunctionChildren.at(i).function.isConstructor ||
            element->nonVirtualFunctionChildren.at(i).function.isDefaultConstructor)
        {
            lastConstructorIndex = i;
        }
        else if (lastConstructorIndex != -1)
        {
            break;
        }
    }

    static const FunctionOptions functionOptions;

    newElement.function.prototype = FunctionTypeToString(const_cast<Element*>(&newElement), &functionOptions);

    if (lastConstructorIndex + 1 == nonVirtualFunctionChildrenCount)
    {
        element->nonVirtualFunctionChildren.append(newElement);
    }
    else
    {
        element->nonVirtualFunctionChildren.insert(lastConstructorIndex + 1, newElement);
    }
}

void PDB::AddVirtualDestructor(Element* element)
{
    Element newElement = {};

    newElement.function.name = QString("~%1").arg(element->udt.name);
    newElement.function.callingConvention = CV_CALL_THISCALL;
    newElement.function.isGeneratedByApp = true;
    newElement.function.isVirtual = true;
    newElement.function.isDestructor = true;
    newElement.function.access = CV_public;
    newElement.function.returnType1.baseType = 1;
    newElement.function.parentClassName = GetParentClassName(element);
    newElement.elementType = ElementType::functionType;

    static const FunctionOptions functionOptions;

    newElement.function.prototype = FunctionTypeToString(const_cast<Element*>(&newElement), &functionOptions);

    int virtualFunctionChildrenCount = element->virtualFunctionChildren.count();

    if (virtualFunctionChildrenCount == 0)
    {
        newElement.function.virtualBaseOffset = 0;

        element->virtualFunctionChildren.append(newElement);
    }
    else
    {
        //This is not safe because if last virtual function is overriden offset will be 0
        /*DWORD lastVirtualFunctionBaseOffset = element->virtualFunctionChildren.last().function.virtualBaseOffset;

        if (type == CV_CFL_80386)
        {
            newElement.function.virtualBaseOffset = lastVirtualFunctionBaseOffset + 4;
        }
        else
        {
            newElement.function.virtualBaseOffset = lastVirtualFunctionBaseOffset + 8;
        }*/

        if (type == CV_CFL_80386)
        {
            newElement.function.virtualBaseOffset = (virtualFunctionChildrenCount + 1) * 4;
        }
        else
        {
            newElement.function.virtualBaseOffset = (virtualFunctionChildrenCount + 1) * 8;
        }

        element->virtualFunctionChildren.append(newElement);
    }
}

void PDB::AddCopyConstructor(Element* element)
{
    Element newElement = {};

    newElement.function.name = element->udt.name;
    newElement.function.callingConvention = CV_CALL_THISCALL;
    newElement.function.isGeneratedByApp = true;
    newElement.function.isCopyConstructor = true;
    newElement.function.access = CV_public;
    newElement.function.returnType1.baseType = 0;
    newElement.function.parentClassName = GetParentClassName(element);
    newElement.function.parameters.append(QString("%1&").arg(element->udt.name));
    newElement.elementType = ElementType::functionType;

    Element parameterElement = {};

    parameterElement.elementType = ElementType::dataType;
    parameterElement.data.name = "__that";
    parameterElement.data.typeName = QString("%1&").arg(element->udt.name);
    parameterElement.data.isReference = true;
    parameterElement.data.isTypeConst = true;
    parameterElement.data.dataKind = DataIsParam;

    newElement.dataChildren.append(parameterElement);

    static const FunctionOptions functionOptions;

    newElement.function.prototype = FunctionTypeToString(const_cast<Element*>(&newElement), &functionOptions);

    int lastConstructorIndex = -1;
    int nonVirtualFunctionChildrenCount = element->nonVirtualFunctionChildren.count();

    for (int i = 0; i < nonVirtualFunctionChildrenCount; i++)
    {
        if (element->nonVirtualFunctionChildren.at(i).function.isConstructor ||
            element->nonVirtualFunctionChildren.at(i).function.isDefaultConstructor)
        {
            lastConstructorIndex = i;
        }
        else if (lastConstructorIndex != -1)
        {
            break;
        }
    }

    if (lastConstructorIndex + 1 == nonVirtualFunctionChildrenCount)
    {
        element->nonVirtualFunctionChildren.append(newElement);
    }
    else
    {
        element->nonVirtualFunctionChildren.insert(lastConstructorIndex + 1, newElement);
    }
}

void PDB::AddCopyAssignmentOperator(Element* element)
{
    Element newElement = {};

    newElement.function.name = "operator=";
    newElement.function.callingConvention = CV_CALL_THISCALL;
    newElement.function.isGeneratedByApp = true;
    newElement.function.isCopyAssignmentOperator = true;
    newElement.function.access = CV_public;
    newElement.function.returnType1.baseType = 0;
    newElement.function.returnType2 = QString("%1&").arg(element->udt.name);
    newElement.function.parentClassName = GetParentClassName(element);
    newElement.function.parameters.append(QString("%1&").arg(element->udt.name));
    newElement.elementType = ElementType::functionType;

    Element parameterElement = {};

    parameterElement.elementType = ElementType::dataType;
    parameterElement.data.name = "__that";
    parameterElement.data.typeName = QString("%1&").arg(element->udt.name);
    parameterElement.data.isReference = true;
    parameterElement.data.isTypeConst = true;
    parameterElement.data.dataKind = DataIsParam;

    newElement.dataChildren.append(parameterElement);

    static const FunctionOptions functionOptions;

    newElement.function.prototype = FunctionTypeToString(const_cast<Element*>(&newElement), &functionOptions);

    int nonVirtualFunctionChildrenCount = element->nonVirtualFunctionChildren.count();

    for (int i = 0; i < nonVirtualFunctionChildrenCount; i++)
    {
        if (element->nonVirtualFunctionChildren.at(i).function.isCopyConstructor)
        {
            element->nonVirtualFunctionChildren.insert(i + 1, newElement);

            if (i + 1 == nonVirtualFunctionChildrenCount)
            {
                element->nonVirtualFunctionChildren.append(newElement);
            }
            else
            {
                element->nonVirtualFunctionChildren.insert(i + 1, newElement);
            }

            break;
        }
    }
}

void PDB::CheckIfCopyCtorAndCopyAssignmentOpAdded(Element* element)
{
    if (element->elementType == ElementType::enumType ||
        element->elementType == ElementType::udtType &&
        element->udt.isUnnamed)
    {
        return;
    }

    int indexOfLastConstructor = 0;

    int udtChildrenCount = element->udtChildren.count();

    for (int i = 0; i < udtChildrenCount; i++)
    {
        CheckIfDefaultCtorAndDtorAdded(&element->udtChildren[i]);
    }

    if (!element->udt.hasCopyConstructor)
    {
        AddCopyConstructor(element);
    }

    if (!element->udt.hasCopyAssignmentOperator)
    {
        AddCopyAssignmentOperator(element);
    }
}

void PDB::ApplyReturnValueOptimization(Element* element)
{
    QString returnType = element->function.returnType2;

    if (element->function.returnType1.baseType == 0 &&
        !element->function.returnType1.isPointer &&
        !element->function.returnType1.isReference)
    {
        element->function.returnType2.append("*");

        Element parameterElement = {};

        parameterElement.elementType = ElementType::dataType;
        parameterElement.data.name = "result";
        parameterElement.data.typeName = returnType;
        parameterElement.data.isPointer = true;
        parameterElement.data.pointerLevel = 1;
        parameterElement.data.dataKind = DataIsParam;

        if (element->dataChildren.count() == 0)
        {
            element->dataChildren.append(parameterElement);
        }
        else
        {
            if (element->dataChildren.at(0).data.dataKind == DataIsObjectPtr)
            {
                element->dataChildren.insert(1, parameterElement);
            }
            else
            {
                element->dataChildren.insert(0, parameterElement);
            }
        }

        element->function.parameters.append(QString("%1*").arg(returnType));
    }
}

void PDB::FlattenUDT(Element* element, Element* newElement, quint64* offset, quint32* alignmentNum)
{
    int count = element->children.count();
    int numberOfBits = 0;

    for (int i = 0; i < count; i++)
    {
        if (element->children.at(i).elementType == ElementType::baseClassType)
        {
            JoinLists(&element->children[i]);
            FlattenUDT(&element->children[i], newElement, offset, alignmentNum);
        }
        else if (element->children.at(i).elementType == ElementType::dataType &&
            element->children.at(i).data.dataKind != DataIsStaticMember)
        {
            bool increaseOffset = false;

            element->children[i].offset = *offset;

            if (element->children.at(i).numberOfBits > 0)
            {
                while (i < count &&
                    element->children.at(i).elementType == ElementType::dataType &&
                    element->children.at(i).numberOfBits > 0)
                {
                    element->children[i].offset = *offset;
                    newElement->children.append(element->children.at(i));

                    numberOfBits += element->children.at(i).numberOfBits;

                    if (numberOfBits >= element->children.at(i).size * 8)
                    {
                        numberOfBits = 0;
                        *offset += element->children.at(i).size;
                    }

                    i++;
                }

                i--;
                
                if (numberOfBits == 0)
                {
                    if (element->children.at(i).elementType == ElementType::dataType &&
                        element->children.at(i).numberOfBits > 0)
                    {
                        element->children[i].offset = *offset;
                        newElement->children.append(element->children.at(i));
                    }
                }
                else
                {
                    *offset += element->children.at(i).size;
                    numberOfBits = 0;
                }

                continue;
            }

            if (element->children.at(i).data.isTypeNameOfEnum)
            {
                newElement->children.append(element->children.at(i));
                increaseOffset = true;
            }
            else if (element->children.at(i).data.hasChildren &&
                element->children.at(i).udt.isAnonymousUnion)
            {
                JoinLists(&element->children[i]);
                FlattenUDT(&element->children[i], newElement, offset, alignmentNum);
            }
            else if (element->children.at(i).data.hasChildren &&
                element->children.at(i).udt.isAnonymousStruct)
            {
                JoinLists(&element->children[i]);
                FlattenUDT(&element->children[i], newElement, offset, alignmentNum);
            }
            else if (element->children.at(i).data.isCompilerGenerated)
            {
                if (!element->children.at(i).data.isVTablePointer)
                {
                    if (element->elementType == ElementType::udtType &&
                        element->children[i].data.isEndPadding)
                    {
                        element->children[i].data.name = QString("__endPadding%1").arg(*alignmentNum);
                    }
                    else
                    {
                        element->children[i].data.isEndPadding = false;
                        element->children[i].data.name = QString("__padding%1").arg(*alignmentNum);
                    }

                    ++*alignmentNum;
                }
                
                newElement->children.append(element->children.at(i));
                increaseOffset = true;
            }
            else if (element->children.at(i).data.isArray)
            {
                //In this case don't split it to base types because list get very long long if type has many members
                if (element->children.at(i).data.baseType == 0 &&
                    !element->children.at(i).data.isPointer)
                {
                    newElement->children.append(element->children.at(i));
                    increaseOffset = true;
                }
                else
                {
                    if (element->children.at(i).data.baseType != 0 ||
                        element->children.at(i).data.isPointer &&
                        element->children.at(i).data.baseType != 0)
                    {
                        int arrayCount = element->dataChildren.at(i).data.arrayCount.count();
                        int count2 = 1;

                        for (int j = 0; j < arrayCount; j++)
                        {
                            count2 *= element->dataChildren.at(i).data.arrayCount.at(j);
                        }

                        quint32 size = 0;

                        if (count2 > 0)
                        {
                            size = element->children.at(i).size / count2;
                        }

                        for (int j = 0; j < count2; j++)
                        {
                            Element newElement2 = {};

                            newElement2.elementType = ElementType::dataType;
                            newElement2.data.name = QString("%1_%2").arg(element->children.at(i).data.name).arg(j);
                            newElement2.data.typeName = element->children.at(i).data.typeName;
                            newElement2.data.name = QString("%1_%2").arg(element->children.at(i).data.name).arg(j);
                            newElement2.data.dataKind = DataIsMember;
                            newElement2.size = size;
                            newElement2.offset = *offset;

                            *offset += size;

                            newElement->children.append(newElement2);
                        }
                    }
                    else
                    {
                        int arrayCount = element->dataChildren.at(i).data.arrayCount.count();
                        int count2 = 1;

                        for (int j = 0; j < arrayCount; j++)
                        {
                            count2 *= element->dataChildren.at(i).data.arrayCount.at(j);
                        }

                        quint32 size = 0;

                        if (count2 > 0)
                        {
                            size = element->children.at(i).size / count2;
                        }

                        for (int j = 0; j < count2; j++)
                        {
                            Element newElement2 = {};

                            newElement2.elementType = ElementType::dataType;
                            newElement2.data.name = QString("%1_%2").arg(element->children.at(i).data.name).arg(j);
                            newElement2.data.typeName = element->children.at(i).data.typeName;
                            newElement2.data.name = QString("%1_%2").arg(element->children.at(i).data.name).arg(j);
                            newElement2.data.dataKind = DataIsMember;
                            newElement2.size = size;
                            newElement2.offset = *offset;

                            *offset += size;

                            newElement->children.append(newElement2);
                        }
                    }
                }
            }
            else
            {
                if (!element->children.at(i).data.isPointer &&
                    element->children.at(i).data.baseType == 0)
                {
                    QString typeName = element->children.at(i).data.originalTypeName;
                    QHash<QString, DWORD>::const_iterator it = diaSymbols->find(typeName);

                    DWORD id = it.value();
                    IDiaSymbol* symbol;

                    if (GetSymbolByID(id, &symbol))
                    {
                        Element element2 = GetElement(symbol);
                        symbol->Release();

                        if (element2.elementType == ElementType::udtType)
                        {
                            CheckIfUnionsAreMissing(&element2);
                        }

                        JoinLists(&element2);
                        FlattenUDT(&element2, newElement, offset, alignmentNum);
                    }
                }
                else
                {
                    newElement->children.append(element->children.at(i));
                    increaseOffset = true;
                }
            }

            if (increaseOffset)
            {
                *offset += element->children.at(i).size;
            }
        }

        //__m128
        if (element->elementType == ElementType::udtType &&
            element->udt.udtKind == UdtUnion ||
            element->elementType == ElementType::dataType &&
            element->data.hasChildren &&
            element->udt.isAnonymousUnion)
        {
            break;
        }
    }
}

quint32 PDB::CalculateDefaultAlignment(const Element* element, QHash<QString, int> checkedTypes)
{
    quint32 alignment = 1;

    if (element->elementType == ElementType::udtType &&
        element->udt.hasVTable)
    {
        if (type == CV_CFL_80386)
        {
            alignment = 4;
        }
        else
        {
            alignment = 8;
        }
    }

    quint32 size;
    int baseClassChildrenCount = element->baseClassChildren.count();
    int dataChildrenCount = element->dataChildren.count();

    for (int i = 0; i < baseClassChildrenCount; i++)
    {
        if (checkedTypes.find(element->baseClassChildren.at(i).baseClass.name) != checkedTypes.end())
        {
            continue;
        }

        QString name = element->baseClassChildren.at(i).baseClass.name;

        checkedTypes.insert(name, element->baseClassChildren.at(i).baseClass.name.length());

        quint32 alignment2 = CalculateDefaultAlignment(&element->baseClassChildren.at(i), checkedTypes);

        if (alignment2 > alignment)
        {
            alignment = alignment2;
        }
    }

    for (int i = 0; i < dataChildrenCount; i++)
    {
        if (element->dataChildren.at(i).data.isCompilerGenerated)
        {
            continue;
        }

        size = element->dataChildren.at(i).size;

        if (element->dataChildren.at(i).data.hasChildren)
        {
            if (element->dataChildren.at(i).data.isTypeNameOfEnum)
            {
                size = 4;

                if (size > alignment)
                {
                    alignment = size;
                }
            }
            else
            {
                Element element2 = element->dataChildren.at(i);
                element2.elementType = ElementType::udtType;

                quint32 alignment2 = CalculateDefaultAlignment(&element2, checkedTypes);

                if (alignment2 > alignment)
                {
                    alignment = alignment2;
                }
            }

            continue;
        }
        else
        {
            if (element->dataChildren.at(i).data.isTypeNameOfEnum)
            {
                size = 4;

                if (size > alignment)
                {
                    alignment = size;
                }

                continue;
            }

            if (element->dataChildren.at(i).data.isArray)
            {
                if (element->dataChildren.at(i).data.isPointer ||
                    element->dataChildren.at(i).data.baseType != 0)
                {
                    int arrayCount = element->dataChildren.at(i).data.arrayCount.count();
                    int count = 1;

                    for (int j = 0; j < arrayCount; j++)
                    {
                        count *= element->dataChildren.at(i).data.arrayCount.at(j);
                    }

                    if (count > 0)
                    {
                        size = element->dataChildren.at(i).size / count;
                    }
                }
                else
                {
                    if (checkedTypes.find(element->dataChildren.at(i).data.typeName) != checkedTypes.end())
                    {
                        continue;
                    }

                    QString typeName = element->dataChildren.at(i).data.originalTypeName;

                    checkedTypes.insert(typeName, typeName.length());

                    quint32 alignment2 = 0;

                    if (typeName.startsWith("m128") || typeName.startsWith("_m128") || typeName.startsWith("__m128"))
                    {
                        alignment2 = 16;
                    }
                    else
                    {
                        QHash<QString, DWORD>::const_iterator it = diaSymbols->find(typeName);
                        DWORD id = it.value();

                        SymbolRecord symbolRecord;

                        symbolRecord.id = id;
                        symbolRecord.typeName = typeName;

                        //It's only important to detect if type is enum because enums shouldn't be imported
                        if (element->dataChildren.at(i).data.isTypeNameOfEnum)
                        {
                            symbolRecord.type = SymbolType::enumType;
                        }

                        Element element2 = GetElement(&symbolRecord);

                        alignment2 = CalculateDefaultAlignment(&element2, checkedTypes);
                    }

                    if (alignment2 > alignment)
                    {
                        alignment = alignment2;
                    }

                    continue;
                }
            }

            if (!element->dataChildren.at(i).data.isPointer)
            {
                if ((element->dataChildren.at(i).data.typeName != element->udt.name) &&
                    element->dataChildren.at(i).data.baseType == 0)
                {
                    if (checkedTypes.find(element->dataChildren.at(i).data.typeName) != checkedTypes.end())
                    {
                        continue;
                    }

                    QString typeName = element->dataChildren.at(i).data.originalTypeName;

                    checkedTypes.insert(typeName, typeName.length());

                    quint32 alignment2 = 0;
                    QString lower = typeName.toLower();

                    if (lower.startsWith("m128") || lower.startsWith("_m128") || lower.startsWith("__m128"))
                    {
                        alignment2 = 16;
                    }
                    else
                    {
                        QHash<QString, DWORD>::const_iterator it = diaSymbols->find(typeName);
                        DWORD id = it.value();

                        SymbolRecord symbolRecord;

                        symbolRecord.id = id;
                        symbolRecord.typeName = typeName;

                        //It's only important to detect if type is enum because enums shouldn't be imported
                        if (element->dataChildren.at(i).data.isTypeNameOfEnum)
                        {
                            symbolRecord.type = SymbolType::enumType;
                        }

                        Element element2 = GetElement(&symbolRecord);

                        alignment2 = CalculateDefaultAlignment(&element2, checkedTypes);
                    }

                    if (alignment2 > alignment)
                    {
                        alignment = alignment2;
                    }

                    continue;
                }
            }
        }

        if (size > alignment)
        {
            alignment = size;
        }
    }

    return alignment;
}

quint32 PDB::GetCorrectAlignment(quint32 defaultAlignment, quint32 typeSize)
{
    while ((typeSize & (defaultAlignment - 1)) > 0)
    {
        defaultAlignment >>= 1;
    }

    return defaultAlignment;
}

unsigned int PDB::GetGreatestPaddingInUDT(const Element* element)
{
    unsigned int greatestPadding = 0;
    int dataChildrenCount = element->dataChildren.count();

    for (int i = 0; i < dataChildrenCount; i++)
    {
        if (element->dataChildren.at(i).data.isPadding &&
            element->dataChildren.at(i).size > greatestPadding)
        {
            greatestPadding = element->dataChildren.at(i).size;
        }
    }

    return greatestPadding;
}

unsigned int PDB::NextPowerOf2(unsigned int value)
{
	unsigned long index;

	_BitScanReverse(&index, --value);

    if (value == 0)
    {
        index = static_cast<unsigned long>(-1);
    }

    return 1U << (index + 1);
}

QString PDB::TrimStart(const QString& input)
{
    QString result = input;

    while (result.count() > 0 && result.at(0).isSpace())
    {
        result = result.right(result.count() - 1);
    }

    return result;
}

QString PDB::TrimEnd(const QString& input)
{
    QString result = input;

    while (result.count() > 0 && result.at(result.count() - 1).isSpace())
    {
        result = result.left(result.count() - 1);
    }

    return result;
}

QString PDB::FunctionTypeToString(const Element* element, const FunctionOptions* functionOptions)
{
    Function function = element->function;
    QString result;

    //Don't add keywords to prototypes while comparing prototypes because they will be compared faster then
    if (functionOptions->addKeywordsAndParameterNames)
    {
        if (function.hasInlSpec && options->addInlineKeywordToInlineFunctions)
        {
            result += "inline";
        }
        else if (function.isNaked && options->addDeclspecKeywords)
        {
            result += "__declspec(naked)";
        }
        else if (function.isNoInline && options->addDeclspecKeywords)
        {
            result += "__declspec(noinline)";
        }
        else if (function.isNoReturn && options->addDeclspecKeywords)
        {
            result += "__declspec(noreturn)";
        }

        //Don't add virtual and static keywords while generating C++ code
        if (functionOptions->addStaticAndVirtualKeywords)
        {
            if (function.isVirtual && !function.isOverridden)
            {
                result += "virtual";
            }
            else if (function.isStatic)
            {
                if (!(element->dataChildren.count() > 0 &&
                    element->dataChildren.at(0).data.dataKind == DataIsObjectPtr))
                {
                    result += "static";
                }
            }
        }

        if (function.isConstructor &&
            function.parameters.count() == 1 &&
            options->addExplicitKeyword)
        {
            result += "explicit";
        }
    }

    if (result.length() > 0)
    {
        result += " ";
    }

    if (!options->includeConstKeyword)
    {
        if (function.returnType2.contains(", const"))
        {
            function.returnType2.replace(" const", " ");
        }
        else if (function.returnType2.contains(" const"))
        {
            function.returnType2.replace(" const", "");
        }
        else if (function.returnType2.contains("const "))
        {
            function.returnType2.replace("const ", "");
        }
    }

    QString functionReturnType = function.returnType2;

    if (!function.isConstructor &&
        !function.isDefaultConstructor &&
        !function.isCopyConstructor &&
        !function.isDestructor &&
        !function.returnsFunctionPointer &&
        !function.isCastOperator)
    {
        if (functionOptions->addKeywordsAndParameterNames)
        {
            result += QString("%1 ").arg(functionReturnType);
        }
        else
        {
            result += QString("%1 ").arg(function.originalReturnType);
        }
    }

    if (function.returnsFunctionPointer)
    {
        if (options->displayWithTrailingReturnType || options->displayWithAuto)
        {
            result += "auto ";
        }
        else if (options->displayWithTypedef || options->displayWithUsing)
        {
            QString functionName = function.name;

            functionName[0] = functionName.at(0).toLower();

            QString functionPointerName = QString("%1Ptr").arg(functionName);
            QString declaration;
            Element newElement = {};

            result += QString("%1 ").arg(functionPointerName);

            newElement.elementType = ElementType::typedefType;
            newElement.typeDef.oldTypeName = functionReturnType;
            newElement.typeDef.newTypeName = functionPointerName;

            if (options->displayWithTypedef)
            {
                if (options->displayCallingConventionForFunctionPointers)
                {
                    functionPointerName.prepend(" ");
                }

                functionReturnType = functionReturnType.insert(functionReturnType.indexOf("*)(") + 1,
                    functionPointerName);

                declaration = QString("typedef %1").arg(functionReturnType);
            }
            else
            {
                declaration = QString("using %1 = %2").arg(functionPointerName).arg(functionReturnType);
            }

            newElement.typeDef.declaration = declaration;

            typedefChildren.append(newElement);
        }
    }

    if (functionOptions->addKeywordsAndParameterNames && options->displayCallingConventions)
    {
        //If function is variadic calling convention is __cdecl so there is no need display __cdecl
        if ((function.isStatic &&
            (function.callingConvention != CV_CALL_NEAR_C &&
                function.callingConvention != CV_CALL_FAR_C)) &&
            !function.isVariadic && function.callingConvention != CV_CALL_THISCALL)
        {
            result += QString("%1 ").arg(convertCallingConventionToString(function.callingConvention));
        }
    }

    if (options->modifyFunctionNames)
    {
        function.name = ModifyNamingCovention(function.name, false, true, false);
    }

    if (functionOptions->appendCurrentUDTName)
    {
        result += QString("%1::").arg(function.parentClassName);
    }
    else
    {
        if (options->removeScopeResolutionOperator && function.name.contains("::"))
        {
            if (function.parentClassName.length() == 0)
            {
                RemoveScopeResolutionOperators(function.name, parentClassName);
            }
            else
            {
                RemoveScopeResolutionOperators(function.name, function.parentClassName);
            }
        }
    }

    result += function.name;
    result += "(";

    int parametersCount = element->function.parameters.count();

    if (element->function.isVariadic)
    {
        parametersCount--;
    }

    if (!functionOptions->addKeywordsAndParameterNames)
    {
        for (int i = 0; i < parametersCount; i++)
        {
            //result += element->function.parameters.at(i);
            result += element->function.originalParameters.at(i);

            if (i != parametersCount - 1)
            {
                result += ", ";
            }
        }

        if (element->function.isVariadic)
        {
            if (parametersCount > 0)
            {
                result += ", ";
            }

            result += "...";
            //result += "va_list";
        }
    }
    else
    {
        int n = 0;
        int parameterNamesCount = element->dataChildren.count();

        if (functionOptions->displayThisPointer &&
            parameterNamesCount > 0 &&
            element->dataChildren.at(0).data.dataKind == DataIsObjectPtr)
		{
            static const DataOptions dataOptions = {};

            result += DataTypeToString(&element->dataChildren.at(0).data, &dataOptions);

            if (parametersCount > 0)
            {
                result += ", ";
            }
		}

        for (int i = 0; i < parametersCount; i++)
        {
            QString parameterType = element->function.parameters.at(i);
            QString parameterName;
            bool isParameterFunctionPointer = false;

            if (!options->includeConstKeyword)
            {
                if (parameterType.contains("<") && parameterType.contains(" const"))
                {
                    parameterType.replace(" const", "");
                }

                if (parameterType.startsWith("const "))
                {
                    parameterType.remove(0, 6);
                }
            }

            if (parameterType.contains("(*)"))
            {
                isParameterFunctionPointer = true;
            }
            else
            {
                result += parameterType;
            }

            if (parameterNamesCount > 0)
            {
                if (i == 0 && element->dataChildren.at(0).data.dataKind == DataIsObjectPtr)
                {
                    n++;
                }

                if (n < parameterNamesCount)
                {
                    parameterName = element->dataChildren.at(n++).data.name;
                }
                else
                {
                    parameterName = GenerateCustomParameterName(parameterType, i);
                }
            }
            else
            {
                parameterName = GenerateCustomParameterName(parameterType, i);
            }

            if (options->modifyVariableNames)
            {
                parameterName = ModifyNamingCovention(parameterName, false, false, true);
            }

            if (functionParameterNames.contains(parameterName))
            {
                parameterName += QString("%1").arg(i);
            }

            functionParameterNames.append(parameterName);

            if (isParameterFunctionPointer)
            {
                parameterType.insert(parameterType.indexOf("(*)") + 2, parameterName);

                result += parameterType;
            }
            else
            {
                result += QString(" %1").arg(parameterName);
            }

            if (i != parametersCount - 1)
            {
                result += ", ";
            }
        }

        if (element->function.isVariadic)
        {
            if (parametersCount > 0)
            {
                result += ", ";
            }

            if (options->useVAList)
            {
                result += "...";

                //result += "va_list argumentsList";
            }
            else if (options->useTemplateFunction)
            {
                result += "Args... args";
            }
        }

        functionParameterNames.clear();
    }

    result += ")";

    if (function.isConst && options->includeConstKeyword)
    {
        result += " const";
    }

    if (function.isVolatile && options->includeVolatileKeyword)
    {
        result += " volatile";
    }

    /*
    * Before checking whether it's pure virtual function 
    * check first whether it's virtual because static functions can be marked as pure also
    */
    if (functionOptions->addKeywordsAndParameterNames)
    {
        if (function.isVirtual && functionOptions->addStaticAndVirtualKeywords)
        {
            if (function.isPure)
            {
                result += " = 0";
            }
            else if (function.isOverridden)
            {
                result += " override";
            }
        }

        if (function.isDestructor && options->addNoexceptKeyword)
        {
            result += " noexcept";
        }

        if (functionOptions->addStaticAndVirtualKeywords &&
            !function.isPure &&
            (function.isDefaultConstructor ||
                function.isCopyConstructor ||
                function.isCopyAssignmentOperator ||
                function.isDestructor))
        {
            result += " = default";
        }
    }

    if (function.returnsFunctionPointer && options->displayWithTrailingReturnType)
    {
        result += QString(" -> %1").arg(functionReturnType);
    }

    return result;
}

QString PDB::GenerateCustomParameterName(QString parameterType, int i)
{
    QString result = "";

    if (parameterType.length() > 0)
    {
        if (parameterType.contains(", const"))
        {
            parameterType.replace(" const", " ");
        }
        else if (parameterType.contains(" const"))
        {
            parameterType.replace(" const", "");
        }
        else if (parameterType.contains("const "))
        {
            parameterType.replace("const ", "");
        }

        if (parameterType.contains("<"))
        {
            parameterType = parameterType.mid(0, parameterType.indexOf('<'));
        }

        if (parameterType.contains("::"))
        {
            parameterType = parameterType.mid(parameterType.lastIndexOf("::") + 2);
        }
    }

    if (parameterType.length() > 0 && parameterType.at(0).isUpper())
    {
        //Example: ENTITY_REF -> entityRef
        if (parameterType.contains("_"))
        {
            QStringList parts = parameterType.split('_', QString::SkipEmptyParts);

            parts[0] = parts[0].toLower();

            for (int i = 1; i < parts.size(); ++i)
            {
                QString part = parts.at(i);

                parts[i] = part.at(0);
                parts[i] += part.mid(1).toLower();
            }

            result = parts.join("");

            if (result.contains("*"))
            {
                result = result.mid(0, result.indexOf('*'));
            }
            else if (parameterType.contains("&"))
            {
                result = result.mid(0, result.indexOf('&'));
            }

            if (functionParameterNames.contains(result))
            {
                result += QString("%1").arg(i + 1);
            }
        }
        else
        {
            //Example: EntityRef -> entityRef
            for (int j = 0; j < parameterType.length(); j++)
            {
                if (j > 0 && parameterType.at(j).isLower() && parameterType.at(j - 1).isUpper())
                {
                    result += parameterType.at(j - 1).toLower();

                    if (parameterType.contains("*"))
                    {
                        result += parameterType.mid(j, parameterType.indexOf('*') - j);
                    }
                    else if (parameterType.contains("&"))
                    {
                        result += parameterType.mid(j, parameterType.indexOf('&') - j);
                    }
                    else
                    {
                        result += parameterType.mid(j);
                    }

                    if (functionParameterNames.contains(result))
                    {
                        result += QString("%1").arg(i + 1);
                    }

                    break;
                }
            }
        }

        /*if (functionParameterNames.contains(result))
        {
            result += QString("%1").arg(i + 1);
        }*/

        /*if (result.length() == 0)
        {
            result += QString("param%1").arg(i + 1);
        }*/

        if (result.length() == 0)
        {
            for (int j = 0; j < parameterType.length(); j++)
            {
                if (parameterType.at(j).isLetter())
                {
                    result += parameterType.at(j).toLower();
                }
            }

            if (functionParameterNames.contains(result))
            {
                result += QString("%1").arg(i + 1);
            }
        }
    }
    else
    {
        if (options->useGhidraNameStyle)
        {
            result += QString("param%1").arg(i + 1);
        }
        else
        {
            result += QString("p%1").arg(i + 1);
        }
    }

    return result;
}

QString PDB::GenerateCPPCode(const Element* element, int level)
{
    QString cppCode = "";
    int udtChildrenCount = element->udtChildren.count();
    int functionChildrenCount = element->virtualFunctionChildren.count() + element->nonVirtualFunctionChildren.count();
    QList<Element> children;

    children.append(element->virtualFunctionChildren);
    children.append(element->nonVirtualFunctionChildren);

    for (int i = 0; i < udtChildrenCount; i++)
    {
        cppCode += GenerateCPPCode(&element->udtChildren.at(i), level + 1);
    }

    for (int i = 0; i < functionChildrenCount; i++)
    {
        DWORD functionOffset = children.at(i).function.relativeVirtualAddress;

        if (functionOffset == 0 &&
            !options->displayNonImplementedFunctions &&
            /*!children.at(i).function.isGeneratedByApp &&*/ //default keyword is added in this case so compiler will implement it
            !children.at(i).function.isDefaultConstructor &&
            !children.at(i).function.isDestructor &&
            !(children.at(i).function.isCopyConstructor &&
                options->applyRuleOfThree) &&
            !(children.at(i).function.isCopyAssignmentOperator &&
                options->applyRuleOfThree))
        {
            continue;
        }

        if (children.at(i).function.isVariadic && options->useTemplateFunction)
        {
            continue;
        }

        QString udtName = children.at(i).function.parentClassName;

        if (children.at(i).function.isVirtual &&
            children.at(i).function.indexOfVTable > 0)
        {
            udtName = element->udt.vTableNames.value(children.at(i).function.indexOfVTable);
        }

        QList<QString> parameterTypes = children.at(i).function.parameters;
        QString functionName = children.at(i).function.name;

        int parameterTypesCount = parameterTypes.count();

        if (children.at(i).function.isVariadic)
        {
            parameterTypesCount--;
        }

        int parameterNamesCount = children.at(i).dataChildren.count();

        static const FunctionOptions functionOptions = { true, false, false, true };
        QString functionPrototype = FunctionTypeToString(const_cast<Element*>(&children.at(i)), &functionOptions);

		cppCode += QString("%1\r\n{\r\n\t").arg(functionPrototype);

		if (children.at(i).function.isVariadic)
		{
            if (children.at(i).function.isVirtual)
            {
                int vTableIndex = children.at(i).function.virtualBaseOffset >> 2;

                cppCode += QString("usigned int address = *reinterpret_cast<void***>(this))[%1]\r\n\t").arg(vTableIndex);
            }

            if (options->useVAList)
            {
                cppCode += "va_list argumentsList;\r\n\r\n\t";
                cppCode += QString("va_start(argumentsList, %1);\r\n\r\n\t").arg(children.at(i).dataChildren.last().data.name);
            }
		}

        if (children.at(i).function.isVirtual ||
            (children.at(i).function.isVirtual &&
                children.at(i).function.isPure &&
                functionOffset > 0))
        {
            if (children.at(i).function.isDestructor)
            {
                if (element->udt.isNested)
                {
                    if (options->implementDefaultConstructorAndDestructor && !options->implementMethodsOfInnerUDT)
                    {
                        continue;
                    }
                }
                else if (!options->implementDefaultConstructorAndDestructor)
                {
                    continue;
                }
            }

            int vTableIndex = children.at(i).function.virtualBaseOffset >> 2;

            /*
            * Virtual destructor implementation shouldn't call original destructor.
            * Instead of calling original destructor copy destructor's decompiled code from IDA or Ghidra
            */
            if (children.at(i).function.isDestructor)
            {
                hasVirtualDestructor = true;
                cppCode += QString("\r\n}\r\n\r\n");

                continue;
            }

            if (children.at(i).function.isGeneratedByApp)
            {
                cppCode += QString("\r\n}\r\n\r\n");

                continue;
            }

            if (children.at(i).function.returnType1.baseType == 1 &&
                !(children.at(i).function.returnType1.isPointer ||
                    children.at(i).function.returnType1.isReference))
            {
                if (children.at(i).function.isVariadic)
                {
					if (children.at(i).function.isConst)
					{
						cppCode += QString("Function::Call<const %1*").arg(udtName);
					}
					else
					{
						cppCode += QString("Function::Call<%1*").arg(udtName);
					}
                }
                else
                {
                    if (children.at(i).function.isConst)
                    {
                        cppCode += QString("Function::CallConstVirtualMethod<%1, const %2*").arg(vTableIndex).arg(udtName);
                    }
                    else
                    {
                        cppCode += QString("Function::CallVirtualMethod<%1, %2*").arg(vTableIndex).arg(udtName);
                    }
                }
            }
            else
            {
                QString returnType = children.at(i).function.returnType2;

                if (!options->includeConstKeyword)
                {
                    if (returnType.contains("<") && returnType.contains(" const"))
                    {
                        returnType.replace(" const", "");
                    }
                }

                if (children.at(i).function.isRVOApplied)
                {
                    if (children.at(i).function.isVariadic)
                    {
						if (children.at(i).function.isConst)
						{
                            cppCode += QString("return Function::CallRVOAndReturn<%1, const %2*").arg(returnType).arg(udtName);
						}
						else
						{
                            cppCode += QString("return Function::CallRVOAndReturn<%1, %2*").arg(returnType).arg(udtName);
						}
                    }
                    else
                    {
                        if (children.at(i).function.isConst)
                        {
                            cppCode += QString("return Function::CallRVOConstVirtualMethodAndReturn<%1, %2, const %3*").arg(returnType)
                                .arg(vTableIndex).arg(udtName);
                        }
                        else
                        {
                            cppCode += QString("return Function::CallRVOVirtualMethodAndReturn<%1, %2, %3*").arg(returnType)
                                .arg(vTableIndex).arg(udtName);
                        }
                    }
                }
                else
                {
					if (children.at(i).function.isVariadic)
					{
						if (children.at(i).function.isConst)
						{
                            cppCode += QString("return Function::CallAndReturn<%1, const %2*").arg(returnType).arg(udtName);
						}
						else
						{
                            cppCode += QString("return Function::CallAndReturn<%1, %2*").arg(returnType).arg(udtName);
						}
					}
                    else
                    {
                        if (children.at(i).function.isConst)
                        {
                            cppCode += QString("return Function::CallConstVirtualMethodAndReturn<%1, %2, const %3*").arg(returnType)
                                .arg(vTableIndex).arg(udtName);
                        }
                        else
                        {
                            cppCode += QString("return Function::CallVirtualMethodAndReturn<%1, %2, %3*").arg(returnType)
                                .arg(vTableIndex).arg(udtName);
                        }
                    }
                }
            }

            if (parameterTypesCount > 0)
            {
                cppCode += ", ";
            }
        }
        else
        {
            if (children.at(i).function.isDefaultConstructor || children.at(i).function.isDestructor)
            {
                if (element->udt.isNested)
                {
                    if (options->implementDefaultConstructorAndDestructor && !options->implementMethodsOfInnerUDT)
                    {
                        continue;
                    }
                }
                else if (!options->implementDefaultConstructorAndDestructor)
                {
                    continue;
                }
            }

            /*
            * Constructor implementation shouldn't call original constructor if class/struct has virtual destructor.
            * Instead of calling original constructor copy constructor's decompiled code from IDA or Ghidra
            */
            if (children.at(i).function.isDefaultConstructor && hasVirtualDestructor)
            {
                cppCode += QString("\r\n}\r\n\r\n");

                continue;
            }

            /*if ((children.at(i).function.isDefaultConstructor ||
                children.at(i).function.isConstructor) &&
                hasVirtualDestructor)
            {
                cppCode += QString("\r\n}\r\n\r\n");

                continue;
            }*/

            if (children.at(i).function.isGeneratedByApp)
            {
                cppCode += QString("\r\n}\r\n\r\n");

                continue;
            }

            if (functionOffset == 0 &&
                (children.at(i).function.isDefaultConstructor ||
                    children.at(i).function.isConstructor ||
                    children.at(i).function.isCopyConstructor ||
                    children.at(i).function.isCopyAssignmentOperator ||
                    children.at(i).function.isDestructor))
            {
                cppCode += QString("\r\n}\r\n\r\n");

                continue;
            }

            if (children.at(i).function.returnType1.baseType == 1 &&
                !(children.at(i).function.returnType1.isPointer ||
                    children.at(i).function.returnType1.isReference))
            {
                switch (children.at(i).function.callingConvention)
                {
                case CV_CALL_THISCALL:
                {
                    if (children.at(i).function.isConst)
                    {
                        cppCode += QString("Function::CallMethod<const %1*").arg(udtName);
                    }
                    else
                    {
                        cppCode += QString("Function::CallMethod<%1*").arg(udtName);
                    }

                    if (parameterTypesCount > 0)
                    {
                        cppCode += ", ";
                    }

                    break;
                }
                case CV_CALL_NEAR_FAST:
                case CV_CALL_FAR_FAST:
                    cppCode += QString("Function::FastCall<");

                    break;
                case CV_CALL_NEAR_STD:
                case CV_CALL_FAR_STD:
                    cppCode += QString("Function::StdCall<");

                    break;
                case CV_CALL_NEAR_C:
                case CV_CALL_FAR_C:
                    cppCode += QString("Function::Call<");

                    if (children.at(i).dataChildren.count() > 0 &&
                        children.at(i).dataChildren.at(0).data.dataKind == DataIsObjectPtr)
					{
                        if (children.at(i).function.isConst)
                        {
                            cppCode += QString("const %1*").arg(children.at(i).dataChildren.at(0).data.typeName);
                        }
                        else
                        {
                            cppCode += QString("%1*").arg(children.at(i).dataChildren.at(0).data.typeName);
                        }

						if (parameterTypesCount > 0)
						{
							cppCode += ", ";
						}
					}

                    break;
                case CV_CALL_NEAR_VECTOR:
                    cppCode += QString("Function::VectorCall<");

                    break;
                }
            }
            else
            {
                QString returnType = children.at(i).function.returnType2;

                if (!options->includeConstKeyword)
                {
                    if (returnType.contains("<") && returnType.contains(" const"))
                    {
                        returnType.replace(" const", "");
                    }
                }

                switch (children.at(i).function.callingConvention)
                {
                case CV_CALL_THISCALL:
                {
                    if (children.at(i).function.isRVOApplied)
                    {
						if (children.at(i).function.isConst)
						{
							cppCode += QString("return Function::CallRVOMethodAndReturn<%1, const %2*").arg(returnType).arg(udtName);
						}
						else
						{
							cppCode += QString("return Function::CallRVOMethodAndReturn<%1, %2*").arg(returnType).arg(udtName);
						}
                    }
                    else
                    {
                        if (children.at(i).function.isConst)
                        {
                            cppCode += QString("return Function::CallMethodAndReturn<%1, const %2*").arg(returnType).arg(udtName);
                        }
                        else
                        {
                            cppCode += QString("return Function::CallMethodAndReturn<%1, %2*").arg(returnType).arg(udtName);
                        }
                    }

                    break;
                }
                case CV_CALL_NEAR_FAST:
                case CV_CALL_FAR_FAST:
                    if (children.at(i).function.isRVOApplied)
                    {
                        cppCode += QString("return Function::FastCallRVOAndReturn<%1").arg(returnType);
                    }
                    else
                    {
                        cppCode += QString("return Function::FastCallAndReturn<%1").arg(returnType);
                    }

                    break;
                case CV_CALL_NEAR_STD:
                case CV_CALL_FAR_STD:
                    if (children.at(i).function.isRVOApplied)
                    {
                        cppCode += QString("return Function::StdCallRVOAndReturn<%1").arg(returnType);
                    }
                    else
                    {
                        cppCode += QString("return Function::StdCallAndReturn<%1").arg(returnType);
                    }

                    break;
                case CV_CALL_NEAR_C:
                case CV_CALL_FAR_C:
                    if (children.at(i).function.isRVOApplied)
                    {
                        cppCode += QString("return Function::CallRVOAndReturn<%1").arg(returnType);
                    }
                    else
                    {
                        cppCode += QString("return Function::CallAndReturn<%1").arg(returnType);
                    }

                    if (children.at(i).dataChildren.count() > 0 &&
                        children.at(i).dataChildren.at(0).data.dataKind == DataIsObjectPtr)
                    {
                        if (children.at(i).function.isConst)
                        {
                            cppCode += QString(", const %1*").arg(children.at(i).dataChildren.at(0).data.typeName);
                        }
                        else
                        {
                            cppCode += QString(", %1*").arg(children.at(i).dataChildren.at(0).data.typeName);
                        }
                    }

                    break;
                case CV_CALL_NEAR_VECTOR:
                    if (children.at(i).function.isRVOApplied)
                    {
                        cppCode += QString("return Function::VectorCallRVOAndReturn<%1");
                    }
                    else
                    {
                        cppCode += QString("return Function::VectorCallAndReturn<%1");
                    }

                    break;
                }

                if (parameterTypesCount > 0)
                {
                    cppCode += ", ";
                }
            }

            /*if (parameterTypesCount > 0)
            {
                cppCode += ", ";
            }*/
        }

        for (int j = 0; j < parameterTypesCount; j++)
        {
            static const DataOptions dataOptions = { true, false };
            QString parameterType = parameterTypes.at(j);

            if (!options->includeConstKeyword)
            {
                if (parameterType.contains("<") && parameterType.contains(" const"))
                {
                    parameterType.replace(" const", "");
                }

                if (parameterType.startsWith("const "))
                {
                    parameterType.remove(0, 6);
                }
            }

            cppCode += parameterType;

            if (j != parameterTypesCount - 1)
            {
                cppCode += ", ";
            }
        }

        if (children.at(i).function.isVariadic)
        {
            if (parameterTypesCount > 0)
            {
                cppCode += ", ";
            }

            if (options->useVAList)
            {
                cppCode += "va_list";
            }
            else
            {
                cppCode += "Args...";
            }
        }

        cppCode += ">(";

        if (children.at(i).function.isVirtual)
        {
			if (children.at(i).function.isVariadic)
			{
                cppCode += "address";

				if (parameterNamesCount > 0)
				{
					cppCode += ", ";
				}
			}
        }
        else
        {
            QString baseAddressVariableName = GetBaseAddressVariableName();

			DWORD functionOffset = children.at(i).function.relativeVirtualAddress;
			QString offset = QString::number(functionOffset, 16).toUpper();

			cppCode += QString("BaseAddresses::%1 + 0x%2").arg(baseAddressVariableName).arg(offset);

			if (parameterNamesCount > 0)
			{
				cppCode += ", ";
			}
        }

        QString parameterType = "";

        if (parameterNamesCount > 0 && children.at(i).dataChildren.at(0).data.dataKind == DataIsObjectPtr)
        {
            if (children.at(i).function.isVirtual &&
                children.at(i).function.indexOfVTable > 0)
            {
                if (children.at(i).function.isConst)
                {
                    cppCode += QString("static_cast<const %1*>(this)").arg(udtName);
                }
                else
                {
                    cppCode += QString("static_cast<%1*>(this)").arg(udtName);
                }
            }
            else
            {
                cppCode += "this";
            }

            if (parameterNamesCount > 1)
            {
                cppCode += ", ";
            }
        }

        for (int j = 0; j < parameterNamesCount; j++)
        {
            if (j == 0 && children.at(i).dataChildren.at(0).data.dataKind == DataIsObjectPtr)
            {
                continue;
            }

            QString parameterName = children.at(i).dataChildren.at(j).data.name;

            if (cppCode.contains("(*)"))
            {
                cppCode = cppCode.replace("(*)", QString("(*%1)").arg(parameterName));
            }
            else
            {
                cppCode += parameterName;
            }

            if (j != parameterNamesCount - 1)
            {
                cppCode += ", ";
            }
        }

        if (children.at(i).function.isVariadic)
        {
            //cppCode += "...";

            if (options->useVAList)
            {
                cppCode += ", argumentsList";
            }
            else
            {
                cppCode += "args...";
            }
        }

        if (children.at(i).function.isVariadic && options->useVAList)
        {
            cppCode += ");\r\n\r\n\t";
            cppCode += "va_end(argumentsList);";
            cppCode += "\r\n}\r\n\r\n";
        }
        else
        {
            cppCode += ");\r\n}\r\n\r\n";
        }
        
        functionParameterNames.clear();
    }

    ImplementFunctionsForStaticVariables(element, cppCode);

    if (level == 0)
    {
        cppCode.remove(cppCode.length() - 2, 2);
    }

    return cppCode;
}

QString PDB::ImplementVariadicTemplateFunction(const Element* element, int level)
{
    QString cppCode = "";
    QString udtName = element->function.parentClassName;

    if (element->function.isVirtual &&
        element->function.indexOfVTable > 0)
    {
        udtName = element->function.vTableName;
    }

    QList<QString> parameterTypes = element->function.parameters;
    QString functionName = element->function.name;

    int parameterTypesCount = parameterTypes.count();
    int parameterNamesCount = element->dataChildren.count();

    parameterTypesCount--;

    static const FunctionOptions functionOptions = { true, false, false, true };
    QString functionPrototype = FunctionTypeToString(element, &functionOptions);

    cppCode += QString("\r\n%1template <typename... Args>\r\n").arg(GetTab(level));
    cppCode += QString("%1%2\r\n%3{\r\n\t").arg(GetTab(level)).arg(functionPrototype).arg(GetTab(level));

	if (element->function.isVirtual)
	{
		int vTableIndex = element->function.virtualBaseOffset >> 2;

        cppCode += QString("%1usigned int address = *reinterpret_cast<void***>(this))[%2]\r\n\t").arg(GetTab(level))
            .arg(vTableIndex);
	}

    cppCode += GetTab(level);

    DWORD functionOffset = element->function.relativeVirtualAddress;

    if (element->function.isVirtual ||
        (element->function.isVirtual &&
            element->function.isPure &&
            functionOffset > 0))
    {
        int vTableIndex = element->function.virtualBaseOffset >> 2;

        if (element->function.returnType1.baseType == 1 &&
            !(element->function.returnType1.isPointer ||
                element->function.returnType1.isReference))
        {
			if (element->function.isConst)
			{
				cppCode += QString("Function::Call<const %1*").arg(udtName);
			}
			else
			{
				cppCode += QString("Function::Call<%1*").arg(udtName);
			}
        }
        else
        {
            QString returnType = element->function.returnType2;

            if (!options->includeConstKeyword)
            {
                if (returnType.contains("<") && returnType.contains(" const"))
                {
                    returnType.replace(" const", "");
                }
            }

            if (element->function.isRVOApplied)
            {
				if (element->function.isConst)
				{
					cppCode += QString("return Function::CallRVOAndReturn<%1, const %2*").arg(returnType).arg(udtName);
				}
				else
				{
					cppCode += QString("return Function::CallRVOAndReturn<%1, %2*").arg(returnType).arg(udtName);
				}
            }
            else
            {
				if (element->function.isConst)
				{
					cppCode += QString("return Function::CallAndReturn<%1, const %2*").arg(returnType).arg(udtName);
				}
				else
				{
					cppCode += QString("return Function::CallAndReturn<%1, %2*").arg(returnType).arg(udtName);
				}
            }
        }

        if (parameterTypesCount > 0)
        {
            cppCode += ", ";
        }
    }
    else
    {
        if (element->function.returnType1.baseType == 1 &&
            !(element->function.returnType1.isPointer ||
                element->function.returnType1.isReference))
        {
            switch (element->function.callingConvention)
            {
            case CV_CALL_THISCALL:
            {
                if (element->function.isConst)
                {
                    cppCode += QString("Function::CallMethod<const %1*").arg(udtName);
                }
                else
                {
                    cppCode += QString("Function::CallMethod<%1*").arg(udtName);
                }

                if (parameterTypesCount > 0)
                {
                    cppCode += ", ";
                }

                break;
            }
            case CV_CALL_NEAR_FAST:
            case CV_CALL_FAR_FAST:
                cppCode += QString("Function::FastCall<");

                break;
            case CV_CALL_NEAR_STD:
            case CV_CALL_FAR_STD:
                cppCode += QString("Function::StdCall<");

                break;
            case CV_CALL_NEAR_C:
            case CV_CALL_FAR_C:
                cppCode += QString("Function::Call<");

                if (element->dataChildren.count() > 0 &&
                    element->dataChildren.at(0).data.dataKind == DataIsObjectPtr)
                {
                    if (element->function.isConst)
                    {
                        cppCode += QString("const %1*").arg(element->dataChildren.at(0).data.typeName);
                    }
                    else
                    {
                        cppCode += QString("%1*").arg(element->dataChildren.at(0).data.typeName);
                    }

                    if (parameterTypesCount > 0)
                    {
                        cppCode += ", ";
                    }
                }

                break;
            case CV_CALL_NEAR_VECTOR:
                cppCode += QString("Function::VectorCall<");

                break;
            }
        }
        else
        {
            QString returnType = element->function.returnType2;

            if (!options->includeConstKeyword)
            {
                if (returnType.contains("<") && returnType.contains(" const"))
                {
                    returnType.replace(" const", "");
                }
            }

            switch (element->function.callingConvention)
            {
            case CV_CALL_THISCALL:
            {
                if (element->function.isRVOApplied)
                {
                    if (element->function.isConst)
                    {
                        cppCode += QString("return Function::CallRVOMethodAndReturn<%1, const %2*").arg(returnType).arg(udtName);
                    }
                    else
                    {
                        cppCode += QString("return Function::CallRVOMethodAndReturn<%1, %2*").arg(returnType).arg(udtName);
                    }
                }
                else
                {
                    if (element->function.isConst)
                    {
                        cppCode += QString("return Function::CallMethodAndReturn<%1, const %2*").arg(returnType).arg(udtName);
                    }
                    else
                    {
                        cppCode += QString("return Function::CallMethodAndReturn<%1, %2*").arg(returnType).arg(udtName);
                    }
                }

                break;
            }
            case CV_CALL_NEAR_FAST:
            case CV_CALL_FAR_FAST:
                if (element->function.isRVOApplied)
                {
                    cppCode += QString("return Function::FastCallRVOAndReturn<%1").arg(returnType);
                }
                else
                {
                    cppCode += QString("return Function::FastCallAndReturn<%1").arg(returnType);
                }

                break;
            case CV_CALL_NEAR_STD:
            case CV_CALL_FAR_STD:
                if (element->function.isRVOApplied)
                {
                    cppCode += QString("return Function::StdCallRVOAndReturn<%1").arg(returnType);
                }
                else
                {
                    cppCode += QString("return Function::StdCallAndReturn<%1").arg(returnType);
                }

                break;
            case CV_CALL_NEAR_C:
            case CV_CALL_FAR_C:
                if (element->function.isRVOApplied)
                {
                    cppCode += QString("return Function::CallRVOAndReturn<%1").arg(returnType);
                }
                else
                {
                    cppCode += QString("return Function::CallAndReturn<%1").arg(returnType);
                }

                if (element->dataChildren.count() > 0 &&
                    element->dataChildren.at(0).data.dataKind == DataIsObjectPtr)
                {
                    if (element->function.isConst)
                    {
                        cppCode += QString(", const %1*").arg(element->dataChildren.at(0).data.typeName);
                    }
                    else
                    {
                        cppCode += QString(", %1*").arg(element->dataChildren.at(0).data.typeName);
                    }
                }

                break;
            case CV_CALL_NEAR_VECTOR:
                if (element->function.isRVOApplied)
                {
                    cppCode += QString("return Function::VectorCallRVOAndReturn<%1");
                }
                else
                {
                    cppCode += QString("return Function::VectorCallAndReturn<%1");
                }

                break;
            }

            if (parameterTypesCount > 0)
            {
                cppCode += ", ";
            }
        }

        /*if (parameterTypesCount > 0)
        {
            cppCode += ", ";
        }*/
    }

    for (int j = 0; j < parameterTypesCount; j++)
    {
        static const DataOptions dataOptions = { true, false };
        QString parameterType = parameterTypes.at(j);

        if (!options->includeConstKeyword)
        {
            if (parameterType.contains("<") && parameterType.contains(" const"))
            {
                parameterType.replace(" const", "");
            }

            if (parameterType.startsWith("const "))
            {
                parameterType.remove(0, 6);
            }
        }

        cppCode += parameterType;

        if (j != parameterTypesCount - 1)
        {
            cppCode += ", ";
        }
    }

	if (parameterTypesCount > 0)
	{
		cppCode += ", ";
	}

    cppCode += "Args...>(";

    if (element->function.isVirtual)
    {
		cppCode += "address";

		if (parameterNamesCount > 0)
		{
			cppCode += ", ";
		}
    }
    else
    {
        QString baseAddressVariableName = GetBaseAddressVariableName();

        DWORD functionOffset = element->function.relativeVirtualAddress;
        QString offset = QString::number(functionOffset, 16).toUpper();

        cppCode += QString("BaseAddresses::%1 + 0x%2").arg(baseAddressVariableName).arg(offset);

        if (parameterNamesCount > 0)
        {
            cppCode += ", ";
        }
    }

    QString parameterType = "";

    if (parameterNamesCount > 0 && element->dataChildren.at(0).data.dataKind == DataIsObjectPtr)
    {
        if (element->function.isVirtual &&
            element->function.indexOfVTable > 0)
        {
            if (element->function.isConst)
            {
                cppCode += QString("static_cast<const %1*>(this)").arg(udtName);
            }
            else
            {
                cppCode += QString("static_cast<%1*>(this)").arg(udtName);
            }
        }
        else
        {
            cppCode += "this";
        }

        if (parameterNamesCount > 1)
        {
            cppCode += ", ";
        }
    }

    for (int j = 0; j < parameterNamesCount; j++)
    {
        if (j == 0 && element->dataChildren.at(0).data.dataKind == DataIsObjectPtr)
        {
            continue;
        }

        QString parameterName = element->dataChildren.at(j).data.name;

        if (cppCode.contains("(*)"))
        {
            cppCode = cppCode.replace("(*)", QString("(*%1)").arg(parameterName));
        }
        else
        {
            cppCode += parameterName;
        }

        if (j != parameterNamesCount - 1)
        {
            cppCode += ", ";
        }
    }

    cppCode += QString("args...);\r\n%1}\r\n").arg(GetTab(level));

    functionParameterNames.clear();

    return cppCode;
}

QString PDB::GetBaseAddressVariableName()
{
    QString baseAddressVariableName = fileNameWithoutExtension;
    QStringList parts = baseAddressVariableName.split('.', QString::SkipEmptyParts);
    int count = parts.count();

    for (int j = 1; j < count; j++)
    {
        parts[j].replace(0, 1, parts[j][0].toUpper());
    }

    return parts.join("");
}

void PDB::SetFileNameWithoutExtension(const QString& fileNameWithoutExtension)
{
    this->fileNameWithoutExtension = fileNameWithoutExtension;
}

void PDB::SetWindowTitle(const QString& windowTitle)
{
    this->windowTitle = windowTitle;
}

void PDB::SetFilePath(const QString& filePath)
{
    this->filePath = filePath;
}

void PDB::SetProcessType(ProcessType processType)
{
    this->processType = processType;
}

void PDB::ExportSymbol(SymbolRecord symbolRecord)
{
    QString name = symbolRecord.typeName;

    if (name.contains("<"))
    {
        name = name.mid(0, name.indexOf("<"));
    }

    if (name.contains("::"))
    {
        name = name.mid(0, name.indexOf("::"));
    }

    if (processType == ProcessType::exportUDTsAndEnums && (options->generateOnlyHeader || options->generateBoth))
    {
        SendStatusMessage(QString("Generating: %1.h").arg(name));
    }
    else if ((processType == ProcessType::exportUDTsAndEnumsWithDependencies ||
        processType == ProcessType::exportAllUDTsAndEnums) &&
        (options->generateOnlyHeader || options->generateBoth))
    {
        SendStatusMessageToProcessDialog(QString("Generating: %1.h").arg(name));
    }

    Element element = GetElement(&symbolRecord);
    QString elementInfo = GetElementInfo(&element);

    QFileInfo fileInfo(filePath);
    QString currentDirectory = fileInfo.absolutePath();

    if (!QDir(QString("%1/include").arg(currentDirectory)).exists())
    {
        QDir().mkdir(QString("%1/include").arg(currentDirectory));
    }

    QFile headerFile(QString("%1/include/%2.h").arg(currentDirectory).arg(name));

    if (!headerFile.exists() && headerFile.open(QIODevice::ReadWrite))
    {
        headerFile.write(elementInfo.toLatin1().data(), elementInfo.length());
        headerFile.close();
    }

    if (options->generateOnlySource || options->generateBoth)
    {
        if (processType == ProcessType::exportUDTsAndEnums && (options->generateOnlyHeader || options->generateBoth))
        {
            SendStatusMessage(QString("Generating: %1.cpp").arg(symbolRecord.typeName));
        }
        else if (processType == ProcessType::exportAllUDTsAndEnums && (options->generateOnlyHeader || options->generateBoth))
        {
            SendStatusMessageToProcessDialog(QString("Generating: %1.cpp").arg(symbolRecord.typeName));
        }

        QString cppCode = GenerateCPPCode(&element);

        cppCode.prepend("#include \"BaseAddresses.h\"\r\n\r\n");
        cppCode.prepend("#include \"Function.h\"\r\n");
        cppCode.prepend(QString("#include \"%1.h\"\r\n").arg(name));

        if (!QDir(QString("%1/src").arg(currentDirectory)).exists())
        {
            QDir().mkdir(QString("%1/src").arg(currentDirectory));
        }

        QFile cppFile(QString("%1/src/%2.cpp").arg(currentDirectory).arg(name));

        if (!cppFile.exists() && cppFile.open(QIODevice::WriteOnly))
        {
            cppFile.write(cppCode.toLatin1().data(), cppCode.length());
            cppFile.close();
        }
    }
}

void PDB::GetDependencies(const Element* element)
{
    int count = element->children.count();

    for (int i = 0; i < count && processEnabled; i++)
    {
        if (element->children.at(i).elementType == ElementType::baseClassType)
        {
            SymbolRecord symbolRecord;
            QHash<QString, DWORD>::const_iterator it = diaSymbols->find(element->children.at(i).baseClass.name);

            if (it == diaSymbols->end())
            {
                continue;
            }

            symbolRecord.id = it.value();
            symbolRecord.typeName = element->children.at(i).baseClass.name;

            //It's only important to detect if type is enum because enums shouldn't be imported
            if (element->dataChildren.at(i).data.isTypeNameOfEnum)
            {
                symbolRecord.type = SymbolType::enumType;
            }

            QHash<int, SymbolRecord>::const_iterator it2 = dependencies.find(symbolRecord.id);

            if (it2 != dependencies.end())
            {
                continue;
            }

            dependencies.insert(symbolRecord.id, symbolRecord);
            GetDependencies(&element->children.at(i));
        }
        else if (element->children.at(i).elementType == ElementType::dataType)
        {
            if (element->dataChildren.at(i).data.hasChildren && !element->dataChildren.at(i).data.isTypeNameOfEnum)
            {
                Element element2 = element->dataChildren.at(i);
                element2.elementType = ElementType::udtType;

                GetDependencies(&element2);
            }
            else if (element->children.at(i).data.baseType == 0)
            {
                SymbolRecord symbolRecord;
                QString typeName = element->children.at(i).data.typeName;

                if (!CheckIfNameOfMainOrInnerUDT(typeName, element->children.at(i).data.parentClassName))
                {
                    QHash<QString, DWORD>::const_iterator it = diaSymbols->find(element->children.at(i).data.originalTypeName);

                    if (it == diaSymbols->end())
                    {
                        continue;
                    }

                    symbolRecord.id = it.value();
                    symbolRecord.typeName = typeName;

                    if (element->dataChildren.at(i).data.isTypeNameOfEnum)
                    {
                        symbolRecord.type = SymbolType::enumType;
                    }

                    QHash<int, SymbolRecord>::const_iterator it2 = dependencies.find(symbolRecord.id);

                    if (it2 != dependencies.end())
                    {
                        continue;
                    }

                    dependencies.insert(symbolRecord.id, symbolRecord);

                    Element element2 = GetElement(&symbolRecord);

                    GetDependencies(&element2);
                }
            }
        }
        else if (element->children.at(i).elementType == ElementType::functionType)
        {
            IDiaSymbol* symbol;
            SymbolRecord symbolRecord;
            int parametersCount = element->children.at(i).function.parameters.count();

            for (int j = 0; j < parametersCount; j++)
            {
                QString parameterType = element->children.at(i).function.parameters.at(j);

                FormatString(parameterType);

                if (!CheckIfNameOfMainOrInnerUDT(parameterType, element->children.at(i).function.parentClassName))
                {
                    QHash<QString, DWORD>::const_iterator it = diaSymbols->find(parameterType);

                    if (it == diaSymbols->end())
                    {
                        continue;
                    }

                    symbolRecord.id = it.value();
                    symbolRecord.typeName = parameterType;

                    if (element->dataChildren.at(i).data.isTypeNameOfEnum)
                    {
                        symbolRecord.type = SymbolType::enumType;
                    }

                    QHash<int, SymbolRecord>::const_iterator it2 = dependencies.find(symbolRecord.id);

                    if (it2 != dependencies.end())
                    {
                        continue;
                    }

                    dependencies.insert(symbolRecord.id, symbolRecord);

                    Element element2 = GetElement(&symbolRecord);

                    GetDependencies(&element2);
                }
            }

            if (element->children.at(i).function.returnType1.baseType == 0)
            {
                QString returnType = element->children.at(i).function.returnType2;

                FormatString(returnType);

                if (!CheckIfNameOfMainOrInnerUDT(returnType, element->children.at(i).function.parentClassName))
                {
                    QHash<QString, DWORD>::const_iterator it = diaSymbols->find(returnType);

                    if (it == diaSymbols->end())
                    {
                        continue;
                    }

                    symbolRecord.id = it.value();
                    symbolRecord.typeName = returnType;

                    if (element->dataChildren.at(i).data.isTypeNameOfEnum)
                    {
                        symbolRecord.type = SymbolType::enumType;
                    }

                    QHash<int, SymbolRecord>::const_iterator it2 = dependencies.find(symbolRecord.id);

                    if (it2 != dependencies.end())
                    {
                        continue;
                    }

                    dependencies.insert(symbolRecord.id, symbolRecord);

                    Element element2 = GetElement(&symbolRecord);

                    GetDependencies(&element2);
                }
            }
        }
        else if (element->children.at(i).elementType == ElementType::udtType)
        {
            GetDependencies(&element->children.at(i));
        }
    }
}

void PDB::ExportSymbolWithDependencies(SymbolRecord* symbolRecord)
{
    processEnabled = true;

    ExportSymbol(*symbolRecord);

    Element element = GetElement(symbolRecord);

    SendStatusMessageToProcessDialog("Getting dependencies...");

    GetDependencies(&element);

    int count = dependencies.count();

    emit SetProgressMinimum(0);
    emit SetProgressMaximum(count);

    int currentIndex = 0;
    int currentProcent = 0;
    int procent = count / 1000;

    int i = 0;
    QList<SymbolRecord> dependencies = this->dependencies.values();

    while (i < count && processEnabled)
    {
        ExportSymbol(dependencies.at(i));

        if (currentIndex > currentProcent * procent)
        {
            currentProcent++;
            emit SetProgressValue(currentIndex);
        }

        currentIndex++;
        i++;
    }

    emit Completed();
}

void PDB::ExportAllSymbols()
{
    processEnabled = true;

    int count = symbolRecords->size();

    emit SetProgressMinimum(0);
    emit SetProgressMaximum(count);

    int currentIndex = 0;
    int currentProcent = 0;
    int procent = count / 1000;

    int i = 0;

    while (i < count && processEnabled)
    {
        ExportSymbol(symbolRecords->at(i));

        if (currentIndex > currentProcent * procent)
        {
            currentProcent++;
            emit SetProgressValue(currentIndex);
        }

        currentIndex++;
        i++;
    }

    emit Completed();
}

void PDB::FormatString(QString& string)
{
    if (string.contains("<"))
    {
        string.replace("class ", "");
        string.replace("struct ", "");
        string.replace("union ", "");
        string.replace("enum ", "");
    }

    string.replace(" >", ">");
    string.replace(" ,", ", ");

    if (!string.contains(", ")) //There are cases when template already has white space after ','
    {
        string.replace(",", ", ");
    }

    string.replace(" *", "*");
    string.replace(" &", "&");
}

void PDB::GetVariables()
{
    if (!global)
    {
        return;
    }

    processEnabled = true;

    this->functions->clear();

    IDiaEnumSymbols* dataSymbols;
    LONG count;

    if (global->findChildren(SymTagData, nullptr, nsNone, &dataSymbols) != S_OK)
    {
        return;
    }

    if (dataSymbols->get_Count(&count) != S_OK || !count)
    {
        return;
    }

    emit SetProgressMinimum(0);
    emit SetProgressMaximum(count);

    int currentIndex = 0;
    int currentProcent = 0;
    int procent = count / 100;

    GetSymbolsFromTable(dataSymbols, &currentIndex, &currentProcent, &procent);

    dataSymbols->Release();

    emit Completed();
}

void PDB::GetFunctions()
{
    if (!global)
    {
        return;
    }

    processEnabled = true;

    this->functions->clear();

    IDiaEnumSymbols* functionSymbols;
    LONG count;

    if (global->findChildren(SymTagFunction, nullptr, nsNone, &functionSymbols) != S_OK)
    {
        return;
    }

    if (functionSymbols->get_Count(&count) != S_OK || !count)
    {
        return;
    }

    emit SetProgressMinimum(0);
    emit SetProgressMaximum(count);

    int currentIndex = 0;
    int currentProcent = 0;
    int procent = count / 100;

    GetSymbolsFromTable(functionSymbols, &currentIndex, &currentProcent, &procent);

    functionSymbols->Release();

    emit Completed();
}

void PDB::GetPublicSymbols()
{
    if (!global)
    {
        return;
    }

    processEnabled = true;

    this->publicSymbols->clear();

    IDiaEnumSymbols* publicSymbols;
    LONG count;

    if (global->findChildren(SymTagPublicSymbol, nullptr, nsNone, &publicSymbols) != S_OK)
    {
        return;
    }

    if (publicSymbols->get_Count(&count) != S_OK || !count)
    {
        return;
    }

    emit SetProgressMinimum(0);
    emit SetProgressMaximum(count);

    int currentIndex = 0;
    int currentProcent = 0;
    int procent = count / 100;

    GetSymbolsFromTable(publicSymbols, &currentIndex, &currentProcent, &procent);

    publicSymbols->Release();

    emit Completed();
}

QString PDB::ModifyNamingCovention(const QString& name, bool isEnum, bool isFunction, bool isVariable)
{
    if (name.length() == 0)
    {
        return "";
    }

    QString result;

    if (options->removeHungaryNotationFromUDTAndEnums && !isFunction && !isVariable)
    {
        bool result2 = true;

        if (isEnum)
        {
            result2 = CheckIfEnumHasHungaryNotation(name);
        }
        else
        {
            result2 = CheckIfUDTHasHungaryNotation(name);
        }

        if (result2)
        {
            result = RemoveHungarianNotationFromUDTAndEnums(name, isEnum);
        }
    }
    else if (isVariable)
    {
        if (options->removeHungaryNotationFromVariable)
        {
            if (CheckIfVariableHasHungaryNotation(name))
            {
                result = RemoveHungarianNotationFromVariable(name);
            }

            if (options->variableCamelCase)
            {
                result = ConvertPascalCaseToCamelCase(name);
            }
            else if (options->variableSnakeCase)
            {
                result = ConvertPascalCaseToSnakeCase(name);
            }
        }
        else if (!CheckIfVariableHasHungaryNotation(name))
        {
            if (options->variableCamelCase)
            {
                if (name.contains("_"))
                {
                    result = ConvertSnakeCaseToCamelCase(name);
                }

                if (name.at(0).isUpper())
                {
                    result = ConvertPascalCaseToCamelCase(name);
                }
            }
            else if (options->variablePascalCase)
            {
                if (name.contains("_"))
                {
                    result = ConvertSnakeCaseToPascalCase(name);
                }

                if (name.at(0).isLower())
                {
                    result = ConvertCamelCaseToPascalCase(name);
                }
            }
            else if (options->variableSnakeCase)
            {
                if (name.at(0).isLower())
                {
                    result = ConvertCamelCaseToPascalCase(name);
                }
                else if (name.at(0).isUpper())
                {
                    result = ConvertPascalCaseToSnakeCase(name);
                }
            }
        }
    }
    else if (isFunction)
    {
        if (options->functionCamelCase)
        {
            if (name.contains("_"))
            {
                result = ConvertSnakeCaseToCamelCase(name);
            }

            if (name.at(0) == '~' &&
                name.at(1).isUpper())
            {
                result = ConvertPascalCaseToCamelCase(name);
            }
            else if (name.at(0).isUpper())
            {
                result = ConvertPascalCaseToCamelCase(name);
            }
        }
        else if (options->functionPascalCase)
        {
            if (name.contains("_"))
            {
                result = ConvertSnakeCaseToPascalCase(name);
            }

            if (name.at(0) == '~' &&
                name.at(1).isLower())
            {
                result = ConvertCamelCaseToPascalCase(name);
            }
            else if (name.at(0).isLower())
            {
                result = ConvertCamelCaseToPascalCase(name);
            }
        }
        else if (options->functionSnakeCase)
        {
            if (name.at(0) == '~' &&
                name.at(1).isLower())
            {
                result = ConvertCamelCaseToPascalCase(name);
            }
            else if (name.at(0).isLower())
            {
                result = ConvertCamelCaseToPascalCase(name);
            }
            else if (name.at(0) == '~' &&
                name.at(1).isUpper())
            {
                result = ConvertPascalCaseToSnakeCase(name);
            }
            else if (name.at(0).isUpper())
            {
                result = ConvertPascalCaseToSnakeCase(name);
            }
        }
    }

    return result;
}

QString PDB::ConvertCamelCaseToPascalCase(const QString& name)
{
    QString result = name;

    if (result.at(0) == '~')
    {
        result[1] = result.at(1).toUpper();
    }
    else
    {
        result[0] = result.at(0).toUpper();
    }

    return result;
}

QString PDB::ConvertCamelCaseToSnakeCase(const QString& name)
{
    QString result = name;

    static QRegularExpression regExp1{ "(.)([A-Z][a-z]+)" };
    static QRegularExpression regExp2{ "([a-z0-9])([A-Z])" };

    result.replace(regExp1, "\\1_\\2");
    result.replace(regExp2, "\\1_\\2");

    result = result.toLower();

    return result;
}

QString PDB::ConvertPascalCaseToCamelCase(const QString& name)
{
    QString result = name;

    if (result.at(0) == '~')
    {
        result[1] = result.at(1).toLower();
    }
    else
    {
        result[0] = result.at(0).toLower();
    }

    return result;
}

QString PDB::ConvertPascalCaseToSnakeCase(const QString& name)
{
    QString result = name;

    static QRegularExpression regExp1{ "(.)([A-Z][a-z]+)" };
    static QRegularExpression regExp2{ "([a-z0-9])([A-Z])" };

    result.replace(regExp1, "\\1_\\2");
    result.replace(regExp2, "\\1_\\2");

    result = result.toLower();

    return result;
}

QString PDB::ConvertSnakeCaseToCamelCase(const QString& name)
{
    QString name2 = name;

    if (CheckIfAllLettersAreUpperCase(name))
    {
        name2 = name.toLower();
    }

    QStringList parts = name2.split('_', QString::SkipEmptyParts);

    for (int i = 1; i < parts.count(); ++i)
    {
        if (parts[i][0] == '~')
        {
            parts[i].replace(1, 1, parts.at(i).at(1).toUpper());
        }
        else
        {
            parts[i].replace(0, 1, parts.at(i).at(0).toUpper());
        }
    }

    return parts.join("");
}

QString PDB::ConvertSnakeCaseToPascalCase(const QString& name)
{
	QString name2 = name;

	if (CheckIfAllLettersAreUpperCase(name))
	{
		name2 = name.toLower();
	}

    QStringList parts = name2.split('_', QString::SkipEmptyParts);

    for (int i = 0; i < parts.size(); ++i)
    {
        if (parts[i][0] == '~')
        {
            parts[i].replace(1, 1, parts.at(i).at(1).toUpper());
        }
        else
        {
            parts[i].replace(0, 1, parts.at(i).at(0).toUpper());
        }
    }

    return parts.join("");
}

bool PDB::CheckIfAllLettersAreUpperCase(const QString& name)
{
	bool allLettersAreUpperCase = false;
	int length = name.length();

	for (int i = 0; i < length; i++)
	{
		if (name.at(i).isLetter())
		{
			if (name.at(i).isUpper())
			{
				allLettersAreUpperCase = true;
			}
			else
			{
				allLettersAreUpperCase = false;

				break;
			}
		}
	}

    return allLettersAreUpperCase;
}

bool PDB::CheckIfVariableHasHungaryNotation(const QString& name)
{
    bool result = false;

    if (name.startsWith("m_") || name.startsWith("g_"))
    {
        result = true;
    }

    if (name.startsWith("str") || name.startsWith("psz"))
    {
        result = true;
    }

    if (name.startsWith("sz") || name.startsWith("by") || name.startsWith("dw"))
    {
        result = true;
    }

    if (name.startsWith("p"))
    {
        result = true;
    }

    static char letters[] = { 's', 'h', 'c', 'y', 'n', 'f', 'd', 'b', 'u', 'w', 'l', 'v', 'm' }; //v -> Vector, m -> Matrix

    if (name.length() > 1 && name.at(1).isUpper())
    {
        for (int i = 0; i < sizeof(letters); i++)
        {
            if (name.startsWith(letters[i]))
            {
                result = true;
            }
        }
    }

    return result;
}

bool PDB::CheckIfUDTHasHungaryNotation(const QString& name)
{
    if ((name.startsWith("C") || name.startsWith("S") || name.startsWith("I") || name.startsWith("X")) &&
        name.length() > 1 &&
        name.at(1).isUpper())
    {
        return true;
    }

    return false;
}

bool PDB::CheckIfEnumHasHungaryNotation(const QString& name)
{
    if ((name.startsWith("E") || name.startsWith("e")) &&
        name.length() > 1 &&
        name.at(1).isUpper())
    {
        return true;
    }

    return false;
}

QString PDB::RemoveHungarianNotationFromUDTAndEnums(const QString& name, bool isEnum)
{
    QString result = name;

    if (isEnum)
    {
        if (result.startsWith("E") || result.startsWith("e"))
        {
            result.remove(0, 1);
        }
    }
    else
    {
        if (result.startsWith("C") || result.startsWith("S") || result.startsWith("I") || result.startsWith("X"))
        {
            result.remove(0, 1);
        }
    }

    return result;
}

QString PDB::RemoveHungarianNotationFromVariable(const QString& name)
{
    QString result = name;

    if (result.startsWith("m_") || result.startsWith("s_") || result.startsWith("g_"))
    {
        result.remove(0, 2);
    }

    if (result.startsWith("ms_") || result.startsWith("str") || result.startsWith("psz"))
    {
        result.remove(0, 3);
    }

    if (result.startsWith("sz") || result.startsWith("by") || result.startsWith("dw"))
    {
        result.remove(0, 2);
    }

    if (result.startsWith("p"))
    {
        result.remove(0, 1);
    }

    static char letters[] = { 's', 'h', 'c', 'y', 'n', 'f', 'd', 'b', 'u', 'w', 'l', 'v', 'm' }; //v -> Vector, m -> Matrix

    if (result.length() > 1 && result.at(1).isUpper())
    {
        for (int i = 0; i < sizeof(letters); i++)
        {
            if (result.startsWith(letters[i]))
            {
                result.remove(0, 1);

                break;
            }
        }
    }

    return result;
}

void PDB::ClearVTables()
{
    vTables.clear();
}

void PDB::ClearVirtualFunctionPrototypes()
{
    virtualFunctionPrototypes.clear();
}

void PDB::ClearVTableNames()
{
    vTableNames.clear();
}

void PDB::ClearElements()
{
    elements.clear();
}

void PDB::GetVTablesLayout(Element* element, QString& layout, quint64* offset, int level)
{
    if (element->elementType == ElementType::udtType)
    {
        vTableOffsets.insert(vTableNames.value(0), 0);

        layout += "1>  +---\n";
    }

    int count = element->children.count();

    for (int i = 0; i < count; i++)
    {
        if (element->children.at(i).elementType == ElementType::baseClassType)
        {
            JoinLists(&element->children[i]);

            if (i > 0)
            {
                *offset += element->children.at(i).baseClass.length;
                
                if (vTables.contains(element->children.at(i).baseClass.name))
                {
                    vTableOffsets.insert(element->children.at(i).baseClass.name, *offset);
                }
            }

            layout += QString("1> %1\t| ").arg(*offset);

            for (int j = 0; j < level; j++)
            {
                layout += " | ";
            }

            layout += QString("+--- (base class %1)\r\n").arg(element->children.at(i).baseClass.name);

            GetVTablesLayout(&element->children[i], layout, offset, level + 1);

            if (element->children.at(i).baseClass.hasVTablePointer)
            {
                layout += QString("1> %1\t| ").arg(*offset);

                for (int j = 0; j < level; j++)
                {
                    layout += " | ";
                }

                layout += " | {vfptr}\r\n";
            }

            layout += "1>  ";

            for (int j = 0; j < level + 1; j++)
            {
                layout += " | ";
            }
            
            layout += "+---\r\n";
        }
        else if (element->children.at(i).elementType == ElementType::dataType &&
            element->children.at(i).data.dataKind != DataIsStaticMember)
        {
            if (*offset != 0 && element->children.at(i).offset != 0)
            {
                *offset += element->children.at(i).size;
            }
        }
    }

    if (element->elementType == ElementType::udtType)
    {
        layout += "1>  +---\n";
    }
}

QString PDB::GetVirtualFunctionsInfo(const Element* element)
{
    QString virtualFunctionsInfo;
    QMap<int, QString>::const_iterator it;
    QMap<QString, int> virtualFunctions;
    QMultiMap<int, QString> adjustors;

    for (it = vTableNames.begin(); it != vTableNames.end(); it++)
    {
        virtualFunctionsInfo += QString("1>%1::$vftable@%2:\n").arg(element->udt.name).arg(it.value());

        QHash<QString, int>::const_iterator it2;
        int vTableOffset = 0;

        for (it2 = vTableOffsets.begin(); it2 != vTableOffsets.end(); it2++)
        {
            if (it.value() == it2.key())
            {
                vTableOffset = it2.value();
            }
        }

        if (vTableOffset == 0)
        {
            virtualFunctionsInfo += QString("1>  | &%1_meta\n").arg(element->udt.name);
            virtualFunctionsInfo += QString("1>  |  %1\n").arg(vTableOffset);
        }
        else
        {
            virtualFunctionsInfo += QString("1>  |  -%1\n").arg(vTableOffset);
        }

        QHash<QString, QHash<QString, QString>>::const_iterator it3;

        for (it3 = virtualFunctionPrototypes2.begin(); it3 != virtualFunctionPrototypes2.end(); it3++)
        {
            if (it.value() == it3.key())
            {
                QHash<QString, int>::const_iterator it4;
                QHash<QString, int> vTable = GetVTable(element->udt.vTableNames.value(it.key()));
                QHash<QString, QString> vTableFunctions = getFunctionPrototypes(element->udt.vTableNames.value(it.key()));
                QMap<int, QString> virtualFunctions2;

                for (it4 = vTable.begin(); it4 != vTable.end(); it4++)
                {
                    QHash<QString, QString>::const_iterator it5 = vTableFunctions.find(it4.key());

                    if (it5 != vTableFunctions.end())
                    {
                        QString functionName = it5.value();
                        QString functionName2 = it5.value().mid(it5.value().lastIndexOf("::") + 2);

                        if (functionName2.at(0) == '~')
                        {
                            functionName.replace(functionName2, "{dtor}");
                        }

                        QString result;

                        if (vTableOffset > 0)
                        {
                            QMap<QString, int>::const_iterator it6 = virtualFunctions.find(functionName);

                            if (it6 != virtualFunctions.end() && vTableOffset != it6.value())
                            {
                                result = QString("1> %1\t| &thunk: this-=%2; goto %3\n").arg(it4.value())
                                    .arg(it6.value() - vTableOffset).arg(functionName);
                            }
                            else
                            {
                                result = QString("1> %1\t| &%2\n").arg(it4.value()).arg(functionName);
                            }
                        }
                        else
                        {
                            result = QString("1> %1\t| &%2\n").arg(it4.value()).arg(functionName);
                        }

                        virtualFunctions2.insert(it4.value(), functionName);

                        QMap<QString, int>::const_iterator it6 = virtualFunctions.find(functionName);

                        if (it6 == virtualFunctions.end())
                        {
                            virtualFunctions.insert(functionName, vTableOffset);
                        }

                        virtualFunctions2.insert(it4.value(), result);
                    }
                }

                QMap<int, QString>::const_iterator it5;

                for (it5 = virtualFunctions2.begin(); it5 != virtualFunctions2.end(); it5++)
                {
                    virtualFunctionsInfo += it5.value();
                }

                QMap<QString, int>::const_iterator it6;

                for (it6 = virtualFunctions.begin(); it6 != virtualFunctions.end(); it6++)
                {
                    if (it6.key().startsWith(element->udt.name))
                    {
                        QString result = QString("1>%1 this adjustor: %2\n").arg(it6.key()).arg(it6.value());

                        adjustors.insert(it6.value(), result);
                    }
                }
            }
        }
    }

    QMultiMap<int, QString>::const_iterator it2;

    for (it2 = adjustors.begin(); it2 != adjustors.end(); it2++)
    {
        virtualFunctionsInfo += it2.value();
    }

    return virtualFunctionsInfo;
}

void PDB::GetMSVCLayout(Element* element, QString& layout, quint64* offset, int level)
{
    if (element->elementType == ElementType::udtType &&
        !element->data.hasChildren)
    {
        layout += QString("1>%1 %2\tsize(%3):\n").arg(element->udt.type).arg(element->udt.name).arg(element->size);
        layout += "1>  +---\n";

        vTableOffsets.insert(vTableNames.value(0), 0);
    }

    int count = element->children.count();

    for (int i = 0; i < count; i++)
    {
        if (element->children.at(i).elementType == ElementType::baseClassType)
        {
            JoinLists(&element->children[i]);

            layout += QString("1> %1\t| ").arg(*offset);

            for (int j = 0; j < level; j++)
            {
                layout += " | ";
            }

            layout += QString("+--- (base class %1)\r\n").arg(element->children.at(i).baseClass.name);

            GetMSVCLayout(&element->children[i], layout, offset, level + 1);

            layout += "1>  ";

            for (int j = 0; j < level + 1; j++)
            {
                layout += " | ";
            }

            layout += "+---\r\n";

            if (i > 0)
            {
                if (vTables.contains(element->children.at(i).baseClass.name))
                {
                    vTableOffsets.insert(element->children.at(i).baseClass.name, *offset);
                }
            }
        }
        else if (element->children.at(i).elementType == ElementType::dataType &&
            element->children.at(i).data.dataKind != DataIsStaticMember)
        {
            if (element->children.at(i).data.isVTablePointer)
            {
                layout += QString("1> %1\t| ").arg(*offset);

                for (int j = 0; j < level; j++)
                {
                    layout += " | ";
                }

                layout += " | {vfptr}\r\n";

                *offset += element->children.at(i).size;

                continue;
            }

            if (element->children.at(i).data.hasChildren && !element->children.at(i).data.isTypeNameOfEnum)
            {
                Element element2 = element->children.at(i);
                element2.elementType = ElementType::udtType;

                GetMSVCLayout(&element2, layout, offset, level);
            }
            else
            {
                if (element->children.at(i).numberOfBits > 0)
                {
                    int bitStart = 0;
                    int bits = 0;

                    while (element->children.at(i).numberOfBits > 0 && i < count)
                    {
                        bits = element->children.at(i).numberOfBits;

                        if (bitStart + bits <= element->children.at(i).size * 8)
                        {
                            if (element->children.at(i).data.hasChildren &&
                                element->children.at(i).udt.isAnonymousUnion)
                            {
                                layout += QString("1> %1\t| ").arg(element->offset);
                            }
                            else
                            {
                                layout += QString("1> %1\t| ").arg(*offset);
                            }

                            for (int j = 0; j < level; j++)
                            {
                                layout += " | ";
                            }

                            layout += QString("%1 (bitstart=%2,nbits=%3)\n").arg(element->children.at(i).data.name)
                                .arg(bitStart).arg(bits);
                        }
                        else
                        {
                            bitStart = 0;

                            *offset += element->children.at(i).size;
                            layout += QString("1> %1\t| ").arg(*offset);

                            for (int j = 0; j < level; j++)
                            {
                                layout += " | ";
                            }

                            layout += QString("%1 (bitstart=0,nbits=%2)\n").arg(element->children.at(i).data.name).arg(bits);
                        }

                        bitStart += bits;
                        i++;
                    }

                    i--;
                }
                else
                {
                    if (element->children.at(i).data.isPadding)
                    {
                        layout += QString("1> \t| ");
                    }
                    else
                    {
                        if (element->children.at(i).data.hasChildren &&
                            element->children.at(i).udt.isAnonymousUnion)
                        {
                            layout += QString("1> %1\t| ").arg(element->offset);
                        }
                        else
                        {
                            layout += QString("1> %1\t| ").arg(*offset);
                        }
                    }

                    for (int j = 0; j < level; j++)
                    {
                        layout += " | ";
                    }

                    if (element->children.at(i).data.isPadding)
                    {
                        layout += QString("<alignment member> (size=%1)\n").arg(element->children.at(i).size);
                    }
                    else
                    {
                        if (element->children.at(i).data.baseType == 0)
                        {
                            layout += QString("%1 %2\n").arg(element->children.at(i).data.typeName).arg(element->children.at(i).data.name);
                        }
                        else
                        {
                            layout += QString("%1\n").arg(element->children.at(i).data.name);
                        }
                    }
                }
            }

            *offset += element->children.at(i).size;
        }
    }

    if (element->elementType == ElementType::udtType &&
        !element->data.hasChildren)
    {
        layout += "1>  +---\n";
        layout += GetVirtualFunctionsInfo(element);
    }
}
