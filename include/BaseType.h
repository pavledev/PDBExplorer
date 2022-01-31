#pragma once

#include <Windows.h>

struct BaseType
{
    DWORD type;
    ULONGLONG length;
    BOOL isConst;
    BOOL isVolatile;
};
