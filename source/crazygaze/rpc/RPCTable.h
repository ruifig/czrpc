#pragma once

namespace cz
{
namespace rpc
{

//! Small utility struct to make it easier to work with the RPC headers
struct RPCHeader
{
	enum
	{
		kSizeBits = 32,
		kRPCIdBits = 8,
		kCounterBits = 22,
	};
	explicit RPCHeader()
	{
		static_assert(sizeof(*this) == sizeof(uint64_t), "Invalid size. Check the bitfields");
		all_ = 0;
	}

	struct Bits
	{
		uint32_t size : kSizeBits;
		unsigned counter : kCounterBits;
		unsigned rpcid : kRPCIdBits;
		unsigned isReply : 1; // Is it a reply to a RPC call ?
		unsigned success : 1; // Was the RPC call a success ?
	};

	uint32_t key() const { return (bits.counter << kRPCIdBits) | bits.rpcid; }
	union
	{
		Bits bits;
		uint64_t all_;
	};
};

inline Stream& operator<<(Stream& s, const RPCHeader& v)
{
	s << v.all_;
	return s;
}

inline Stream& operator >> (Stream& s, RPCHeader& v)
{
	s >> v.all_;
	return s;
}

struct BaseRPCInfo
{
	BaseRPCInfo() {}
	virtual ~BaseRPCInfo() {};
	std::string name;
};

namespace details
{
	template<typename R>
	struct Call
	{
		template<typename OBJ, typename F, typename P>
		static void impl(OBJ& obj, F f, P&& params, Stream& out)
		{
			out << callMethod(obj, f, std::move(params));
		}
	};

	template<>
	struct Call<void>
	{
		template<typename OBJ, typename F, typename P>
		static void impl(OBJ& obj, F f, P&& params, Stream& out)
		{
			callMethod(obj, f, std::move(params));
		}
	};
}

class BaseTable
{
public:
	BaseTable() {}
	virtual ~BaseTable() {}
	bool isValid(uint32_t rpcid) const { return rpcid < m_rpcs.size(); }
protected:
	std::vector<std::unique_ptr<BaseRPCInfo>> m_rpcs;
};

template<typename T>
class TableImpl : public BaseTable
{
public:
	using Type = T;

	struct RPCInfo : public BaseRPCInfo
	{
		std::function<void(Type&, Stream& in, Stream& out)> dispatcher;
	};

	template<typename F>
	void registerRPC(uint32_t rpcid, const char* name, F f)
	{
		assert(rpcid == m_rpcs.size());
		auto info = std::make_unique<RPCInfo>();
		info->name = name;
		info->dispatcher = [f](Type& obj, Stream& in, Stream& out)
		{
			using Traits = FunctionTraits<F>;
			typename Traits::param_tuple params;
			in >> params;
			using R = typename Traits::return_type;
			details::Call<R>::impl(obj, f, std::move(params), out);
		};
		m_rpcs.push_back(std::move(info));
	}

};

template <typename T>
class Table : public TableImpl<T>
{
	static_assert(sizeof(T) == 0, "RPC Table not specified for the type.");
};

} // namespace rpc
} // namespace cz

