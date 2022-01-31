#pragma once

#include <Windows.h>
#include "DIA SDK/cvconst.h"
#include <QString>

struct Enum
{
    DWORD id;
    DWORD baseType;
    ULONGLONG length;
    QString originalTypeName;
    QString name;
    QString parentClassName;
    BOOL isConst;
    BOOL isVolatile;
    CV_access_e access;
    BOOL isUnnamed;
    BOOL isAnonymous;
    BOOL isNested;
};
