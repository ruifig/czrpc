#pragma once

#include <stdarg.h>
#include <utility>
#include <string>

namespace cz
{

char* getTemporaryString();
char* formatStringVA(const char* format, va_list argptr);
const char* formatString(const char* format, ...);

//! Splits a string in the form IP:PORT (e.g:127.0.0.1:9000) into the ip and port
std::pair<std::string, int> splitAddress(const std::string& str);

} // namespace cz

