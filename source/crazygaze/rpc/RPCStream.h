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

	// To allow for faster reading of (for example) std::string and std::vector<T>, where T is arithmetic,
	// we can do a fake read that returns a range.
	// This is to allow passing a range to initialize std::string/std::vector
	template<typename T>
	std::pair<T*,T*> readRange(int count)
	{
		static_assert(std::is_arithmetic<T>::value, "T needs to be arithmetic");
		int  size = sizeof(T)*count;
		assert(static_cast<int>(m_buf.size()) - m_readpos >= size);
		auto res = std::make_pair(
			reinterpret_cast<T*>(m_buf.data() + m_readpos),
			reinterpret_cast<T*>(m_buf.data() + m_readpos) + count);
		m_readpos += size;
		return res;
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

//
// StreamDirection and StreamWrapper is an experimental way to allow the user to define only one function that
// specifies the fields to serialize for a given type, so there is no need to create both the ParamTraits<T>::write and
// ParamTraits<T>::read.
// This might be removed in the future
enum StreamDirection
{
	Read,
	Write
};

template<StreamDirection D>
struct StreamWrapper { };

template<>
struct StreamWrapper<Read>
{ 
	StreamWrapper(Stream& s) : s(s) {}
	template<typename T>
	inline void op(T& v)
	{
		ParamTraits<T>::read(s, v);
	}
	Stream& s;
};

template<>
struct StreamWrapper<Write>
{ 
	StreamWrapper(Stream& s) : s(s) {}
	template<typename T>
	inline void op(const T& v)
	{
		ParamTraits<T>::write(s, v);
	}
	Stream& s;
};

template <typename T>
Stream& operator<<(Stream& s, const T& v) {
	ParamTraits<T>::write(s, v);
    return s;
}

template <typename T>
inline Stream& operator>>(Stream& s, T& v) {
	ParamTraits<T>::read(s, v);
    return s;
}

template<StreamDirection D, typename T>
inline StreamWrapper<D>& operator^(StreamWrapper<D>& s, T& v)
{
	s.op(v);
	return s;
}

}  // namespace rpc
}  // namespace cz
