#pragma once

namespace cz
{
namespace rpc
{

#pragma warning(push)
// destructor was implicitly defined as deleted because a base class destructor is inaccessible or deleted
#pragma warning(disable : 4624)

template<typename T>
class Reply
{
public:
	using Type = T;

	Reply() : m_state(State::Aborted) {}

	explicit Reply(Type&& val)
		: m_state(State::Valid)
		, m_val(std::move(val))
	{
	}

	Reply(Reply&& other)
	{
		moveFrom(std::move(other));
	}

	Reply(const Reply& other)
	{
		copyFrom(other);
	}

	~Reply()
	{
		destroy();
	}

	Reply& operator=(Reply&& other)
	{
		if (this == &other)
			return *this;
		destroy();
		moveFrom(std::move(other));
		return *this;
	}

	// Construction from an exception needs to be separate. so RPCReply<std::string> works.po
	// Otherwise we would have no way to tell if constructing from a value, or from an exception
	static Reply fromException(std::string ex)
	{
		Reply r;
		r.m_state = State::Exception;
		new (&r.m_ex) std::string(std::move(ex));
		return r;
	}

	template<typename S>
	static Reply fromStream(S& s)
	{
		Type v;
		s >> v;
		return Reply(std::move(v));
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

	void moveFrom(Reply&& other)
	{
		m_state = other.m_state;
		if (m_state == State::Valid)
			new (&m_val) Type(std::move(other.m_val));
		else if (m_state == State::Exception)
			new (&m_ex) std::string(std::move(other.m_ex));
	}

	void copyFrom(const Reply& other)
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

template<>
class Reply<void>
{
public:
	Reply() : m_state(State::Aborted) {}

	Reply(Reply&& other)
		: m_state(other.m_state)
	{
		if (m_state == State::Exception)
			new (&m_ex) std::string(std::move(other.m_ex));
	}

	Reply(const Reply& other)
		: m_state(other.m_state)
	{
		if (m_state == State::Exception)
			new (&m_ex) std::string(other.m_ex);
	}

	~Reply()
	{
		if (m_state == State::Exception)
		{
			using String = std::string;
			m_ex.~String();
		}
	}

	// Construction from an exception needs to be separate. so RPCReply<std::string> works.
	// Otherwise we would have no way to tell if constructing from a value, or from an exception
	static Reply fromException(std::string ex)
	{
		Reply r;
		r.m_state = State::Exception;
		new (&r.m_ex) std::string(std::move(ex));
		return r;
	}

	template<typename S>
	static Reply fromStream(S& s)
	{
		Reply r;
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

private:

	enum class State { Valid, Aborted, Exception };

	State m_state;
	union
	{
		bool m_dummy;
		std::string m_ex;
	};
};

#pragma warning(pop)

}
}
