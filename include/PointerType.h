#include <Windows.h>

struct PointerType
{
    ULONGLONG length;
    BOOL isReference;
    BOOL isConst;
    BOOL isVolatile;
};
