#pragma once

#include <Windows.h>

struct ArrayType
{
    DWORD count;
    ULONGLONG length;
    BOOL isConst;
    BOOL isVolatile;
};
