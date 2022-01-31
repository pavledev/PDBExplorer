#pragma once

#include <Windows.h>
#include <QString>
#include <QVariant>
#include "DIA SDK/cvconst.h"
#include "RecordType.h"
#include "Value.h"

struct Data
{
    CV_access_e access;
    DWORD bitOffset;
    DWORD numberOfBits;
    BOOL isCompilerGenerated;
    DataKind dataKind;
    BOOL isAggregated;
    ULONGLONG length;
    QString originalTypeName;
    QString typeName;
    QString name;
    QString parentClassName;
    QString parentType;
    LONG offset;
    ULONGLONG virtualAddress;
    DWORD relativeVirtualAddress;
    Value value;
    LocationType locationType;
    QString location;

    DWORD size;
    DWORD baseType;
    BOOL isPointer;
    BOOL isReference;
    DWORD pointerLevel;
    DWORD referenceLevel;
    BOOL isArray;
    QList<quint32> arrayCount;
    QString functionReturnType;
    QList<QString> functionParameters;
    BOOL isEndPadding;
    BOOL noType;
    BOOL unnamedType;
    BOOL isTypeNameOfEnum;
    BOOL isVTablePointer;
    BOOL isPadding;
    BOOL hasChildren;
    BOOL isFunctionPointer;
    BOOL isTypeConst;
    BOOL isTypeVolatile;
    BOOL isPointerConst;
    BOOL isPointerVolatile;
    QString declaration;
    BOOL isVariadicFunction;
    CV_call_e callingConvention;
};
