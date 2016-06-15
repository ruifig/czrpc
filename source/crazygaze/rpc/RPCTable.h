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
	bool isGenericRPC() const { return bits.rpcid == 0; }

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

struct Send
{
	static void error(Transport& trp, Header hdr, const char* what)
	{
		Stream o;
		o << hdr; // reserve space for the header
		o << what;
		hdr.bits.isReply = true;
		hdr.bits.success = false;
		hdr.bits.size = o.writeSize();
		*reinterpret_cast<Header*>(o.ptr(0)) = hdr;
		trp.send(o.extract());
	}

	static void result(Transport& trp, Header hdr, Stream& o)
	{
		hdr.bits.isReply = true;
		hdr.bits.success = true;
		hdr.bits.size = o.writeSize();
		*reinterpret_cast<Header*>(o.ptr(0)) = hdr;
		trp.send(o.extract());
	}
};

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
		static void doCall(OBJ& obj, F f, P&& params, Stream& out, Header hdr)
		{
			auto r = callMethod(obj, f, std::move(params));
			if (hdr.isGenericRPC())
				out << Any(r);
			else
				out << r;
		}
	};

	template <>
	struct Caller<void>
	{
		template <typename OBJ, typename F, typename P>
		static void doCall(OBJ& obj, F f, P&& params, Stream& out, Header hdr)
		{
			callMethod(obj, f, std::move(params));
			if (hdr.isGenericRPC())
				out << Any();
		}
	};

	template <typename OBJ, typename F, typename P>
	static void impl(OBJ& obj, F f, P&& params, ResultOutput& out, Transport& trp, Header hdr)
	{
#if CZRPC_CATCH_EXCEPTIONS
		try {
#endif
			Stream o;
			o << hdr; // Reserve space for the header
			Caller<R>::doCall(obj, std::move(f), std::move(params), o, hdr);
			Send::result(trp, hdr, o);
#if CZRPC_CATCH_EXCEPTIONS
		}
		catch (std::exception& e)
		{
			Send::error(trp, hdr, e.what());
		}
#endif
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
		try
		{
			Stream o;
			o << hdr;
			auto r = ft.get();
			if (hdr.isGenericRPC())
				o << Any(r);
			else
				o << r;
			Send::result(trp, hdr, o);
		}
		catch (const std::exception& e)
		{
			Send::error(trp, hdr, e.what());
		}

		// Delete previously finished futures, and prepare to delete this one.
		// We can't delete this one right here, because it will deadlock.
		out.pending([&](ResultOutput::PendingFutures& pending)
		{
			pending.done.clear();
			auto it = pending.futures.find(counter);
			assert(it != pending.futures.end());
			pending.done.push_back(std::move(it->second));
			pending.futures.erase(it);
		});

	}
};

}

struct BaseInfo
{
	BaseInfo() {}
	virtual ~BaseInfo(){};
	std::string name;
};

class BaseTable
{
  public:
	BaseTable() {}
	virtual ~BaseTable() {}
	bool isValid(uint32_t rpcid) const { return rpcid < m_rpcs.size(); }
  protected:
	std::vector<std::unique_ptr<BaseInfo>> m_rpcs;
};

template <typename T>
class TableImpl : public BaseTable
{
  public:
	using Type = T;

	struct Info : public BaseInfo
	{
		std::function<void(Type&, Stream& in, ResultOutput& out, Transport& trp, Header hdr)> dispatcher;
	};


	Info* getByName(const std::string& name)
	{
		for(auto&& info : m_rpcs)
		{
			if (info->name == name)
				return static_cast<Info*>(info.get());
		}
		return nullptr;
	}

	void registerGenericRPC()
	{
		// Generic RPC needs to have ID 0
		assert(m_rpcs.size() == 0);
		auto info = std::make_unique<Info>();
		info->name = "genericRPC";
		info->dispatcher = [this](Type& obj, Stream& in, ResultOutput& out, Transport& trp, Header hdr) {
			assert(hdr.isGenericRPC());
			std::string name;
			in >> name;
			Info* info = getByName(name);
			if (!info)
			{
				details::Send::error(trp, hdr, "Generic RPC not found");
				return;
			}

			info->dispatcher(obj, in, out, trp, hdr);
		};
		m_rpcs.push_back(std::move(info));
	}

	template <typename F>
	void registerRPC(uint32_t rpcid, const char* name, F f)
	{
		assert(rpcid == m_rpcs.size());
		auto info = std::make_unique<Info>();
		info->name = name;
		info->dispatcher = [f](Type& obj, Stream& in, ResultOutput& out, Transport& trp, Header hdr) {
			using Traits = FunctionTraits<F>;
			typename Traits::param_tuple params;

			if (hdr.isGenericRPC())
			{
				std::vector<Any> a;
				in >> a;
				if (!toTuple(a, params))
				{
					details::Send::error(trp, hdr, "Invalid parameters for generic RPC");
					return;
				}
			}
			else
			{
				in >> params;
			}

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
