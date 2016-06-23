#pragma once

#include <stdarg.h>

namespace cz
{

char* getTemporaryString();
char* formatStringVA(const char* format, va_list argptr);
const char* formatString(const char* format, ...);

} // namespace cz

