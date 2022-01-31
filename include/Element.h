#pragma once

#include <Windows.h>
#include "ElementType.h"
#include "UDT.h"
#include "Function.h"
#include "TypeDef.h"
#include "Data.h"
#include "Enum.h"
#include "BaseClass.h"

struct Element
{
    DWORD size;
    DWORD offset;
    DWORD bitOffset;
    DWORD numberOfBits;
    ElementType elementType;
    UDT udt;
    Function function;
    TypeDef typeDef;
    Data data;
    Enum enum1;
    BaseClass baseClass;
    QList<QString> baseClassNames;
    QList<Element> children;
    QList<Element> baseClassChildren;
    QList<Element> enumChildren;
    QList<Element> udtChildren;
    QList<Element> typedefChildren;
    QList<Element> dataChildren;
    QList<Element> staticDataChildren;
    QList<Element> virtualFunctionChildren;
    QList<Element> nonVirtualFunctionChildren;
    QList<Element> localVariables;
};
