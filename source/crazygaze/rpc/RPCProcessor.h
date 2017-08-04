#pragma once

namespace cz
{
namespace rpc
{

namespace
{
	struct ReplyInfo
	{
		std::function<void(Stream*, Header, DebugInfo*)> h;
		// Pointer, so it doesn't take any memory if the RPC call doesn't have debug information
		std::unique_ptr<DebugInfo> dbg;
	};
}
template<typename T>
struct OutProcessor
{
	using Type = T;
	uint32_t replyIdCounter = 0;
	std::unordered_map<uint32_t, ReplyInfo> replies;

	template<typename F, typename H>
	void addReplyHandler(uint32_t key, H&& handler, DebugInfo* dbg)
	{
		auto& r = replies[key];
		r.h = [handler = std::move(handler)](Stream* in, Header hdr, DebugInfo* dbg)
		{
			using R = typename ParamTraits<typename FunctionTraits<F>::return_type>::store_type;
			if (in)
			{
				if (hdr.bits.success)
				{
					if (dbg)
					{
						CZRPC_LOG(Log, CZRPC_LOGSTR_RESULT "size=%u, success", dbg->num, in->writeSize());
					}
					handler(Result<R>::fromStream((*in)));
				}
				else
				{
					std::string str;
					(*in) >> str;
					if (dbg)
					{
						CZRPC_LOG(Log, CZRPC_LOGSTR_RESULT "size=%u, exception=%s", dbg->num, in->writeSize(),
						          str.c_str());
					}
					handler(Result<R>::fromException(std::move(str)));
				}
			}
			else
			{
				// if the stream is nullptr, it means the result is being aborted
				handler(Result<R>());
			}
		};
		if (dbg)
			r.dbg = std::make_unique<DebugInfo>(*dbg);
	}

	void processReply(Stream& in, Header hdr)
	{
		auto it = replies.find(hdr.key());
		assert(it != replies.end());
		ReplyInfo r = std::move(it->second);
		replies.erase(it);
		r.h(&in, hdr, r.dbg.get());
	}

	void abortReplies()
	{
		decltype(replies) tmp;
		replies.swap(tmp);

		for (auto&& r : tmp)
		{
			if  (r.second.dbg)
			{
				CZRPC_LOG(Log, CZRPC_LOGSTR_ABORT"", r.second.dbg->num);
			}
			r.second.h(nullptr, Header(), r.second.dbg.get());
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

	InProcessor() {}

	void init(Type* obj)
	{
		this->obj = obj;
		data.init(obj);
		data.authPassed = data.objData.getAuthToken() == "" ? true : false;
	}

	void processCall(BaseConnection& con, Transport& transport, Stream& in, Header hdr, DebugInfo* dbg)
	{
		if (dbg)
		{
			CZRPC_LOG(Log, CZRPC_LOGSTR_RECEIVE"size=%u,counter=%u,%u:%s::%s",
				dbg->num,
				in.writeSize(),
				hdr.bits.counter, hdr.bits.rpcid,
				Table<T>::getName().c_str(), Table<T>::get(hdr.bits.rpcid)->name.c_str());
		}
		auto&& info = Table<Type>::get(hdr.bits.rpcid);
		info->dispatcher(*obj, in, data, con, transport, hdr, dbg);
	}

	void clear()
	{
		data.clear();
	}

	Type* obj=nullptr;
	InProcessorData data;
};

template<>
class InProcessor<void>
{
public:
	InProcessor() {}
	void init(void* obj) {}
	void processCall(BaseConnection& con, Transport& trp, Stream& in, Header hdr, DebugInfo* dbg)
	{
		if (dbg)
		{
			CZRPC_LOG(Log, CZRPC_LOGSTR_RECEIVE"size=%u,%u,%u:void::NA",
				dbg->num,
				in.writeSize(), hdr.bits.counter, hdr.bits.rpcid);
		}
		//assert(0 && "Incoming RPC not allowed for void local type");
		details::Send::error(trp, hdr, "Peer doesn't have an object to process RPC calls", dbg);
	}

	void clear()
	{
	}
};

#define CZRPC_CALL_NODBG(con, func, ...)                                        \
    (con).call<decltype(&std::decay<decltype(con)>::type::Remote::func)>( \
        (uint32_t)cz::rpc::Table<                                         \
            std::decay<decltype(con)>::type::Remote>::RPCId::func,        \
        ##__VA_ARGS__)

#define CZRPC_CALL_DBG(con, func, ...)                                     \
    (con).call<decltype(&std::decay<decltype(con)>::type::Remote::func)>( \
		__FILE__, __LINE__,                                               \
        (uint32_t)cz::rpc::Table<                                         \
            std::decay<decltype(con)>::type::Remote>::RPCId::func,        \
        ##__VA_ARGS__)

#define CZRPC_CALLGENERIC_NODBG(con, name, ...) \
	(con).callGeneric(name, ##__VA_ARGS__)

#define CZRPC_CALLGENERIC_DBG(con, name, ...) \
	(con).callGeneric(__FILE__, __LINE__, name, ##__VA_ARGS__)

#if CZRPC_FORCE_CALL_DBG
	#define CZRPC_CALL CZRPC_CALL_DBG
	#define CZRPC_CALLGENERIC CZRPC_CALLGENERIC_DBG
#else
	#define CZRPC_CALL CZRPC_CALL_NODBG
	#define CZRPC_CALLGENERIC CZRPC_CALLGENERIC_NODBG
#endif


} // namespace rpc
} // namespace cz

