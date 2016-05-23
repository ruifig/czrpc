#pragma once


namespace cz {
namespace rpc {

class Stream
{
public:

	Stream() { }
	explicit Stream(std::vector<char> data)
		: m_buf(std::move(data)) { }

	void clear()
	{
		m_buf.clear();
		m_readpos = 0;
	}

	void write(const void* src, int size)
	{
		auto p = reinterpret_cast<const char*>(src);
		m_buf.insert(m_buf.end(), p, p + size);
	}

	void read(void* dst, int size)
	{
		auto p = reinterpret_cast<char*>(dst);
		assert(static_cast<int>(m_buf.size()) - m_readpos >= size);
		memcpy(p, &m_buf[m_readpos], size);
		m_readpos += size;
	}

	int readSize() const
	{
		return static_cast<int>(m_buf.size()) - m_readpos;
	}

	int writeSize() const
	{
		return static_cast<int>(m_buf.size());
	}

	std::vector<char> extract()
	{
		m_readpos = 0;
		return std::move(m_buf);
	}

	char* ptr(int index)
	{
		return &m_buf[index];
	};

private:
	std::vector<char> m_buf;
	int m_readpos = 0;
};

template <typename T>
Stream& operator<<(Stream& s, const T& v) {
	ParamTraits<T>::write(s, v);
    return s;
}

template <typename T>
Stream& operator>>(Stream& s, T& v) {
	ParamTraits<T>::read(s, v);
    return s;
}

namespace details
{
	template<typename T, bool Done, int N>
	struct Tuple
	{
		template<typename S>
		static void deserialize(S& s, T& v)
		{
			s >> std::get<N>(v);
			Tuple<T, N == std::tuple_size<T>::value - 1, N + 1>::deserialize(s, v);
		}
		template<typename S>
		static void serialize(S& s, const T& v)
		{
			s << std::get<N>(v);
			Tuple<T, N == std::tuple_size<T>::value - 1, N + 1>::serialize(s, v);
		}
	};

	template<typename T, int N>
	struct Tuple<T, true, N>
	{
		template<typename S>
		static void deserialize(S&, T&) { }
		template<typename S>
		static void serialize(S&, const T&) {}
	};

	struct ParameterPack
	{
		template<typename S>
		static void serialize(S&) { }
		template<typename S, typename First, typename... Rest>
		static void serialize(S& s, First&& first, Rest&&... rest)
		{
			s << first;
			serialize(s, rest...);
		}
	};
}

template<typename... Elements>
Stream& operator >> (Stream& s, std::tuple<Elements...>& v)
{
	using TupleType = std::tuple<Elements...>;
	details::Tuple<TupleType, std::tuple_size<TupleType>::value == 0, 0>::deserialize(s, v);
	return s;
}

template<typename B, typename... Elements>
Stream& operator << (Stream& s, const std::tuple<Elements...>& v)
{
	using TupleType = std::tuple<Elements...>;
	details::Tuple<TupleType, std::tuple_size<TupleType>::value == 0, 0>::serialize(s, v);
	return s;
}

template<typename B, typename... Args>
void serializeParameterPack(Stream& s, Args&&... args)
{
	details::ParameterPack::serialize(s, args...);
}

}  // namespace rpc
}  // namespace cz
