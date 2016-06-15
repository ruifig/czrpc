#pragma once

#include <string>
#include <vector>

#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable:4996) // deprecated
#endif

namespace cz
{
namespace rpc
{

class Any
{
public:
	enum class Type { None, Bool, Integer, UnsignedInteger, Float, String, Blob, MAX};
private:
	using LargestFundamental = float;
	static constexpr Type LargestFundamentalType = Type::Float;
public:

	Any() : m_type(Type::None)
	{
	}

	~Any()
	{
		destroy();
	}

	// Constructing from an unsuported type leaves it set to None
	template<typename T>
	Any(const T&) : m_type(Type::None)
	{
	}

	Type getType() const
	{
		return m_type;
	}

	explicit Any(bool v)
		: m_type(Type::Bool)
	{
		asF<bool>() = v;
	}

	explicit Any(int v)
		: m_type(Type::Integer)
	{
		asF<int>() = v;
	}

	explicit Any(unsigned v)
		: m_type(Type::UnsignedInteger)
	{
		asF<unsigned>() = v;
	}

	explicit Any(float v)
		: m_type(Type::Float)
	{
		asF<float>() = v;
	}

	explicit Any(std::string v)
		: m_type(Type::String)
		, m_str(std::move(v))
	{ }

	explicit Any(const char* v)
		: m_type(Type::String)
		, m_str(v)
	{ }

	explicit Any(std::vector<unsigned char> v)
		: m_type(Type::Blob)
		, m_blob(std::move(v))
	{ }

	Any(const Any& other)
	{
		copyFrom(other);
	}

	Any(Any&& other)
	{
		moveFrom(std::move(other));
	}

	Any& operator=(const Any& other)
	{
		if (this != &other)
		{
			destroy();
			copyFrom(other);
		}
		return *this;
	}

	Any& operator=(Any&& other)
	{
		if (this != &other)
		{
			destroy();
			moveFrom(std::move(other));
		}
		return *this;
	}

	template<typename T>
	bool getAs(T& dst) const
	{
		return false;
	}

	bool getAs(bool& dst) const
	{
		if (m_type == Type::Bool || m_type == Type::Integer || m_type == Type::UnsignedInteger)
		{
			dst = asF<unsigned int>() == 0 ? false : true;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool getAs(int& dst) const
	{
		if (m_type == Type::Integer)
		{
			dst = asF<int>();
			return true;
		}
		else if (m_type == Type::Float)
		{
			dst = static_cast<int>(asF<float>());
			return true;
		}
		else
		{
			return false;
		}
	}

	bool getAs(unsigned& dst) const
	{
		if (m_type == Type::UnsignedInteger)
		{
			dst = asF<unsigned>();
			return true;
		}
		else if (m_type == Type::Integer)
		{
			dst = asF<int>();
			return true;
		}
		else if (m_type == Type::Float)
		{
			dst = static_cast<int>(asF<float>());
			return true;
		}
		else
		{
			return false;
		}
	}

	bool getAs(float& dst) const
	{
		if (m_type == Type::Float)
		{
			dst = asF<float>();
			return true;
		}
		else if (m_type == Type::Integer)
		{
			dst = static_cast<float>(asF<int>());
			return true;
		}
		else if (m_type == Type::UnsignedInteger)
		{
			dst = static_cast<float>(asF<unsigned>());
			return true;
		}
		else
		{
			return false;
		}
	}

	bool getAs(std::string& dst) const
	{
		if (m_type == Type::String)
		{
			dst = m_str;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool getAs(std::vector<unsigned char>& dst) const
	{
		if (m_type == Type::Blob)
		{
			dst = m_blob;
			return true;
		}
		else
		{
			return false;
		}
	}

	const char* toString() const
	{
		thread_local static char tmp[22];
		switch (m_type)
		{
		case Type::None:
			return "";
		case Type::Bool:
			return asF<bool>() ? "true" : "false";
		case Type::Integer:
			return itoa(asF<int>(), tmp, 10);
		case Type::UnsignedInteger:
			return itoa(asF<unsigned>(), tmp, 10);
		case Type::Float:
		{
			auto res = snprintf(tmp, sizeof(tmp), "%.4f", asF<float>());
			if (res >= 0 && res < sizeof(tmp))
				return tmp;
			else
				return "conversion error";
		}
		case Type::String:
			return m_str.c_str();
		case Type::Blob:
		{
			int res = snprintf(tmp, sizeof(tmp), "BLOB{%d}", static_cast<int>(m_blob.size()));
			if (res >= 0 && res < sizeof(tmp))
				return tmp;
			else
				return "conversion error";
		}
		default:
			assert(false && "Unimplemented");
			return "";
		};
	}

	template<typename S>
	void write(S& s) const
	{
		s << (unsigned char)m_type;
		if (m_type == Type::Bool)
			s << asF<bool>();
		else if (m_type == Type::String)
			s << m_str;
		else if (m_type == Type::Blob)
			s << m_blob;
		else
			s << asF<LargestFundamental>();
	}
	
	template<typename S>
	void read(S& s)
	{
		destroy();
		// Read type
		unsigned char t;
		s >> t;
		assert(t < (unsigned char)Type::MAX);
		m_type = static_cast<Type>(t);

		// Read data
		if (m_type == Type::Bool)
			s >> asF<bool>();
		else if (m_type == Type::String)
		{
			std::string v;
			s >> v;
			new(&m_str) std::string(std::move(v));
		}
		else if (m_type == Type::Blob)
		{
			std::vector<unsigned char> v;
			s >> v;
			new(&m_blob) std::vector<unsigned char>(std::move(v));
		}
		else
			s >> asF<LargestFundamental>();
	}


private:

	void copyFrom(const Any& other)
	{
		m_type = other.m_type;
		if (m_type == Type::String)
			new(&m_str) std::string(other.m_str);
		else if (m_type == Type::Blob)
			new(&m_blob) std::vector<unsigned char>(other.m_blob);
		else
			asF<LargestFundamental>() = other.asF<LargestFundamental>();
	}

	void moveFrom(Any&& other)
	{
		m_type = other.m_type;
		if (m_type == Type::String)
			new(&m_str) std::string(std::move(other.m_str));
		else if (m_type == Type::Blob)
			new(&m_blob) std::vector<unsigned char>(std::move(other.m_blob));
		else
			asF<LargestFundamental>() = other.asF<LargestFundamental>();
	}

	void destroy()
	{
		using String = std::string;
		using Vector = std::vector<unsigned char>;
		switch(m_type)
		{
		case Type::String:
			m_str.~String();
			break;
		case Type::Blob:
			m_blob.~Vector();
			break;
		default:
			asF<LargestFundamental>() = 0;
		};

		m_type = Type::None;
	}

	template<typename T>
	T& asF()
	{
		static_assert(std::is_fundamental<T>::value, "Not a fundamental type");
		static_assert(sizeof(T) <= sizeof(m_f), "Storage not big enough for desired type");
		return *reinterpret_cast<T*>(&m_f);
	}

	template<typename T>
	const T& asF() const
	{
		static_assert(std::is_fundamental<T>::value, "Not a fundamental type");
		static_assert(sizeof(T) <= sizeof(m_f), "Storage not big enough for desired type");
		return *reinterpret_cast<const T*>(&m_f);
	}

	union
	{
		int m_f; // any supported fundamental type
		std::string m_str;
		std::vector<unsigned char> m_blob;
	};
	Type m_type;

};

// Specialize ParamTraits for Any, so it can be used as RPC parameters
template<>
struct ParamTraits<Any>
{
	using store_type = Any;
	static constexpr bool valid = true;

	template<typename S>
	static void write(S& s, const Any& v)
	{
		v.write(s);
	}

	template<typename S>
	static void read(S& s, Any& v)
	{
		v.read(s);
	}

	static Any&& get(Any&& v)
	{
		return std::move(v);
	}
};

namespace details
{
	template<typename Tuple, bool Done, int N>
	struct convert_any
	{
		static bool convert(const std::vector<Any>& v, Tuple& dst)
		{
			if (!v[N].getAs(std::get<N>(dst)))
				return false;
			return convert_any<Tuple, std::tuple_size<Tuple>::value == N + 1, N + 1>::convert(v, dst);
		}
	};

	template<typename Tuple, int N>
	struct convert_any<Tuple, true, N>
	{
		static bool convert(const std::vector<Any>& v, Tuple& dst)
		{
			return true;
		}
	};
}

template<typename Tuple>
static bool toTuple(const std::vector<Any>& v, Tuple& t)
{
	if (v.size() != std::tuple_size<Tuple>::value)
		return false;
	return details::convert_any<Tuple, std::tuple_size<Tuple>::value == 0, 0>::convert(v, t);
}

} // namespace rpc
} // namespace cz

#if defined(_MSC_VER)
	#pragma warning(pop)
#endif