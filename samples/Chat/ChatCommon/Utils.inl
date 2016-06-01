#pragma once

#define CZ_TEMPORARY_STRING_MAX_SIZE 512
#define CZ_TEMPORARY_STRING_MAX_NESTING 20

char* getTemporaryString()
{
	// Use several static strings, and keep picking the next one, so that callers can hold the string for a while without risk of it
	// being changed by another call.
	__declspec( thread ) static char bufs[CZ_TEMPORARY_STRING_MAX_NESTING][CZ_TEMPORARY_STRING_MAX_SIZE];
	__declspec( thread ) static int nBufIndex=0;

	char* buf = bufs[nBufIndex];
	nBufIndex++;
	if (nBufIndex==CZ_TEMPORARY_STRING_MAX_NESTING)
		nBufIndex = 0;

	return buf;
}

char* formatStringVA(const char* format, va_list argptr)
{
	char* buf = getTemporaryString();
	if (_vsnprintf(buf, CZ_TEMPORARY_STRING_MAX_SIZE, format, argptr) == CZ_TEMPORARY_STRING_MAX_SIZE)
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

