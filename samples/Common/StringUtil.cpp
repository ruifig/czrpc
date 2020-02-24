#include "CommonPCH.h"
#include "StringUtil.h"

#define CZ_TEMPORARY_STRING_MAX_SIZE 512
#define CZ_TEMPORARY_STRING_MAX_NESTING 20

#pragma warning(disable:4996)

#ifdef _WIN32
	#define my_vsnprintf _vsnprintf
#else
	#define my_vsnprintf vsnprintf
#endif

namespace cz
{

char* getTemporaryString()
{
	// Use several static strings, and keep picking the next one, so that callers can hold the string for a while without risk of it
	// being changed by another call.
	thread_local static char bufs[CZ_TEMPORARY_STRING_MAX_NESTING][CZ_TEMPORARY_STRING_MAX_SIZE];
	thread_local static int nBufIndex=0;

	char* buf = bufs[nBufIndex];
	nBufIndex++;
	if (nBufIndex==CZ_TEMPORARY_STRING_MAX_NESTING)
		nBufIndex = 0;

	return buf;
}

char* formatStringVA(const char* format, va_list argptr)
{
	char* buf = getTemporaryString();
	if (my_vsnprintf(buf, CZ_TEMPORARY_STRING_MAX_SIZE, format, argptr) == CZ_TEMPORARY_STRING_MAX_SIZE)
		buf[CZ_TEMPORARY_STRING_MAX_SIZE-1] = 0;
	return buf;
}
const char* formatString(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	const char *str= formatStringVA(format, args);
	va_end(args);
	return str;
}

std::pair<std::string, int> splitAddress(const std::string& str)
{
	std::pair<std::string, int> res;

	auto i = str.find(':');
	if (i == std::string::npos)
		return res;

	res.first = str.substr(0, i);
	res.second = std::atoi(str.substr(i + 1).c_str());
	if (res.second == 0)
		return std::pair<std::string, int>();
	else
		return res;
}

} // namespace cz
