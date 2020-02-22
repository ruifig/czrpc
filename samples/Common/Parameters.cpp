/********************************************************************
	CrazyGaze (http://www.crazygaze.com)
	Author : Rui Figueira
	Email  : rui@crazygaze.com
	
	purpose:
	
*********************************************************************/

#include "CommonPCH.h"
#include "Parameters.h"

namespace cz
{

std::string Parameters::ms_empty;
Parameters::Parameters()
{
}

void Parameters::set(int argc, char* argv[])
{
	if (argc<=1)
		return;
	for (int i=1; i<argc; i++)
	{
		const char *arg = argv[i];
		if (*arg == '-')
			arg++;

		const char *separator = strchr(arg, '=');
		if (separator==nullptr)
		{
			m_args.emplace_back(arg, "");
		}
		else
		{
			std::string name(arg, separator);
			std::string value(++separator);
			m_args.emplace_back(std::move(name), std::move(value));
		}
	}
}

const Parameters::Param* Parameters::begin() const
{
	if (m_args.size())
		return &m_args[0];
	else
		return nullptr;
}

const Parameters::Param* Parameters::end() const
{
	return begin() + m_args.size();
}

bool Parameters::has( const char* name ) const
{
	for(auto &i: m_args)
	{
		if (i.name==name)
		{
			return true;
		}
	}
	return false;
}

bool Parameters::has( const std::string& name ) const
{
	return has(name.c_str());
}

const std::string& Parameters::get( const char *name ) const
{
	for (auto &i: m_args)
	{
		if (i.name==name)
			return i.value;
	}
	return ms_empty;
}

} // namespace cz



