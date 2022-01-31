#pragma once

#include <unordered_map>
#include <vector>
#include "DIA SDK/dia2.h"
#include "DIA SDK/diacreate.h"
#include <QObject>
#include <QHash>
#include <QtGlobal>
#include <QSet>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QStringBuilder>
#include <QMessageBox>
#include <QProcess>
#include "PEHeaderParser.h"
#include "SymbolRecord.h"
#include "Element.h"
#include "BaseType.h"
#include "PointerType.h"
#include "ArrayType.h"
#include "FunctionType.h"
#include "Options.h"
#include "ProcessType.h"
#include "DataOptions.h"
#include "FunctionOptions.h"
#include "PublicSymbol.h"
#include "MSVCDemangler.h"

class PDB : public QObject
{
    Q_OBJECT

private:
	IDiaDataSource* diaDataSource;
	IDiaSession* diaSession;
	IDiaSymbol* global;
    QHash<QString, DWORD>* diaSymbols;
    std::vector<SymbolRecord>* symbolRecords;
    int classesCount;
    int structsCount;
    int interfacesCount;
    int unionsCount;
    int enumsCount;
    bool processEnabled;
    Options* options;
    PEHeaderParser* peHeaderParser;
    CV_CPU_TYPE_e type;
    QList<QString> functionParameterNames;
    QSet<QString> includes;
    QHash<int, SymbolRecord> dependencies;
    QHash<QString, int> vTableIndices;
    QMap<int, QString> vTableIndices2;
    QHash<QString, QString> virtualFunctionPrototypes;
    QHash<QString, QHash<QString, QString>> virtualFunctionPrototypes2;
    QMap<int, QString> vTableNames;
    QMap<QString, int> vTableNames2;
    QHash<QString, QHash<QString, int>> vTables;
    QHash<QString, int> vTableOffsets;
    int vTableIndex;
    bool destructorAdded;
    QString fileNameWithoutExtension;
    QString filePath;
    QMap<int, Element> virtualFunctions;
    std::unordered_map<std::string, std::string> imports;
    ProcessType processType;
    QHash<QString, DWORD>* variables;
    QHash<QString, DWORD>* functions;
    QHash<QString, DWORD>* publicSymbols;
    bool hasVirtualDestructor;
    QHash<quint32, Element> elements;
    QString windowTitle;
    bool isTypeImported;
    QHash<QString, quint32> baseTypes;
    QHash<QString, QString> baseTypes2;
    QString parentClassName;
    QString parentClassName2;
    bool isMainUDT;
    bool belongsToMainUDT;
    QList<Element> typedefChildren;
    bool displayIncludes;
    MSVCDemangler msvcDemangler;

signals:
    void Completed();
    void SetProgressMinimum(int min);
    void SetProgressMaximum(int max);
    void SetProgressValue(int value);
    void SendStatusMessage(const QString& statusMessage);
    void SendStatusMessageToProcessDialog(const QString& statusMessage);

public:
    PDB(QObject* parent, Options* options, PEHeaderParser* peHeaderParser, QHash<QString, DWORD>* diaSymbols,
        std::vector<SymbolRecord>* symbolRecords, QHash<QString, DWORD>* variables = nullptr, QHash<QString, DWORD>* functions = nullptr,
        QHash<QString, DWORD>* publicSymbols = nullptr);
    ~PDB();

    bool ReadFromFile(const QString& filePath);
    void LoadPDBData();
	void GetVariables();
	void GetFunctions();
	void GetPublicSymbols();
    void GetSymbolsFromTable(IDiaEnumSymbols* enumSymbols, int* currentIndex, int* currentProcent, int* procent);
    int GetCountOfClasses();
    int GetCountOfStructs();
    int GetCountOfInterfaces();
    int GetCountOfUnions();
    int GetCountOfEnums();
    void Stop();

    RecordType GetRecordType(IDiaSymbol* symbol);
    RecordType GetType(IDiaSymbol* symbol);
    UDT GetUDT(IDiaSymbol* symbol);
    Function GetFunction(IDiaSymbol* symbol);
    BaseType GetBaseType(IDiaSymbol* symbol);
    PointerType GetPointerType(IDiaSymbol* symbol);
    ArrayType GetArrayType(IDiaSymbol* symbol);
    Enum GetEnum(IDiaSymbol* symbol);
    FunctionType GetFunctionType(IDiaSymbol* symbol);
    TypeDef GetTypeDef(IDiaSymbol* symbol);
    Value GetValue(IDiaSymbol* symbol);
    Data GetData(IDiaSymbol* symbol);
    BaseClass GetBaseClass(IDiaSymbol* symbol);
    PublicSymbol GetPublicSymbol(IDiaSymbol* symbol);
    QString GetSourceFilePath(IDiaSymbol* symbol);
    QString GetSourceFileInfo(IDiaSourceFile* source);
    QString GetLines();
    QString GetLineInfo(IDiaEnumLineNumbers* lines);
    QString GetLocation(IDiaSymbol* symbol);
    QString GetModules();
    QStringList GetModuleNames();
    QString GetCompilandDetails(IDiaSymbol* symbol);
    QString GetCompilandEnvironment(IDiaSymbol* symbol);
    QString GetExportName(IDiaSymbol* symbol);

	bool GetSymbolByID(DWORD id, IDiaSymbol** symbol);
	bool GetSymbolByTypeName(enum SymTagEnum symTag, QString typeName, IDiaSymbol** symbol);
    Element GetElement(SymbolRecord* symbolRecord, bool addToPrototypesList = false);
    Element GetElement(IDiaSymbol* symbol);
    void InsertElement(Element* element, const Element* childElement);
    void JoinLists(Element* element);
    void HandleChildElement(Element* parentElement, Element* childElement, bool& add, qint64& currentOffset, DWORD& childSize);
    void HandleBaseClassChild(Element* parentElement, Element* childElement);
    void HandleUDTChild(Element* parentElement, Element* childElement, bool& add);
    void HandleEnumChild(Element* childElement, bool& add);
    void HandleVTableChild(Element* parentElement, bool& add, qint64& currentOffset, DWORD& childSize);
    void HandleTypeDefChild(Element* childElement, bool& add);
    void HandleDataChild(Element* parentElement, Element* childElement, bool& add);
    void HandleFunctionChild(Element* parentElement, Element* childElement, bool& add);
    void AddVTablePointerToUDT(Element* element, qint64& currentOffset, DWORD& childSize);
    void CreateUnnamedType(Element* childElement);
    void AddPaddingToUDT(Element* parentElement, Element* childElement, qint64& currentOffset, int& alignCount);
    void AddEndPaddingToUDT(Element* parentElement, DWORD& childSize, long& sizeDifference);

    QString convertAccessSpecifierToString(CV_access_e accessSpecifier);
    QString convertCallingConventionToString(CV_call_e callingConvention);
    QString convertDataKindToString(DataKind dataKind);
    QString convertLocationTypeToString(LocationType locationType);
    QString convertVariantToString(VARIANT variant);
    QString convertLanguageToString(CV_CFL_LANG language);
    QString convertPlatformToString(CV_CPU_TYPE_e cpuType);
    QString GetTab(int level);
    QString GetBaseClassesInfo(const Element* element, bool addKeywords = true);
    QString GetEnumInfo(const Element* element, int level);
    QString GetUDTInfo(Element* element, int level);
    QString GetElementInfo(Element* element, int level = 0);
    QString GetDataInfo(const Data* data, int level);
    QString GetFunctionInfo(const Element* element, int level);
    Data ConvertRecordTypeToData(const RecordType* recordType);
    QString DataTypeToString(const Data* data, const DataOptions* dataOptions);
    QString FunctionTypeToString(const Element* element, const FunctionOptions* functionOptions);
    QString GenerateCustomParameterName(QString parameterType, int i);
    void GetTemplateIncludes(QString templateTypeName, const QString& parentClassName);
    bool CheckIfNameOfMainOrInnerUDT(const QString& typeName, const QString& parentClassName);
    void AddTypeNameToIncludesList(const QString& typeName, const QString& parentClassName);
    void AddComments(const Element* element, QString& text, int i);
    void AddAdditionalNewLine(const Element* element, QString& text, int i);
    bool AddAdditionalNewLineBeforeFunc(const Element* element, int i);
    void RemoveScopeResolutionOperators(QString& text, const QString& parentClassName);
    bool CheckIfInnerUDTBelongsToMainUDT(Element* element, Element* innerUDT);
    QString GetParentClassName(const QString& typeName);
    QString GetParentClassName(Element* element);
    bool CheckIfPublicKeywordShouldBeAdded(const Element* element);
    bool CheckIfHasAnyEmptyBaseClass(const Element* element);
    bool CheckIfRVOIsAppliedToFunction(IDiaSymbol* symbol);
    void DeclareFunctionsForStaticVariables(Element* element);
    void ImplementFunctionsForStaticVariables(const Element* element, QString& cppCode);
    Element CreateGetter(Element* element, const int i);
    Element CreateSetter(Element* element, const int i);
    ULONGLONG GetFunctionSize(DWORD relativeVirtualAddress);

    QString GetNameOfFirstVTable(const Element* element);
    void GetVTables(Element* element, bool addToPrototypesList = false);
    QHash<QString, int> GetVTable(QString vTableName);
    void UpdateVTable(Element* element, int indexOfVTable, QString vTableName, QMap<int, QString>* vTableNames,
        QHash<QString, int>* vTableIndices, bool addToPrototypesList = false,
        QHash<QString, QString>* virtualFunctionPrototypes = nullptr);
    void UpdateVTables(Element* element, QMap<int, QString>* vTableNames, bool addToPrototypesList = false,
        QHash<QString, QString>* virtualFunctionPrototypes = nullptr);
    QHash<QString, QString> getFunctionPrototypes(QString vTableName);
    void CheckIfDefaultCtorAndDtorAdded(Element* element);
    void CheckIfCopyCtorAndCopyAssignmentOpAdded(Element* element);
    void AddDefaultConstructor(Element* element);
    void AddDestructor(Element* element);
    void AddVirtualDestructor(Element* element);
    void AddCopyConstructor(Element* element);
    void AddCopyAssignmentOperator(Element* element);
    void ApplyReturnValueOptimization(Element* element);
    void FlattenUDT(Element* element, Element* newElement, quint64* offset, quint32* alignmentNum);

    Element OrderUDTElementChildren(Element element);
    Element OrderUDTChildrenByAccessSpecifiers(Element element);

    quint32 GetChildrenSize(const Element* element);
    bool CheckIfChildrenSizesAreCorrect(const Element* element, QString& message);
    void FixOffsets(Element* element);
    void AppendElement(Element* element, QList<Element>* children, int startPosition, int endPosition);
    bool ShouldIncludeElement(Element element);
    void CheckIfUnionsAreMissing(Element* element);

    quint32 CalculateDefaultAlignment(const Element* element, QHash<QString, int> checkedTypes);
    quint32 GetCorrectAlignment(quint32 defaultAlignment, quint32 typeSize);
    unsigned int GetGreatestPaddingInUDT(const Element* element);
    unsigned int NextPowerOf2(unsigned int value);

    void FormatString(QString& string);
    QString TrimStart(const QString& input);
    QString TrimEnd(const QString& input);

    QString GenerateCPPCode(const Element* element, int level = 0);
    QString ImplementVariadicTemplateFunction(const Element* element, int level = 0);
    QString GetBaseAddressVariableName();

    void SetFileNameWithoutExtension(const QString& fileName);
    void SetWindowTitle(const QString& windowTitle);
    void SetFilePath(const QString& filePath);
    void SetMachineType(CV_CPU_TYPE_e type);

    void SetProcessType(ProcessType processType);
    void ExportSymbol(SymbolRecord symbolRecord);
    void GetDependencies(const Element* element);
    void ExportSymbolWithDependencies(SymbolRecord* symbolRecord);
    void ExportAllSymbols();

    QString ModifyNamingCovention(const QString& name, bool isEnum, bool isFunction, bool isVariable);
    QString ConvertCamelCaseToPascalCase(const QString& name);
    QString ConvertCamelCaseToSnakeCase(const QString& name);
    QString ConvertPascalCaseToCamelCase(const QString& name);
    QString ConvertPascalCaseToSnakeCase(const QString& name);
    QString ConvertSnakeCaseToCamelCase(const QString& name);
    QString ConvertSnakeCaseToPascalCase(const QString& name);
    bool CheckIfAllLettersAreUpperCase(const QString& name);

    bool CheckIfVariableHasHungaryNotation(const QString& name);
    bool CheckIfUDTHasHungaryNotation(const QString& name);
    bool CheckIfEnumHasHungaryNotation(const QString& name);
    QString RemoveHungarianNotationFromUDTAndEnums(const QString& name, bool isEnum = false);
    QString RemoveHungarianNotationFromVariable(const QString& name);

    void ClearVTables();
    void ClearVirtualFunctionPrototypes();
    void ClearVTableNames();
    void ClearElements();

    void GetVTablesLayout(Element* element, QString& layout, quint64* offset, int level = 0);
    QString GetVirtualFunctionsInfo(const Element* element);
    void GetMSVCLayout(Element* element, QString& layout, quint64* offset, int level = 0);
};
