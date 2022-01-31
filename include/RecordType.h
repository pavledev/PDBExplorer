#pragma once

#include "DIA SDK/cvconst.h"
#include <QString>
#include <QList>
#include "Data.h"

struct RecordType
{
    int baseType;
    QString originalTypeName;
    QString typeName;
    QString name;
    int size;
    int offset;
    int bitOffset;
    int bitSize;
    bool isPointer;
    bool isReference;
    int pointerLevel;
    int referenceLevel;
    bool isArray;
    QList<quint32> arrayCount;
    Data functionReturnType1;
    QString functionReturnType2;
    QString originalFunctionReturnType;
    bool functionType;
    bool noType;
    bool isVariadicFunction;
    DWORD parametersCount;
    QList<QString> functionParameters;
    QList<QString> originalFunctionParameters;
    CV_call_e callingConvention;
    bool isTypeNameOfEnum;
    bool isTypeConst;
    bool isTypeVolatile;
    bool isPointerConst;
    bool isPointerVolatile;
    bool isFunctionConst;
    bool isFunctionVolatile;
    bool isFunctionPointer;
    bool functionReturnsFunctionPointer;
};
