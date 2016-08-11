#pragma once

namespace cz
{
namespace rpc
{

template<typename T>
struct OutProcessor
{
	using Type = T;
	uint32_t replyIdCounter = 0;
	std::unordered_map<uint32_t, std::function<void(Stream*, Header)>> replies;

	template<typename F, typename H>
	void addReplyHandler(uint32_t key, H&& handler)
	{
		replies[key] = [handler = std::move(handler)](Stream* in, Header hdr)
		{
			using R = typename ParamTraits<typename FunctionTraits<F>::return_type>::store_type;
			if (in)
			{
				if (hdr.bits.success)
				{
					handler(Result<R>::fromStream((*in)));
				}
				else
				{
					std::string str;
					(*in) >> str;
					handler(Result<R>::fromException(std::move(str)));
				}
			}
			else
			{
				// if the stream is nullptr, it means the result is being aborted
				handler(Result<R>());
			}
		};
	}

	void processReply(Stream& in, Header hdr)
	{
		auto it = replies.find(hdr.key());
		assert(it != replies.end());
		std::function<void(Stream*,Header)> h = std::move(it->second);
		replies.erase(it);
		h(&in, hdr);
	}

	void abortReplies()
	{
		decltype(replies) tmp;
		tmp = std::move(replies);

		for (auto&& r : tmp)
		{
			r.second(nullptr, Header());
		}
	};
};

// Specialization for when there is no outgoing RPC calls
// If we have no outgoing RPC calls, receiving a reply is therefore an error.
template <>
class OutProcessor<void>
{
  public:
	OutProcessor() {}
	void processReply(Stream&, Header) { assert(0 && "Incoming replies not allowed for OutProcessor<void>"); }
	void abortReplies() {}
};

template<typename T>
struct InProcessor
{
	using Type = T;
	InProcessor(Type* obj)
		: obj(*obj)
		, data(obj)
	{
		data.authPassed = data.objData.getAuthToken() == "" ? true : false;
	}

	void processCall(Transport& transport, Stream& in, Header hdr)
	{
		auto&& info = Table<Type>::get(hdr.bits.rpcid);
		info->dispatcher(obj, in, data, transport, hdr);
	}

	Type& obj;
	InProcessorData data;
};

template<>
class InProcessor<void>
{
public:
	InProcessor(void*) { }
	void processCall(Transport& trp, Stream& in, Header hdr)
	{
		//assert(0 && "Incoming RPC not allowed for void local type");
		details::Send::error(trp, hdr, "Peer doesn't have an object to process RPC calls");
	}
};

#define CZRPC_CALL(con, func, ...)                                        \
    (con).call<decltype(&std::decay<decltype(con)>::type::Remote::func)>( \
        (uint32_t)cz::rpc::Table<                                         \
            std::decay<decltype(con)>::type::Remote>::RPCId::func,        \
        ##__VA_ARGS__)

#define CZRPC_CALLGENERIC(con, name, ...) \
	(con).callGeneric(name, ##__VA_ARGS__)

} // namespace rpc
} // namespace cz

