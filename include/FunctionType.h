#pragma once

#include <Windows.h>
#include "DIA SDK/cvconst.h"

struct FunctionType
{
    CV_call_e callingConvention;
    DWORD parametersCount;
    BOOL isConst;
    BOOL isVolatile;
};
