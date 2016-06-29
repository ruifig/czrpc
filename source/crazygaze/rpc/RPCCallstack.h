/********************************************************************
	CrazyGaze (http://www.crazygaze.com)
	Author : Rui Figueira
	Email  : rui@crazygaze.com
	
	purpose:
	Allows adding key:value pair markers to the current callstack.
	This allows for example to check if an instance/function is already being executed
	Very similar and inspired by boost::asio::detail::call_stack,
	with the addition that we can iterate the call stack with a range based for
	Implementation based on http://www.crazygaze.com/blog/2016/03/11/callstack-markers-boostasiodetailcall_stack/
*********************************************************************/

#pragma once

namespace cz
{
namespace rpc
{

template<typename Key, typename Value = unsigned char>
class Callstack
{
public:
	class Iterator;

	class Context
	{
	public:
		Context(const Context&) = delete;
		Context& operator=(const Context&) = delete;
		explicit Context(Key* k)
			: m_key(k)
			, m_next(Callstack<Key,Value>::ms_top)
		{
			m_val = reinterpret_cast<unsigned char*>(this);
			Callstack<Key, Value>::ms_top = this;
		}

		Context(Key* k, Value& v)
			: m_key(k)
			, m_val(&v)
			, m_next(Callstack<Key, Value>::ms_top)
		{
			Callstack<Key, Value>::ms_top = this;
		}

		~Context()
		{
			Callstack<Key,Value>::ms_top = m_next;
		}

		Key* getKey()
		{
			return m_key;
		}

		Value* getValue()
		{
			return m_val;
		}
	private:
		friend class Callstack<Key, Value>;
		friend class Callstack<Key, Value>::Iterator;
		Key* m_key;
		Value* m_val;
		Context* m_next;
	};

	class Iterator
	{
	public:
		Iterator(Context* ctx) : m_ctx(ctx) {}
		Iterator& operator++()
		{
			if (m_ctx)
				m_ctx = m_ctx->m_next;
			return *this;
		}

		bool operator != (const Iterator& other)
		{
			return m_ctx != other.m_ctx;
		}

		Context* operator*()
		{
			return m_ctx;
		}

	private:
		Context* m_ctx;
	};

	// Determine if the specified owner is on the stack
	// \return
	//	The address of the value if present, nullptr if not present
	static Value* contains(const Key* k)
	{
		Context* elem = ms_top;
		while(elem)
		{
			if (elem->m_key == k)
				return elem->m_val;
			elem = elem->m_next;
		}
		return nullptr;
	}

	static Iterator begin()
	{
		return Iterator(ms_top);
	}

	static Iterator end()
	{
		return Iterator(nullptr);
	}

private:
	static thread_local Context* ms_top;
};

template<typename Key, typename Value>
thread_local typename Callstack<Key, Value>::Context* Callstack<Key, Value>::ms_top = nullptr;

} // namespace rpc
} // namespace cz

