/********************************************************************
	CrazyGaze (http://www.crazygaze.com)
	Author : Rui Figueira
	Email  : rui@crazygaze.com
	
	purpose:
	
*********************************************************************/

#pragma once

#include <string>
#include <vector>

namespace cz
{

class Parameters
{
public:
	struct Param
	{
		template<class T1, class T2>
		Param(T1&& name_, T2&& value_) : name(std::forward<T1>(name_)), value(std::forward<T2>(value_)){}
		std::string name;
		std::string value;
	};
	Parameters();
	void set(int argc, char* argv[]);
	const Param* begin() const;
	const Param* end() const;
	bool has(const char* name) const;
	bool has(const std::string& name) const;
	const std::string& get(const char *name) const;
	void clear()
	{
		m_args.clear();
	}
private:
	static std::string ms_empty;
	std::vector<Param> m_args;
};

} // namespace cz

