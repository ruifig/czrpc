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

class WorkQueue
{
public:
	virtual ~WorkQueue() {}
	virtual void push(std::function<void()> f) = 0;
};

template<typename F, typename C>
class Call
{
private:
	typedef typename FunctionTraits<F>::return_type RType;
	typedef ParamTraits<RType> RTraits;
public:

	Call(Call&& other) noexcept
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

	template<typename H>
	void async(WorkQueue* workQueue, H&& handler)
	{
		async([workQueue, handler = std::forward<H>(handler)](Result<typename RTraits::store_type> res) mutable
		{
			workQueue->push([handler=std::forward<H>(handler), res=std::move(res)]() mutable
			{
				handler(std::move(res));
			});
		});
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

#if CZRPC_HAS_PPLTASK
	concurrency::task<class Result<typename RTraits::store_type>> ppltask()
	{
		using ResType = Result<typename RTraits::store_type>;
		concurrency::task_completion_event<ResType> ce;
		async([ce](ResType&& res)
		{
			ce.set(std::move(res));
		});
		return concurrency::create_task(ce);
	}
#endif

protected:

	template<typename L, typename R> friend class Connection;

	explicit Call(C& con, uint32_t rpcid)
		: m_con(con)
	{
		Header hdr;
		hdr.setRPCId(rpcid);
		m_data << hdr; // reserve space for the header
	}
	
	unsigned addDebugInfo(const char* file, int line)
	{
		// debug info needs to be right after the header
		CZRPC_ASSERT(m_data.writeSize() == sizeof(Header));
		auto hdr = reinterpret_cast<Header*>(m_data.ptr(0));
		hdr->setHasDbg(true);
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

// Moved the Table<T>::getTbl to an separate function, so I can have the Table<void> specialization which
// does nothing
namespace detail
{
	template<typename T>
	static inline void touchTable(int counter) { Table<T>::getTbl(counter); }
	// Specialization for void, which does nothing
	template<> inline void touchTable<void>(int counter) { }
}

template<typename LOCAL, typename REMOTE>
class Connection : public BaseConnection
{
public:
	using Local = LOCAL;
	using Remote = REMOTE;
	using ThisType = Connection<Local, Remote>;

	template<typename R, typename C> friend class Call;
private:


public:

	Connection()
	{
		//printf("%p : Constructor\n", this);
		detail::touchTable<Local>(1);
		detail::touchTable<Remote>(1);
	}

	void init(Local* localObj, Transport& transport, std::shared_ptr<SessionData> session)
	{
		m_localPrc.init(localObj);
		initBase(transport, std::move(session));
	}

	virtual ~Connection()
	{
		//printf("%p : Destructor\n", this);
		detail::touchTable<Local>(-1);
		detail::touchTable<Remote>(-1);
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

	virtual void close() override
	{
		m_transport->close();
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

			// We need to clear any futures we are holding, since those can hold strong references to the session
			m_localPrc.clear();

			if (m_onDisconnect)
			{
				m_onDisconnect();
				// Release any resources kept by the handler
				m_onDisconnect = nullptr;
			}
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
			if (hdr.hasDbg())
			{
				dbg = std::make_unique<DebugInfo>();
				in >> *dbg;
			}
				
			if (hdr.isReply())
				m_remotePrc.processReply(in, hdr);
			else
				m_localPrc.processCall(*this, *m_transport, in, hdr, dbg.get());
		}
	}
	
	static Header* getHeader(Stream& data)
	{
		return reinterpret_cast<Header*>(data.ptr(0));
	}
	static DebugInfo* getDbgInfo(Stream& data)
	{
		auto hdr = getHeader(data);
		if (hdr->hasDbg())
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
		hdr->setSize(data.writeSize());
		hdr->setCounter(++m_remotePrc.replyIdCounter);
		auto dbg = getDbgInfo(data);
		if (dbg)
		{
			CZRPC_LOG(Log, CZRPC_LOGSTR_SEND"hdr=(size=%u, counter=%u",
				dbg->num, hdr->getSize(), hdr->getCounter());
		}
		m_remotePrc.template addReplyHandler<F>(hdr->key(), std::forward<H>(handler), dbg);
		return m_transport->send(data.extract());
	}

};

} // namespace rpc
} // namespace cz
