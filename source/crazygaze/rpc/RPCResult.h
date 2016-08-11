#pragma once

namespace cz
{
namespace rpc
{

class Exception : public std::exception
{
public:
	Exception(const std::string& msg) : std::exception(msg.c_str()) {}
};

template<typename T>
class Result
{
public:
	using Type = T;

	Result() : m_state(State::Aborted) {}

	explicit Result(Type&& val)
		: m_state(State::Valid)
		, m_val(std::move(val))
	{
	}

	Result(Result&& other)
	{
		moveFrom(std::move(other));
	}

	Result(const Result& other)
	{
		copyFrom(other);
	}

	~Result()
	{
		destroy();
	}

	Result& operator=(Result&& other)
	{
		if (this == &other)
			return *this;
		destroy();
		moveFrom(std::move(other));
		return *this;
	}

	// Construction from an exception needs to be separate. so RPCReply<std::string> works.
	// Otherwise we would have no way to tell if constructing from a value, or from an exception
	static Result fromException(std::string ex)
	{
		Result r;
		r.m_state = State::Exception;
		new (&r.m_ex) std::string(std::move(ex));
		return r;
	}

	template<typename S>
	static Result fromStream(S& s)
	{
		Type v;
		s >> v;
		return Result(std::move(v));
	};

	bool isValid() const { return m_state == State::Valid; }
	bool isException() const { return m_state == State::Exception; };
	bool isAborted() const { return m_state == State::Aborted; }

	T& get()
	{
		if (!isValid())
			throw Exception(isException() ? m_ex : "RPC reply was aborted");
		return m_val;
	}

	const T& get() const
	{
		if (!isValid())
			throw Exception(isException() ? m_ex : "RPC reply was aborted");
		return m_val;
	}

	const std::string& getException()
	{
		assert(isException());
		return m_ex;
	};

private:

	void destroy()
	{
		if (m_state == State::Valid)
			m_val.~Type();
		else if (m_state == State::Exception)
		{
			using String = std::string;
			m_ex.~String();
		}
		m_state = State::Aborted;
	}

	void moveFrom(Result&& other)
	{
		m_state = other.m_state;
		if (m_state == State::Valid)
			new (&m_val) Type(std::move(other.m_val));
		else if (m_state == State::Exception)
			new (&m_ex) std::string(std::move(other.m_ex));
	}

	void copyFrom(const Result& other)
	{
		m_state = other.m_state;
		if (m_state == State::Valid)
			new (&m_val) Type(other.m_val);
		else if (m_state == State::Exception)
			new (&m_ex) std::string(other.m_ex);
	}

	enum class State { Valid, Aborted, Exception };

	State m_state;
	union
	{
		Type m_val;
		std::string m_ex;
	};
};

// void specialization
template<>
class Result<void>
{
public:
	Result() : m_state(State::Aborted) {}

	Result(Result&& other)
	{
		moveFrom(std::move(other));
	}

	Result(const Result& other)
	{
		copyFrom(other);
	}

	~Result()
	{
		destroy();
	}

	Result& operator=(Result&& other)
	{
		if (this == &other)
			return *this;
		destroy();
		moveFrom(std::move(other));
		return *this;
	}

	static Result fromException(std::string ex)
	{
		Result r;
		r.m_state = State::Exception;
		new (&r.m_ex) std::string(std::move(ex));
		return r;
	}

	template<typename S>
	static Result fromStream(S& s)
	{
		Result r;
		r.m_state = State::Valid;
		return r;
	}

	bool isValid() const { return m_state == State::Valid; }
	bool isException() const { return m_state == State::Exception; };
	bool isAborted() const { return m_state == State::Aborted; }

	const std::string& getException()
	{
		assert(isException());
		return m_ex;
	};

	void get() const
	{
		if (!isValid())
			throw Exception(isException() ? m_ex : "RPC reply was aborted");
	}
private:

	void destroy()
	{
		if (m_state == State::Exception)
		{
			using String = std::string;
			m_ex.~String();
		}
		m_state = State::Aborted;
	}

	void moveFrom(Result&& other)
	{
		m_state = other.m_state;
		if (m_state == State::Exception)
			new (&m_ex) std::string(std::move(other.m_ex));
	}

	void copyFrom(const Result& other)
	{
		m_state = other.m_state;
		if (m_state == State::Exception)
			new (&m_ex) std::string(other.m_ex);
	}

	enum class State { Valid, Aborted, Exception };

	State m_state;
	union
	{
		bool m_dummy;
		std::string m_ex;
	};
};

} // namespace rpc
} // namespace cz
