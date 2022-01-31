#pragma once

#include <Windows.h>
#include "DIA SDK/cvconst.h"
#include <QString>

struct BaseClass
{
    CV_access_e access;
    ULONGLONG length;
    QString name;
    QString parentClassName;
    LONG offset;
    UdtKind udtKind;
    BOOL isVirtualBaseClass;
    BOOL isIndirectVirtualBaseClass;
    DWORD virtualBaseDispIndex;
    LONG virtualBasePointerOffset;
    BOOL isConst;
    BOOL isVolatile;
    BOOL hasVTable;
    BOOL hasBaseClass;
    DWORD numOfVTables;
    BOOL hasVTablePointer;
    QMap<int, QString> vTableNames;
};
