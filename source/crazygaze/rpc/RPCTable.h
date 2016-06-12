#pragma once

namespace cz
{
namespace rpc
{

// Small utility struct to make it easier to work with the RPC headers
struct Header
{
	enum
	{
		kSizeBits = 32,
		kRPCIdBits = 8,
		kCounterBits = 22,
	};
	explicit Header()
	{
		static_assert(sizeof(*this) == sizeof(uint64_t), "Invalid size. Check the bitfields");
		all_ = 0;
	}

	struct Bits
	{
		unsigned size : kSizeBits;
		unsigned counter : kCounterBits;
		unsigned rpcid : kRPCIdBits;
		unsigned isReply : 1;  // Is it a reply to a RPC call ?
		unsigned success : 1;  // Was the RPC call a success ?
	};

	uint32_t key() const { return (bits.counter << kRPCIdBits) | bits.rpcid; }
	union {
		Bits bits;
		uint64_t all_;
	};
};

inline Stream& operator<<(Stream& s, const Header& v)
{
	s << v.all_;
	return s;
}

inline Stream& operator>>(Stream& s, Header& v)
{
	s >> v.all_;
	return s;
}

struct ResultOutput
{
	bool voidReplies = false;
	struct PendingFutures
	{
		unsigned counter = 0;
		std::unordered_map<unsigned, std::future<void>> futures;
		std::vector<std::future<void>> done;
	};
	Monitor<PendingFutures> pending;
};

//
// Helper code to dispatch a call.
namespace details
{

template <bool ASYNC,typename R>
struct Dispatcher {};


// Handle synchronous RPCs
template <typename R>
struct Dispatcher<false, R>
{

	template <typename R>
	struct Caller
	{
		template <typename OBJ, typename F, typename P>
		static void doCall(OBJ& obj, F f, P&& params, Stream& out)
		{
			out << callMethod(obj, f, std::move(params));
		}
	};

	template <>
	struct Caller<void>
	{
		template <typename OBJ, typename F, typename P>
		static void doCall(OBJ& obj, F f, P&& params, Stream& out)
		{
			callMethod(obj, f, std::move(params));
		}
	};

	template <typename OBJ, typename F, typename P>
	static void impl(OBJ& obj, F f, P&& params, ResultOutput& out, Transport& trp, Header hdr)
	{
		Stream o;
		hdr.bits.size = 0;
		hdr.bits.isReply = true;
		hdr.bits.success = true;
		o << hdr; // Reserve space for the header

#if CZRPC_CATCH_EXCEPTIONS
		try {
#endif
			Caller<R>::doCall(obj, std::move(f), std::move(params), o);
#if CZRPC_CATCH_EXCEPTIONS
		}
		catch (std::exception& e)
		{
			o.clear();
			o << hdr; // Reserve space for the header
			hdr.bits.success = false;
			o << e.what();
		}
#endif

		if ((o.writeSize() > sizeof(hdr)) || out.voidReplies)
		{
			// Update size
			hdr.bits.size = o.writeSize();
			*reinterpret_cast<Header*>(o.ptr(0)) = hdr;
			// send
			trp.send(o.extract());
		}
	}
};

// For functions return std::future
template <typename R>
struct Dispatcher<true, R>
{
	template <typename OBJ, typename F, typename P>
	static void impl(OBJ& obj, F f, P&& params, ResultOutput& out, Transport& trp, Header hdr)
	{
		using Traits = FunctionTraits<F>;
		auto resFt = callMethod(obj, f, std::move(params));
		out.pending([&](ResultOutput::PendingFutures& pending)
		{
			unsigned counter = pending.counter++;
			auto ft = then(std::move(resFt), [&out, &trp, hdr, counter](std::future<Traits::return_type> ft)
			{
				processReady(out, trp, counter, hdr, std::move(ft));
			});

			pending.futures.insert(std::make_pair(counter, std::move(ft)));
		});
	}

	template<typename T>
	static void processReady(ResultOutput& out, Transport& trp, unsigned counter, Header hdr, std::future<T> ft)
	{
		Stream o;
		hdr.bits.size = 0;
		hdr.bits.isReply = true;
		hdr.bits.success = true;
		try
		{
			o << hdr;
			o << ft.get();
		}
		catch (const std::exception& e)
		{
			o.clear();
			o << hdr; // Reserve space for the header
			hdr.bits.success = false;
			o << e.what();
		}

		// Delete previously finished futures, and prepare to delete this one
		// We can't delete this one right here, because it it deadlock.
		out.pending([&](ResultOutput::PendingFutures& pending)
		{
			pending.done.clear();
			auto it = pending.futures.find(counter);
			assert(it != pending.futures.end());
			pending.done.push_back(std::move(it->second));
			pending.futures.erase(it);
		});

		// Send the reply
		if (out.voidReplies || (o.writeSize()>sizeof(hdr)))
		{
			hdr.bits.size = o.writeSize();
			*reinterpret_cast<Header*>(o.ptr(0)) = hdr;
			trp.send(o.extract());
		}
	}
};

}

struct BaseRPCInfo
{
	BaseRPCInfo() {}
	virtual ~BaseRPCInfo(){};
	std::string name;
};

class BaseTable
{
  public:
	BaseTable() {}
	virtual ~BaseTable() {}
	bool isValid(uint32_t rpcid) const { return rpcid < m_rpcs.size(); }
  protected:
	std::vector<std::unique_ptr<BaseRPCInfo>> m_rpcs;
};

template <typename T>
class TableImpl : public BaseTable
{
  public:
	using Type = T;

	struct RPCInfo : public BaseRPCInfo
	{
		std::function<void(Type&, Stream& in, ResultOutput& out, Transport& trp, Header hdr)> dispatcher;
	};

	template <typename F>
	void registerRPC(uint32_t rpcid, const char* name, F f)
	{
		assert(rpcid == m_rpcs.size());
		auto info = std::make_unique<RPCInfo>();
		info->name = name;
		info->dispatcher = [f](Type& obj, Stream& in, ResultOutput& out, Transport& trp, Header hdr) {
			using Traits = FunctionTraits<F>;
			typename Traits::param_tuple params;
			in >> params;
			using R = typename Traits::return_type;
			details::Dispatcher<Traits::isasync, R>::impl(obj, f, std::move(params), out, trp, hdr);
		};
		m_rpcs.push_back(std::move(info));
	}
};

template <typename T>
class Table : public TableImpl<T>
{
	static_assert(sizeof(T) == 0, "RPC Table not specified for the type.");
};

}  // namespace rpc
}  // namespace cz
