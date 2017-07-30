#pragma once


namespace cz
{
namespace rpc
{

namespace details
{
	// Signature of the generic RPC call. This helps reuse some of the code,
	// since it's just another function
	typedef Any(*GenericRPCFunc)(const std::string&, const std::vector<Any>&);
}

class BaseConnection
{
public:
	BaseConnection() { }
	virtual ~BaseConnection() { }

	enum class Direction
	{
		In = 1 << 0,
		Out = 1 << 1,
		Both = In | Out
	};

	virtual void process(Direction what=Direction::Both) = 0;
	virtual bool isRunningInThread() = 0;

	void close()
	{
		m_transport->close();
	}

	const Transport* getTransport()
	{
		return m_transport;
	}

	std::shared_ptr<Session> getSession()
	{
		return m_weakSession.lock();
	}

protected:

	void initBase(Transport& transport, std::shared_ptr<Session> session)
	{
		m_transport = &transport;
		m_transport->m_con = this;
		m_strongSession = std::move(session);
		m_weakSession = m_strongSession;
	}

	Transport* m_transport = nullptr;
	// The strong reference is kept until the transport disconnects, or the user explicitly closes the connection
	// This keeps a session alive even if there are no pending operations.
	std::shared_ptr<Session> m_strongSession;
	// This keeps a weak reference, and it's the one from where everything gets a session pointers.
	// This allows the transport to still get a valid session pointer during shutdown, since at the point the strong
	// reference might not be valid anymore.
	std::weak_ptr<Session> m_weakSession;
};

template<typename F, typename C>
class Call
{
private:
	typedef typename FunctionTraits<F>::return_type RType;
	typedef ParamTraits<RType> RTraits;
public:

	Call(Call&& other)
		: m_con(other.m_con)
		, m_data(std::move(other.m_data))
		, m_commited(other.m_commited)
	{
	}

	Call(const Call&) = delete;
	Call& operator=(const Call&) = delete;
	Call& operator=(Call&&) = delete;

	~Call()
	{
		if (m_data.writeSize() && !m_commited)
			async([](Result<typename RTraits::store_type>&&) {});
	}

	template<typename H>
	void async(H&& handler)
	{
		m_con.template commit<F>(std::move(m_data), std::forward<H>(handler));
		m_commited = true;
	}

	std::future<class Result<typename RTraits::store_type>> ft()
	{
		auto pr = std::make_shared<std::promise<Result<typename RTraits::store_type>>>();
		auto ft = pr->get_future();
		async([pr=std::move(pr)](Result<typename RTraits::store_type>&& res)
		{
			pr->set_value(std::move(res));
		});

		return ft;
	}

protected:

	template<typename L, typename R> friend class Connection;

	explicit Call(C& con, uint32_t rpcid)
		: m_con(con)
	{
		Header hdr;
		hdr.bits.rpcid = rpcid;
		m_data << hdr; // reserve space for the header
	}
	
	unsigned addDebugInfo(const char* file, int line)
	{
		// debug info needs to be right after the header
		CZRPC_ASSERT(m_data.writeSize() == sizeof(Header));
		auto hdr = reinterpret_cast<Header*>(m_data.ptr(0));
		hdr->bits.hasDbg = true;
		DebugInfo dbg(file, line);
		m_data << dbg;
		return dbg.num;
	}

	template<typename... Args>
	void serializeParams(Args&&... args)
	{
		serializeMethod<F>(m_data, std::forward<Args>(args)...);
	}

	C& m_con;
	Stream m_data;
	// Used in the destructor to do a commit with an empty handler if the rpc was not committed.
	bool m_commited = false;
};

template<typename LOCAL, typename REMOTE>
class Connection : public BaseConnection
{
public:
	using Local = LOCAL;
	using Remote = REMOTE;
	using ThisType = Connection<Local, Remote>;

	template<typename R, typename C> friend class Call;

	//! If using this constructor, then use #init method to initialize the object.
	Connection()
	{
	}

	void init(Local* localObj, Transport& transport, std::shared_ptr<Session> session)
	{
		m_localPrc.init(localObj);
		initBase(transport, std::move(session));
	}

	virtual ~Connection()
	{
	}

	template<typename F, typename... Args>
	auto call(uint32_t rpcid, Args&&... args)
	{
		using Traits = FunctionTraits<F>;
		static_assert(
			std::is_member_function_pointer<F>::value &&
			std::is_base_of<typename Traits::class_type, Remote>::value,
			"Function is not a member function of the specified remote side class");
		Call<F, ThisType> c(*this, rpcid);
		c.serializeParams(std::forward<Args>(args)...);
		return c;
	}

	template<typename F, typename... Args>
	auto call(const char* file, int line, uint32_t rpcid, Args&&... args)
	{
		using Traits = FunctionTraits<F>;
		static_assert(
			std::is_member_function_pointer<F>::value &&
			std::is_base_of<typename Traits::class_type, Remote>::value,
			"Function is not a member function of the specified remote side class");
		Call<F, ThisType> c(*this, rpcid);
		auto dbgnum = c.addDebugInfo(file, line);
		c.serializeParams(std::forward<Args>(args)...);
		CZRPC_LOG(Log, CZRPC_LOGSTR_CREATE"%u:%s::%s (%s,%d)",
			dbgnum,
			rpcid, Table<Remote>::getName().c_str(), Table<Remote>::get(rpcid)->name.c_str(),
			file, line);
		return c;
	}

	auto callGeneric(const std::string& name, const std::vector<Any>& args = std::vector<Any>())
	{
		uint32_t rpcid = (uint32_t)Table<Remote>::RPCId::genericRPC;
		Call<details::GenericRPCFunc, ThisType> c(*this, rpcid);
		c.serializeParams(name, args);
		return c;
	}
	auto callGeneric(const char* file, int line, const std::string& name, const std::vector<Any>& args = std::vector<Any>())
	{
		uint32_t rpcid = (uint32_t)Table<Remote>::RPCId::genericRPC;
		Call<details::GenericRPCFunc, ThisType> c(*this, rpcid);
		auto dbgnum = c.addDebugInfo(file, line);
		c.serializeParams(name, args);
		CZRPC_LOG(Log, CZRPC_LOGSTR_CREATE"%u:%s::%s(%s) (%s,%d)",
			dbgnum,
			rpcid, Table<Remote>::getName().c_str(), Table<Remote>::get(rpcid)->name.c_str(), name.c_str(),
			file, line);
		return c;
	}

	static ThisType* getCurrent()
	{
		auto it = Callstack<ThisType>::begin();
		return (*it)==nullptr ? nullptr : (*it)->getKey();
	}

	virtual bool isRunningInThread() override
	{
		return getCurrent() == this;
	}

	virtual void process(Direction what) override
	{
		// Place a callstack marker, so other code can detect we are serving an RPC
		typename Callstack<ThisType>::Context ctx(this);
		bool ok = true;
		if ((int)what & (int)Direction::Out)
			ok = processOut() && ok;
		if ((int)what & (int)Direction::In)
			ok = processIn() && ok;
		if (!ok)
		{
			m_remotePrc.abortReplies();
			if (m_onDisconnect)
			{
				m_onDisconnect();
				// Release any resources kept by the handler
				m_onDisconnect = nullptr;
			}
			// Release our strong reference to the session data.
			// If the transport will still need to keep the session alive, it can get it from the weak_ptr.
			m_strongSession = nullptr;
		}
	}

	// Called whenever a RPC call is committed (e.g: As a result of CZRPC_CALL).
	// Can be used by custom transports to trigger calls to Connection::process
	// #TODO : Remove this
#if 0
	void setOutSignal(std::function<void()> callback)
	{
		m_outSignal = std::move(callback);
	}
#endif

	// #TODO : Remove/revise this
	void setOnDisconnect(std::function<void()> callback)
	{
		m_onDisconnect = std::move(callback);
	}

protected:

	using WorkQueue = std::queue<std::function<bool()>>;
	InProcessor<Local> m_localPrc;
	OutProcessor<Remote> m_remotePrc;

	Monitor<WorkQueue> m_outWork;
	WorkQueue m_tmpOutWork;
	// #TODO : Remove this
#if 0
	std::function<void()> m_outSignal; // #TODO Remove this
#endif
	std::function<void()> m_onDisconnect; // #TODO : Remove/revise this

	virtual bool processOut()
	{
		m_outWork([&](WorkQueue& q)
		{
			std::swap(m_tmpOutWork, q);
		});

		bool ok = true;
		while (m_tmpOutWork.size())
		{
			// NOTE: If the transport fails while we still have outgoing work, we still process all of it,
			// so the replies are properly setup and later canceled.
			ok = m_tmpOutWork.front()() && ok;
			m_tmpOutWork.pop();
		}

		return ok;
	}

	virtual bool processIn()
	{
		std::vector<char> data;
		size_t done = 0;

		while(true)
		{
			if (!m_transport->receive(data))
				return false;

			// If we get an empty RPC, but "receive" returns true, it means the transport is still
			// open, but there is no incoming RPC data
			if (data.size() == 0)
				return true;

			Header hdr;
			Stream in(std::move(data));
			in >> hdr;
			std::unique_ptr<DebugInfo> dbg;
			if (hdr.bits.hasDbg)
			{
				dbg = std::make_unique<DebugInfo>();
				in >> *dbg;
			}
				
			if (hdr.bits.isReply)
				m_remotePrc.processReply(in, hdr);
			else
				m_localPrc.processCall(*m_transport, in, hdr, dbg.get());
		}
	}
	
	static Header* getHeader(Stream& data)
	{
		return reinterpret_cast<Header*>(data.ptr(0));
	}
	static DebugInfo* getDbgInfo(Stream& data)
	{
		auto hdr = getHeader(data);
		if (hdr->bits.hasDbg)
			return reinterpret_cast<DebugInfo*>(hdr + 1);
		else
			return nullptr;
	}

	template<typename F, typename H>
	void commit(Stream&& data, H&& handler)
	{
		auto dbg = getDbgInfo(data);
		if (dbg)
		{
			auto hdr = getHeader(data);
			CZRPC_LOG(Log, CZRPC_LOGSTR_COMMIT"", dbg->num);
		}
		
		auto session = getSession();
		m_outWork([&](WorkQueue& q)
		{
			q.emplace([this, session=std::move(session), data = std::move(data), handler = std::move(handler)]() mutable -> bool
			{
				return send<F>(std::move(data), std::move(handler));
			});
		});

		m_transport->onSendReady();
	}

	template<typename F, typename H>
	bool send(Stream&& data, H&& handler)
	{
		auto hdr = getHeader(data);
		hdr->bits.size = data.writeSize();
		hdr->bits.counter = ++m_remotePrc.replyIdCounter;
		auto dbg = getDbgInfo(data);
		if (dbg)
		{
			CZRPC_LOG(Log, CZRPC_LOGSTR_SEND"hdr=(size=%u, counter=%u",
				dbg->num, hdr->bits.size, hdr->bits.counter);
		}
		m_remotePrc.template addReplyHandler<F>(hdr->key(), std::forward<H>(handler), dbg);
		return m_transport->send(data.extract());
	}

};

} // namespace rpc
} // namespace cz
