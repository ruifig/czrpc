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

class BaseOutProcessor;

template<typename F>
class Call
{
private:
	using RType = typename FunctionTraits<F>::return_type;
	using RTraits = typename ParamTraits<RType>;

public:

	Call(Call&& other)
		: m_outer(other.m_outer)
		, m_transport(other.m_transport)
		, m_rpcid(other.m_rpcid)
		, m_data(std::move(other.m_data))
	{
	}

	Call(const Call&) = delete;
	Call& operator=(const Call&) = delete;
	Call& operator=(Call&&) = delete;

	~Call()
	{
		if (m_data.writeSize() && !m_commited)
			async([](Reply<RTraits::store_type>&) {});
	}

	template<typename H>
	void async(H&& handler)
	{
		m_outer.commit<F>(m_transport, m_rpcid, m_data, std::forward<H>(handler));
		m_commited = true;
	}

	std::future<typename Reply<typename RTraits::store_type>> ft()
	{
		auto pr = std::make_shared<std::promise<Reply<RTraits::store_type>>>();
		auto ft = pr->get_future();
		async([pr=std::move(pr)](Reply<RTraits::store_type>&& res) 
		{
			pr->set_value(std::move(res));
		});

		return ft;
	}

protected:

	template<typename T> friend class OutProcessor;

	explicit Call(BaseOutProcessor& outer, Transport& transport, uint32_t rpcid)
		: m_outer(outer), m_transport(transport), m_rpcid(rpcid)
	{
		m_data << uint64_t(0); // rpc header
	}

	template<typename... Args>
	void serializeParams(Args&&... args)
	{
		serializeMethod<F>(m_data, std::forward<Args>(args)...);
	}

	BaseOutProcessor& m_outer;
	Transport& m_transport;
	uint32_t m_rpcid;
	Stream m_data;
	// Used in the destructor to do a commit with an empty handler if the rpc was not committed.
	bool m_commited = false;
};

class BaseOutProcessor
{
public:
	virtual ~BaseOutProcessor() {}
protected:

	template<typename R> friend class Call;
	template<typename L, typename R> friend struct Connection;

	template<typename F, typename H>
	void commit(Transport& transport, uint32_t rpcid, Stream& data, H&& handler)
	{
		std::unique_lock<std::mutex> lk(m_mtx);
		Header hdr;
		hdr.bits.size = data.writeSize();
		hdr.bits.counter = ++m_replyIdCounter;
		hdr.bits.rpcid = rpcid;
		*reinterpret_cast<Header*>(data.ptr(0)) = hdr;
		m_replies[hdr.key()] = [handler = std::move(handler)](Stream& in, Header hdr)
		{
			using R = typename ParamTraits<typename FunctionTraits<F>::return_type>::store_type;
			if (hdr.bits.success)
			{
				handler(Reply<R>::fromStream((in)));
			}
			else
			{
				std::string str;
				in >> str;
				handler(Reply<R>::fromException(std::move(str)));
			}
		};
		lk.unlock();

		transport.send(data.extract());
	}

	void processReply(Stream& in, Header hdr)
	{
		std::function<void(Stream&, Header)> h;
		{
			std::unique_lock<std::mutex> lk(m_mtx);
			auto it = m_replies.find(hdr.key());
			assert(it != m_replies.end());
			h = std::move(it->second);
			m_replies.erase(it);
		}

		h(in, hdr);
	}

	std::mutex m_mtx;
	uint32_t m_replyIdCounter = 0;
	std::unordered_map<uint32_t, std::function<void(Stream&, Header)>> m_replies;
};

template<typename T>
class OutProcessor : public BaseOutProcessor
{
public:
	using Type = T;

	template<typename F, typename... Args>
	auto call(Transport& transport, uint32_t rpcid, Args&&... args)
	{
		using Traits = FunctionTraits<F>;
		static_assert(
			std::is_member_function_pointer<F>::value &&
			std::is_base_of<typename Traits::class_type, Type>::value,
			"Not a member function of the wrapped class");
		Call<F> c(*this, transport, rpcid);
		c.serializeParams(std::forward<Args>(args)...);
		return std::move(c);
	}


protected:

};

// Specialization for when there is no outgoing RPC calls
// If we have no outgoing RPC calls, receiving a reply is therefore an error.
template <>
class OutProcessor<void>
{
  public:
	OutProcessor() {}
	void processReply(Stream&, Header) { assert(0 && "Incoming replies not allowed for OutProcessor<void>"); }
};

class BaseInProcessor
{
public:
	virtual ~BaseInProcessor() {}
};

template<typename T>
class InProcessor : public BaseInProcessor
{
public:
	using Type = T;
	InProcessor(Type* obj, bool doVoidReplies=true)
		: m_obj(*obj)
		, m_voidReplies(doVoidReplies)
	{
	}

	void processCall(Transport& transport, Stream& in, Header hdr)
	{
		Stream out;
		// Reuse the header as the header for the reply, so we keep the counter and rpcid
		hdr.bits.size = 0;
		hdr.bits.isReply = true;
		hdr.bits.success = true;

		auto&& info = Table<Type>::get(hdr.bits.rpcid);

#if CZRPC_CATCH_EXCEPTIONS
		try {
#endif
			out << hdr; // Reserve space for the header
			info->dispatcher(m_obj, in, out);
#if CZRPC_CATCH_EXCEPTIONS
		}
		catch (std::exception& e)
		{
			out.clear();
			out << hdr; // Reserve space for the header
			hdr.bits.success = false;
			out << e.what();
		}
#endif

		if (m_voidReplies || ( out.writeSize() > sizeof(hdr)))
		{
			hdr.bits.size = out.writeSize();
			*reinterpret_cast<Header*>(out.ptr(0)) = hdr;
			transport.send(out.extract());
		}
	}
protected:
	Type& m_obj;
	bool m_voidReplies = false;
};

template<>
class InProcessor<void>
{
public:
	InProcessor(void*) { }
	void processCall(Transport&, Stream& , Header)
	{
		assert(0 && "Incoming RPC not allowed for void local type");
	}
};

#define CZRPC_CALL(con, func, ...)                                        \
    (con).call<decltype(&std::decay<decltype(con)>::type::Remote::func)>( \
        *(con).transport,                                                 \
        (uint32_t)cz::rpc::Table<                                         \
            std::decay<decltype(con)>::type::Remote>::RPCId::func,        \
        ##__VA_ARGS__)

} // namespace rpc
} // namespace cz

