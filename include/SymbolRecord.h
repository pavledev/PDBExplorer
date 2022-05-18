#pragma once

#include <Windows.h>
#include <QString>
#include "SymbolType.h"

struct SymbolRecord
{
    DWORD id;
    QString typeName;
    SymbolType type;
};
