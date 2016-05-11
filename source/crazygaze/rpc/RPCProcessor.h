#pragma once

namespace cz
{
namespace rpc
{

class BaseOutProcessor;

template<typename F>
class Call
{
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

	template<typename H>
	void async(H&& handler)
	{
		m_outer.commit<F, false>(m_transport, m_rpcid, m_data, std::forward<H>(handler));
	}

	// same as 'async' but allows to check for exceptions
	template<typename H>
	void asyncEx(H&& handler)
	{
		m_outer.commit<F,true>(m_transport, m_rpcid, m_data, std::forward<H>(handler));
	}

	// For RPCs that return something
	template<typename FR = typename FunctionTraits<F>::return_type>
	std::future<typename ParamTraits< typename std::enable_if<!std::is_void<FR>::value, FR>::type >::store_type>
	ft()
	{
		using R = typename ParamTraits<FR>::store_type;
		auto pr = std::make_shared<std::promise<R>>();
		auto ft = pr->get_future();
		async([pr=std::move(pr)](R&& res) 
		{
			pr->set_value(std::move(res));
		});

		return ft;
	}

	// For RPCs that don't return anything
	template<typename FR = typename FunctionTraits<F>::return_type>
	std::future<typename std::enable_if<std::is_void<FR>::value, FR>::type>
	ft()
	{
		auto pr = std::make_shared<std::promise<void>>();
		auto ft = pr->get_future();
		async([pr=std::move(pr)]() 
		{
			pr->set_value();
		});

		return ft;
	}

protected:

	template<typename T> friend class OutProcessor;

	explicit Call(BaseOutProcessor& outer, Transport& transport, uint32_t rpcid)
		: m_outer(outer), m_transport(transport), m_rpcid(rpcid)
	{
		m_data << uint64_t(0); // rpc data size + header
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
};

namespace details
{
	template<typename F, bool E, typename ENABLED = void> struct ReplyHandler;

	// non-void functions, no exception handling
	template< typename F>
	struct ReplyHandler<F, false, typename std::enable_if_t<!std::is_void<typename FunctionTraits<F>::return_type>::value> >
	{
		template<typename H>
		static void handle(Stream& in, RPCHeader hdr, H& handler)
		{
			using RetType = typename ParamTraits<typename FunctionTraits<F>::return_type>::store_type;
			if (hdr.bits.success)
			{
				RetType ret;
				in >> ret;
				handler(std::move(ret));
			}
			else
			{
				std::string str;
				in >> str;
				throw std::runtime_error(
					std::string("RPC returned an exception, and no exception handling was specified. Exception: ") +
					str);
			}
		}
	};
	// non-void functions, exception handling
	template< typename F>
	struct ReplyHandler<F, true, typename std::enable_if_t<!std::is_void<typename FunctionTraits<F>::return_type>::value> >
	{
		template<typename H>
		static void handle(Stream& in, RPCHeader hdr, H& handler)
		{
			using RetType = typename ParamTraits<typename FunctionTraits<F>::return_type>::store_type;
			if (hdr.bits.success)
			{
				RetType ret;
				in >> ret;
				handler(Expected<RetType>(std::move(ret)));
			}
			else
			{
				std::string str;
				in >> str;
				handler(Expected<RetType>::from_exception(std::runtime_error(str)));
			}
		}
	};

	// void functions, no exception handling
	template<typename F>
	struct ReplyHandler<F, false, typename std::enable_if_t<std::is_void<typename FunctionTraits<F>::return_type>::value>>
	{
		template<typename H>
		static void handle(Stream& in, RPCHeader hdr, H& handler)
		{
			if (hdr.bits.success)
			{
				handler();
			}
			else
			{
				std::string str;
				in >> str;
				throw std::runtime_error(
					std::string("RPC returned an exception, and no exception handling was specified. Exception: ") +
					str);
			}
		}
	};
	// void functions, exception handling
	template<typename F>
	struct ReplyHandler<F, true, typename std::enable_if_t<std::is_void<typename FunctionTraits<F>::return_type>::value>>
	{
		template<typename H>
		static void handle(Stream& in, RPCHeader hdr, H& handler)
		{
			if (hdr.bits.success)
			{
				handler(Expected<void>());
			}
			else
			{
				std::string str;
				in >> str;
				handler(Expected<void>::from_exception(std::runtime_error(str)));
			}
		}
	};

}

class BaseOutProcessor
{
public:
	virtual ~BaseOutProcessor() {}
protected:

	template<typename R> friend class Call;
	template<typename L, typename R> friend struct Connection;

	template<typename F, bool E, typename H>
	void commit(Transport& transport, uint32_t rpcid, Stream& data, H&& handler)
	{
		std::unique_lock<std::mutex> lk(m_mtx);
		RPCHeader hdr;
		hdr.bits.size = data.writeSize();
		hdr.bits.counter = ++m_replyIdCounter;
		hdr.bits.rpcid = rpcid;
		*reinterpret_cast<RPCHeader*>(data.ptr(0)) = hdr;
		m_replies[hdr.key()] = [handler = std::move(handler)](Stream& in, RPCHeader hdr)
		{
			details::ReplyHandler<F, E>::handle(in, hdr, handler);
		};
		lk.unlock();

		transport.send(data.extract());
	}

	void processReply(Stream& in, RPCHeader hdr)
	{
		std::function<void(Stream&, RPCHeader)> h;
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
	std::unordered_map<uint32_t, std::function<void(Stream&, RPCHeader)>> m_replies;
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

template<>
class OutProcessor<void>
{
public:
	OutProcessor() {}
	void processReply(Stream&, RPCHeader)
	{
		assert(0 && "incoming replies not allowed for OutProcessor<void>");
	}
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

	void processCall(Transport& transport, Stream& in, RPCHeader hdr)
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
			*reinterpret_cast<RPCHeader*>(out.ptr(0)) = hdr;
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
	void processCall(Transport&, Stream& , RPCHeader)
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

