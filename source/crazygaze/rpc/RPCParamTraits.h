#pragma once

namespace cz {
namespace rpc {

	namespace detail {
		struct json_impl
		{
			static std::string to_json(const char* val)
			{
				std::string res = "\"";
				while (*val)
				{
					switch (*val)
					{
					case '\b':
						res += "\\b";
						break;
					case '\f':
						res += "\\f";
						break;
					case '\n':
						res += "\\n";
						break;
					case '\r':
						res += "\\r";
						break;
					case '\t':
						res += "\\t";
						break;
					case '"':
						res += "\\\"";
						break;
					case '\\':
						res += "\\\\";
						break;
					default:
						res += *val;
					}
					val++;
				}
				return  res + "\"";
			}

			template<typename T>
			static std::string to_string_impl(const T& v)
			{
				if constexpr (std::is_same_v<T, std::string> || std::is_same_v < T, const char*>)
				{
					return v;
				}
				else
				{
					return ParamTraits<T>::to_json(v);
				}
			}

			template<typename Tuple, std::size_t... Idx>
			static std::string to_json_tuple_impl(const Tuple& t, std::index_sequence<Idx...>)
			{
				std::vector<std::string> res({ to_string_impl(std::get<Idx>(t))... });
				return cz::rpc::detail::to_json(res);
			}
		};

		template<
			typename T,
			typename = std::enable_if<std::is_arithmetic_v<T>>::type
		>
			std::string to_json(T val)
		{
			return std::to_string(val);
		}

		inline std::string to_json(bool val)
		{
			return std::string(val ? "true" : "false");
		}

		inline std::string to_json(const char* val)
		{
			return detail::json_impl::to_json(val);
		}

		inline std::string to_json(const std::string& val)
		{
			return detail::json_impl::to_json(val.c_str());
		}

		// std::vector of any T that can be converted to json
		template<typename T>
		std::string to_json(const std::vector<T>& val)
		{
			if (val.size() == 0)
				return "[]";

			std::string res;
			for (auto&& v : val)
				res += "," + ParamTraits<T>::to_json(v);
			res += "]";
			res[0] = '[';

			return res;
		}

		// std::pair of any FIRST,SECOND that can be converted to json
		// #RVF : Not sure this is appropriate.
		// The user might not want fields named 'first' and 'second'
		template<typename FIRST, typename SECOND>
		std::string to_json(const std::pair<FIRST, SECOND>& val)
		{
			std::string res = "{\"first\":" + ParamTraits<FIRST>::to_json(val.first);
			res += ", \"second\":" + ParamTraits<SECOND>::to_json(val.second) + "}";
			return res;
		}

		template<typename... Args>
		std::string to_json(const std::tuple<Args...>& val)
		{
			return detail::json_impl::to_json_tuple_impl(val, std::index_sequence_for<Args...>());
		}


	} // namespace detail

template <typename T>
struct DefaultParamTraits {
    using store_type = T;
    static constexpr bool valid = true;

    static store_type&& get(store_type&& v) {
        static_assert(
            valid == true,
            "Invalid RPC parameter type. Specialize ParamTraits if required.");
        return std::move(v);
    }
};

// By default, all types for which ParamTraits is not specialized, are invalid
template <typename T, typename ENABLED = void>
struct ParamTraits {
    using store_type = int;
    static constexpr bool valid = false;
};

//
// Make "const T&" valid for any valid T
#define CZRPC_ALLOW_CONST_LVALUE_REFS                                          \
    namespace cz {                                                             \
    namespace rpc {                                                            \
    template <typename T>                                                      \
    struct ParamTraits<const T&> : ParamTraits<T> {                            \
        static_assert(ParamTraits<T>::valid,                                   \
                      "Invalid RPC parameter type. Specialize ParamTraits if " \
                      "required.");                                            \
    };                                                                         \
    }                                                                          \
    }

//
// Make "T&" valid for any valid T
#define CZRPC_ALLOW_NON_CONST_LVALUE_REFS                                      \
    namespace cz {                                                             \
    namespace rpc {                                                            \
    template <typename T>                                                      \
    struct ParamTraits<T&> : ParamTraits<T> {                                  \
        static_assert(ParamTraits<T>::valid,                                   \
                      "Invalid RPC parameter type. Specialize ParamTraits if " \
                      "required.");                                            \
        static T& get(typename ParamTraits<T>::store_type&& v) {               \
            return v;                                                          \
        }                                                                      \
    };                                                                         \
    }                                                                          \
    }

//
// Make "T&&" valid for any valid T
#define CZRPC_ALLOW_RVALUE_REFS                                                \
    namespace cz {                                                             \
    namespace rpc {                                                            \
    template <typename T>                                                      \
    struct ParamTraits<T&&> : ParamTraits<T> {                                 \
        static_assert(ParamTraits<T>::valid,                                   \
                      "Invalid RPC parameter type. Specialize ParamTraits if " \
                      "required.");                                            \
    };                                                                         \
    }                                                                          \
    }

#define CZRPC_DEFINE_CONST_LVALUE_REF(TYPE)             \
	namespace cz                                        \
	{                                                   \
	namespace rpc                                       \
	{                                                   \
	template <>                                         \
	struct ParamTraits<const TYPE&> : ParamTraits<TYPE> \
	{                                                   \
	};                                                  \
	}                                                   \
	}

#define CZRPC_DEFINE_NON_CONST_LVALUE_REF(TYPE)                                    \
	namespace cz                                                                   \
	{                                                                              \
	namespace rpc                                                                  \
	{                                                                              \
	template <>                                                                    \
	struct ParamTraits<TYPE&> : ParamTraits<TYPE>                                  \
	{                                                                              \
		static TYPE& get(typename ParamTraits<TYPE>::store_type&& v) { return v; } \
	};                                                                             \
	}                                                                              \
	}

#define CZRPC_DEFINE_RVALUE_REF(TYPE)              \
	namespace cz                                   \
	{                                              \
	namespace rpc                                  \
	{                                              \
	template <>                                    \
	struct ParamTraits<TYPE&&> : ParamTraits<TYPE> \
	{                                              \
	};                                             \
	}                                              \
	}

// Specializes ParamTraits for TYPE, and uses a generic serialization function that uses the operator ^.
// This allows the user to have just one function that specifies all the fields to serialize/deserialize for a given
// type.
// E.g:
//	template<cz::rpc::StreamDirection D>
//	void generic_serialize(cz::rpc::StreamWrapper<D>& s, CustomType& v)
//	{
//		s ^ v.a;
//		s ^ v.b;
//		s ^ v.c;
//	}
//
// Also, there must be a to_json function defined by the user
#define CZRPC_DEFINE_PARAMTRAITS_FROM_GENERIC(TYPE)                           \
	template <>                                                               \
	struct ::cz::rpc::ParamTraits<TYPE> : ::cz::rpc::DefaultParamTraits<TYPE> \
	{                                                                         \
		template <typename S>                                                 \
		static void write(S& s, const TYPE& v)                                \
		{                                                                     \
			auto sw = StreamWrapper<Write>(s);                                \
			generic_serialize(sw, const_cast<TYPE&>(v));                      \
		}                                                                     \
		template <typename S>                                                 \
		static void read(S& s, TYPE& v)                                       \
		{                                                                     \
			auto sw = StreamWrapper<Read>(s);                                 \
			generic_serialize(sw, v);                                         \
		}                                                                     \
		static std::string to_json(const TYPE& v)                             \
		{                                                                     \
			StreamWrapper<WriteJson> sw;                                      \
			generic_serialize(sw, const_cast<TYPE&>(v));                      \
			return sw.s;                                                      \
		}                                                                     \
	};

// void type is valid
template <>
struct ParamTraits<void> {
    static constexpr bool valid = true;
	using store_type = void;
};

// arithmetic types
template <typename T>
struct ParamTraits<T,
                   typename std::enable_if_t<std::is_arithmetic<T>::value> >
    : DefaultParamTraits<T> {
    using store_type = typename std::decay<T>::type;
    static constexpr bool valid = true;

    template <typename S>
    static void write(S& s, typename std::decay<T>::type v) {
        s.write(&v, sizeof(v));
    }

    template <typename S>
    static void read(S& s, store_type& v) {
        s.read(&v, sizeof(v));
    }

	static std::string to_json(store_type v)
	{
		return detail::to_json(v);
	}
};

//
// Macros to makes it easier to serialize enums
//
#define CZRPC_HELPER_ENUM_DEFINE(ENUM, TYPE)                                   \
  template <>                                                                  \
  struct ::cz::rpc::ParamTraits<ENUM> : ::cz::rpc::DefaultParamTraits<TYPE> {  \
    using store_type = ENUM;                                                   \
    static constexpr bool valid = true;                                        \
    template <typename S> static void write(S &s, ENUM v) {                    \
      TYPE tmp = TYPE(v);                                                      \
      s.write(&tmp, sizeof(tmp));                                              \
    }                                                                          \
    template <typename S> static void read(S &s, ENUM &v) {                    \
      TYPE tmp;                                                                \
      s.read(&tmp, sizeof(tmp));                                               \
      v = ENUM(tmp);                                                           \
    }                                                                          \
    static ENUM get(ENUM v) { return v; }                                      \
    static std::string to_json(store_type v) { return detail::to_json((unsigned int)v);} \
  };

#define CZRPC_DEFINE_PARAMTRAITS_ENUM_8(TYPE) \
	CZRPC_HELPER_ENUM_DEFINE(TYPE, uint8_t)

#define CZRPC_DEFINE_PARAMTRAITS_ENUM_16(TYPE) \
	CZRPC_HELPER_ENUM_DEFINE(TYPE, uint16_t)

#define CZRPC_DEFINE_PARAMTRAITS_ENUM_32(TYPE) \
	CZRPC_HELPER_ENUM_DEFINE(TYPE, uint32_t)

//
// std::string and const char*
//
namespace details {
struct StringTraits {
    template <typename S>
    static void write(S& s, const char* v) {
        int len = static_cast<int>(strlen(v));
        s.write(&len, sizeof(len));
        s.write(v, len);
    }

    template <typename S>
    static void write(S& s, const std::string& v) {
        int len = static_cast<int>(v.size());
        s.write(&len, sizeof(len));
        s.write(v.c_str(), len);
    }

    template <typename S>
    static void read(S& s, std::string& v) {
        int len;
        s.read(&len, sizeof(len));

		auto r = s.template readRange<char>(len);
		v = std::string(r.first, r.second);

		/*
		v = std::string(len, 0);
		if (len)
			s.read(&v[0], len);
			*/
    }
};
}

// const char*
template <>
struct ParamTraits<const char*> {
    using store_type = std::string;
    static constexpr bool valid = true;

    template <typename S>
    static void write(S& s, const char* v) {
        details::StringTraits::write(s, v);
    }

    template <typename S>
    static void read(S& s, std::string& v) {
        details::StringTraits::read(s, v);
    }

    static const char* get(const std::string& v) {
        return v.c_str();
    }
};

template <int N>
struct ParamTraits<char[N]> : ParamTraits<const char*> {};
template <int N>
struct ParamTraits<const char[N]> : ParamTraits<const char*> {};
template <int N>
struct ParamTraits<const char (&)[N]> : ParamTraits<const char*> {};

// std::string
template <>
struct ParamTraits<std::string> {
	using store_type = std::string;
	static constexpr bool valid = true;
	template <typename S>
	static void write(S& s, const char* v) {
		details::StringTraits::write(s, v);
	}

	template <typename S>
	static void write(S& s, const std::string& v) {
		details::StringTraits::write(s, v);
	}

	template <typename S>
	static void read(S& s, std::string& v) {
		details::StringTraits::read(s, v);
	}

	static std::string&& get(std::string&& v)
	{
		return std::move(v);
	}

	static std::string to_json(const std::string& v)
	{
		return detail::to_json(v);
	}
};

// std::pair
template <typename FIRST, typename SECOND>
struct ParamTraits<std::pair<FIRST,SECOND>>
{
	static constexpr bool valid = ParamTraits<FIRST>::valid && ParamTraits<SECOND>::valid;
	static_assert(
		valid == true,
		"Invalid std::pair type. Check if FIRST and SECOND are both valid rpc parameters");
	using store_type = std::pair<FIRST, SECOND>;
    template <typename S>
    static void write(S& s, const store_type& v) {
		s << v.first;
		s << v.second;
    }

    template <typename S>
    static void read(S& s, store_type& v) {
		s >> v.first;
		s >> v.second;
    }

	static store_type&& get(store_type&& v)
	{
		return std::move(v);
	}

	static std::string to_json(const store_type& v)
	{
		return detail::to_json(v);
	}
};

//
// std::vector
//
template <typename T>
struct ParamTraits<
	std::vector<T>,
	typename std::enable_if_t<std::is_arithmetic<T>::value==false>
	>
{
	using store_type = std::vector<T>;
	static constexpr bool valid = ParamTraits<T>::valid;
	static_assert(ParamTraits<T>::valid == true, "T is not valid RPC parameter type.");

	// std::vector serialization is done by writing the vector size, followed by  each element
	template <typename S>
	static void write(S& s, const std::vector<T>& v)
	{
		int len = static_cast<int>(v.size());
		s.write(&len, sizeof(len));
		for (auto&& i : v) ParamTraits<T>::write(s, i);
	}

	template <typename S>
	static void read(S& s, std::vector<T>& v)
	{
		int len;
		s.read(&len, sizeof(len));
		v.clear();
		v.reserve(len);
		while (len--)
		{
			T i;
			ParamTraits<T>::read(s, i);
			v.push_back(std::move(i));
		}
	}

	static std::vector<T>&& get(std::vector<T>&& v) { return std::move(v); }

	static std::string to_json(const store_type& v)
	{
		return detail::to_json(v);
	}

};

//
// Optimization for std::vector where T is an arithmetic
//
template <typename T>
struct ParamTraits<
	std::vector<T>,
	typename std::enable_if_t<std::is_arithmetic<T>::value>
	>
{
	using store_type = std::vector<T>;
	static constexpr bool valid = ParamTraits<T>::valid;
	static_assert(ParamTraits<T>::valid == true, "T is not valid RPC parameter type.");

	// std::vector serialization is done by writing the vector size, followed by  each element
	template <typename S>
	static void write(S& s, const std::vector<T>& v)
	{
		int len = static_cast<int>(v.size());
		s.write(&len, sizeof(len));
		if (len)
			s.write(&v[0], len*sizeof(T));
	}

	template <typename S>
	static void read(S& s, std::vector<T>& v)
	{
		int len;
		s.read(&len, sizeof(len));
		auto r = s.template readRange<T>(len);
		v = std::vector<T>(r.first, r.second);
	}

	static std::vector<T>&& get(std::vector<T>&& v) { return std::move(v); }

	static std::string to_json(const store_type& v)
	{
		return detail::to_json(v);
	}
};

//
// Validate if all parameter types in a parameter pack can be used for RPC
// calls
//
template <typename... T>
struct ParamPack {
    static constexpr bool valid = true;
};

template <typename First>
struct ParamPack<First> {
    static constexpr bool valid = ParamTraits<First>::valid;
};

template <typename First, typename... Rest>
struct ParamPack<First, Rest...> {
    static constexpr bool valid =
        ParamTraits<First>::valid && ParamPack<Rest...>::valid;
};

//
// std::tuple
//
namespace details
{
template <typename T, bool Done, int N>
struct Tuple
{
	template <typename S>
	static void deserialize(S& s, T& v)
	{
		s >> std::get<N>(v);
		Tuple<T, N == std::tuple_size<T>::value - 1, N + 1>::deserialize(s, v);
	}

	template <typename S>
	static void serialize(S& s, const T& v)
	{
		s << std::get<N>(v);
		Tuple<T, N == std::tuple_size<T>::value - 1, N + 1>::serialize(s, v);
	}
};

template <typename T, int N>
struct Tuple<T, true, N>
{
	template <typename S>
	static void deserialize(S&, T&)
	{
	}
	template <typename S>
	static void serialize(S&, const T&)
	{
	}
};
}  // namespace details

template <typename... T>
struct ParamTraits<std::tuple<T...>>
{
	// for internal use
	using tuple_type = std::tuple<T...>;

	using store_type = tuple_type;
	static constexpr bool valid = ParamPack<T...>::valid;

	static_assert(ParamPack<T...>::valid == true, "One or more tuple elements is not a valid RPC parameter type.");

	template <typename S>
	static void write(S& s, const tuple_type& v)
	{
		details::Tuple<tuple_type, std::tuple_size<tuple_type>::value == 0, 0>::serialize(s, v);
	}

	template <typename S>
	static void read(S& s, tuple_type& v)
	{
		details::Tuple<tuple_type, std::tuple_size<tuple_type>::value == 0, 0>::deserialize(s, v);
	}

	static tuple_type&& get(tuple_type&& v) { return std::move(v); }

	static std::string to_json(const store_type& v)
	{
		return detail::to_json(v);
	}
};

}  // namespace rpc
}  // namespace cz
