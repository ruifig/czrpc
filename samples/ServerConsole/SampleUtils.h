#pragma once

namespace cz
{

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
}


