#pragma once

#include <Windows.h>
#include "DIA SDK/cvconst.h"
#include <QString>

struct UDT
{
    DWORD id;
    QString originalTypeName;
    QString name;
    QString parentClassName;
    QString type;
    ULONGLONG length;
    UdtKind udtKind;
    BOOL isConst;
    BOOL isVolatile;
    CV_access_e access;
    BOOL hasVTable;
    BOOL hasBaseClass;
    DWORD numOfVTables;
    BOOL hasVTablePointer;
    QMap<int, QString> vTableNames;
    BOOL isAbstract;
    BOOL isUnnamed;
    BOOL isAnonymousUnion;
    BOOL isAnonymousStruct;
    BOOL isNested;
    BOOL isMainUDT;
    BOOL belongsToMainUDT;
    DWORD countOfVirtualFunctions;
    BOOL hasDestructor;
    BOOL hasVirtualDestructor;
    BOOL hasDefaultConstructor;
    BOOL hasCopyConstructor;
    BOOL hasCopyAssignmentOperator;
    QSet<QString> includes;
    DWORD defaultAlignment;
    DWORD correctAlignment;
    BOOL hasCastOperator;
};
