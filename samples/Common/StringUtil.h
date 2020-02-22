#pragma once

#include <stdarg.h>
#include <utility>
#include <string>
#include <algorithm>

namespace cz
{

char* getTemporaryString();
char* formatStringVA(const char* format, va_list argptr);
const char* formatString(const char* format, ...);

//! Splits a string in the form IP:PORT (e.g:127.0.0.1:9000) into the ip and port
std::pair<std::string, int> splitAddress(const std::string& str);

inline bool notSpace(int a)
{
	return !(a == ' ' || a == '\t' || a == 0xA || a == 0xD);
}

// trim from start
template <class StringType>
static inline StringType ltrim(const StringType &s_)
{
	StringType s = s_;
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
	return s;
}

// trim from end
template <class StringType>
static inline StringType rtrim(const StringType &s_)
{
	StringType s = s_;
	s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
	return s;
}

// trim from both ends
template <class StringType>
static inline StringType trim(const StringType &s)
{
	return ltrim(rtrim(s));
}

} // namespace cz

