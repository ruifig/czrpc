#pragma once

namespace cz
{
namespace rpc
{

struct BaseConnection
{
	virtual ~BaseConnection() { }
	virtual uint32_t process() = 0;
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

	virtual uint32_t process() override
	{
        // Place a callstack marker, so other code can detect we are serving an
        // RPC
        typename Callstack<ThisType>::Context ctx(this);
		uint32_t count = 0;

		std::vector<char> data;
		while (transport->receive(data))
		{
			count++;
			RPCHeader hdr;
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
		return count;
	}

	InProcessor<Local> localPrc;
	OutProcessor<Remote> remotePrc;
	std::shared_ptr<Transport> transport;
};

} // namespace rpc
} // namespace cz
