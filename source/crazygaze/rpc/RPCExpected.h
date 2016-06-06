//
// Based on expected<T> by Andrei Alexandrescu (https://en.wikipedia.org/wiki/Andrei_Alexandrescu)
//
#pragma once

namespace cz
{
namespace rpc
{

#pragma warning(push)
// destructor was implicitly defined as deleted because a base class destructor is inaccessible or deleted
#pragma warning(disable : 4624)

template <typename>
class Expected;

template <typename>
class Expected_common;

template <typename T>
class Expected_traits
{
	friend class Expected_common<T>;

	typedef T storage;
	typedef T value;
	typedef T* pointer;
	typedef T& reference;
};  // Expected_traits

template <typename T>
class Expected_traits<T&>
{
	friend class Expected_common<T&>;

	typedef std::reference_wrapper<T> storage;
	typedef T value;
	typedef T* pointer;
	typedef T& reference;
};  // Expected_traits<T&>

template <typename T>
class Expected_traits<const T&>
{
	friend class Expected_common<const T&>;

	typedef std::reference_wrapper<const T> storage;
	typedef T value;
	typedef const T* pointer;
	typedef const T& reference;
};  // Expected_traits<const T&>

template <typename T>
class Expected_common
{
	friend class Expected<T>;

	typedef typename Expected_traits<T>::value value;
	typedef typename Expected_traits<T>::storage storage;
	typedef typename Expected_traits<T>::pointer pointer;
	typedef typename Expected_traits<T>::reference reference;

	Expected_common(){};

	Expected_common(const value& value) : m_valid(true), m_storage(value){};

	Expected_common(value& value) : m_valid(true), m_storage(value){};

	Expected_common(value&& value) : m_valid(true), m_storage(std::move(value)){};

	template <typename... AA>
	explicit Expected_common(AA&&... arguments)
		: m_valid(true), m_storage(std::forward<AA>(arguments)...){};

	Expected_common(const Expected_common& other) : m_valid(other.m_valid)
	{
		if (m_valid)
			new (&m_storage) storage(other.m_storage);
		else
			new (&m_exception) std::exception_ptr(other.m_exception);
	};

	Expected_common(Expected_common&& other) : m_valid(other.m_valid)
	{
		if (m_valid)
			new (&m_storage) storage(std::move(other.m_storage));
		else
			new (&m_exception) std::exception_ptr(std::move(other.m_exception));
	};

	~Expected_common()
	{
		if (m_valid)
			m_storage.~storage();
		else
			m_exception.~exception_ptr();
	};

	void swap(Expected_common& other)
	{
		if (m_valid)
		{
			if (other.m_valid)
				std::swap(m_storage, other.m_storage);
			else
			{
				auto exception = std::move(other.m_exception);
				new (&other.m_storage) storage(std::move(m_storage));
				new (&m_exception) std::exception_ptr(exception);
			}
		}
		else if (other.m_valid)
			other.swap(*this);
		else
			m_exception.swap(other.m_exception);
	};

  public:
	template <class E>
	static Expected<T> from_exception(const E& exception)
	{
		if (typeid(exception) != typeid(E))
			throw std::invalid_argument("slicing detected");
		return from_exception(std::make_exception_ptr(exception));
	}
	static Expected<T> from_exception(std::exception_ptr exception)
	{
		Expected<T> result;
		result.m_valid = false;
		new (&result.m_exception) std::exception_ptr(exception);
		return result;
	};
	static Expected<T> from_exception() { return from_exception(std::current_exception()); };
	template <class F, typename... AA>
	static Expected<T> from_code(F function, AA&&... arguments)
	{
		try
		{
			return Expected<T>(function(std::forward<AA>(arguments)...));
		}
		catch (...)
		{
			return from_exception();
		};
	};
	operator bool() const { return m_valid; };
	template <class E>
	bool exception_is() const
	{
		try
		{
			if (!m_valid)
				std::rethrow_exception(m_exception);
		}
		catch (const E&)
		{
			return true;
		}
		catch (...)
		{
		};
		return false;
	};

  private:
	reference get()
	{
		if (!m_valid)
			std::rethrow_exception(m_exception);
		return m_storage;
	};
	const reference get() const
	{
		if (!m_valid)
			std::rethrow_exception(m_exception);
		return m_storage;
	};

  public:
	reference operator*() { return get(); };
	const reference operator*() const { return get(); };
	pointer operator->() { return &get(); };
	const pointer operator->() const { return &get(); };
  private:
	bool m_valid;
	union
	{
		storage m_storage;
		std::exception_ptr m_exception;
	};
};  // Expected_common

template <typename T>
class Expected : public Expected_common<T>
{
	friend class Expected_common<T>;
	typedef Expected_common<T> common;
	Expected(){};

  public:
	template <class... AA>
	Expected(AA&&... arguments)
		: common(std::forward<AA>(arguments)...){};
	Expected(const T& value) : common(value){};
	Expected(T&& value) : common(value){};
	Expected(const Expected& other) : common(static_cast<const common&>(other)){};
	Expected(Expected&& other) : common(static_cast<common&&>(other)){};
	void swap(Expected& other) { common::swap(static_cast<common&>(other)); };
};  // Expected

template <typename T>
class Expected<T&> : public Expected_common<T&>
{
	friend class Expected_common<T&>;
	typedef Expected_common<T&> common;
	Expected(){};

  public:
	Expected(T& value) : common(value){};
	Expected(const Expected& other) : common(static_cast<const common&>(other)){};
	Expected(Expected&& other) : common(static_cast<common&&>(other)){};
	void swap(Expected& other) { common::swap(static_cast<common&>(other)); };
};  // Expected<T&>
template <typename T>
class Expected<const T&> : public Expected_common<const T&>
{
	friend class Expected_common<const T&>;
	typedef Expected_common<const T&> common;
	Expected(){};

  public:
	Expected(const T& value) : common(value){};
	Expected(const Expected& other) : common(static_cast<const common&>(other)){};
	Expected(Expected&& other) : common(static_cast<common&&>(other)){};
	void swap(Expected& other) { common::swap(static_cast<common&>(other)); };
};  // Expected<const T&>

//
// Specialization for void, that enforces checking
//
template <>
class Expected<void>
{
	mutable bool m_read;
	std::exception_ptr m_exception;

  public:
	Expected() : m_read(false), m_exception() {}
	Expected(const Expected& other) : m_read(other.m_read), m_exception(other.m_exception) {}
	Expected(Expected&& other) : m_read(other.m_read), m_exception(std::move(other.m_exception)) { other.m_read = true; }
	~Expected() { assert(m_read); }

	Expected<void>& operator=(const Expected&& other)
	{
		get(); // If we have an exception, make sure to make some noise, since we will lose that information
		m_read = other.m_read;
		m_exception = std::move(other.m_exception);
		other.m_read = true;
		return *this;
	}

	void suppress() { m_read = true; }
	bool valid() const
	{
		m_read = true;
		return !m_exception;
	}
	operator bool() const { return valid(); }
	void get() const
	{
		if (!valid())
			std::rethrow_exception(m_exception);
	}

	void swap(Expected& other)
	{
		std::swap(m_read, other.m_read);
		std::swap(m_exception, other.m_exception);
	}

	template <class E>
	static Expected<void> from_exception(const E& exception)
	{
		if (typeid(exception) != typeid(E))
			throw std::invalid_argument("slicing detected");
		return from_exception(std::make_exception_ptr(exception));
	}

	static Expected<void> from_exception(std::exception_ptr exception)
	{
		Expected<void> result;
		new (&result.m_exception) std::exception_ptr(exception);
		return result;
	};
	static Expected<void> from_exception() { return from_exception(std::current_exception()); };
	template <class F, typename... AA>
	static Expected<void> from_code(F func, AA&&... arguments)
	{
		static_assert(std::is_void<decltype(func(std::forward<AA>(arguments)...))>::value, "Function is not void");
		try
		{
			func(std::forward<AA>(arguments)...);
			return Expected<void>();
		}
		catch (...)
		{
			return from_exception();
		};
	};

};

template<typename T, typename E>
Expected<T> from_exception(const E& e)
{
	return Expected<T>::from_exception(e);
}

} // namespace rpc
} // namespace cz

#pragma warning(pop)
