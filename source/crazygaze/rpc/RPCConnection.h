#pragma once

namespace cz
{
namespace rpc
{

struct BaseConnection
{
	virtual ~BaseConnection() { }

	//! Process any incoming RPCs or replies
	// Return true if the connection is still alive, false otherwise
	virtual bool process() = 0;
};

template<typename LOCAL, typename REMOTE>
struct Connection : public BaseConnection
{
	using Local = LOCAL;
	using Remote = REMOTE;
	using ThisType = Connection<Local, Remote>;
	Connection(Local* localObj, std::shared_ptr<Transport> transport)
		: localPrc(localObj)
		, transport(std::move(transport))
	{
	}

	template<typename F, typename... Args>
	auto call(Transport& transport, uint32_t rpcid, Args&&... args)
	{
		return remotePrc.template call<F>(transport, rpcid, std::forward<Args>(args)...);
	}

	static ThisType* getCurrent()
	{
		auto it = Callstack<ThisType>::begin();
		return (*it)==nullptr ? nullptr : (*it)->getKey();
	}

	virtual bool process() override
	{
        // Place a callstack marker, so other code can detect we are serving an
        // RPC
        typename Callstack<ThisType>::Context ctx(this);
		std::vector<char> data;
		while(true)
		{
			if (!transport->receive(data))
			{
				// Transport is closed
				remotePrc.abortReplies();
				return false;
			}

			if (data.size() == 0)
				return true; // No more pending data to process

			Header hdr;
			Stream in(std::move(data));
			in >> hdr;

			if (hdr.bits.isReply)
			{
				remotePrc.processReply(in, hdr);
			}
			else
			{
				localPrc.processCall(*transport, in, hdr);
			}
		}
	}

	InProcessor<Local> localPrc;
	OutProcessor<Remote> remotePrc;
	std::shared_ptr<Transport> transport;
};

} // namespace rpc
} // namespace cz
