
#ifndef RPCTABLE_CLASS
	#error "Macro RPCTABLE_CLASS needs to be defined"
#endif
#ifndef RPCTABLE_CONTENTS
	#error "Macro RPCTABLE_CONTENTS needs to be defined"
#endif


#define RPCTABLE_TOOMANYRPCS_STRINGIFY(arg) #arg
#define RPCTABLE_TOOMANYRPCS(arg) RPCTABLE_TOOMANYRPCS_STRINGIFY(arg)

#define RPCTABLE_STRINGIFY(exp) RPCTABLE_STRINGIFY2(exp)
#define RPCTABLE_STRINGIFY2(exp) #exp

namespace cz { namespace rpc {

template<> class Table<RPCTABLE_CLASS> : TableImpl<RPCTABLE_CLASS>
{
public:
	using Type = RPCTABLE_CLASS;
	#define REGISTERRPC(rpc) rpc,
	enum class RPCId {
		genericRPC,
		RPCTABLE_CONTENTS
		NUMRPCS
	};

	Table() : TableImpl(RPCTABLE_STRINGIFY(RPCTABLE_CLASS))
	{
		registerGenericRPC();
		static_assert((unsigned)((int)RPCId::NUMRPCS-1)<(1<<Header::kRPCIdBits),
			RPCTABLE_TOOMANYRPCS(Too many RPCs registered for class RPCTABLE_CLASS));
		#undef REGISTERRPC
		#define REGISTERRPC(func) registerRPC((uint32_t)RPCId::func, #func, &Type::func);
		RPCTABLE_CONTENTS
	}

	//!
	// A work around to get rid of false warnings for memory leaks when using czrpc in a DLL.
	// A Connection instance using a Table needs to can this with 1 in the constructor, then with -1 in the
	// destructor. Once no more connections need it, it will get destroyed.
	static const Table<RPCTABLE_CLASS>* getTbl(int counterInc)
	{
		static std::mutex mtx;
		static int counter;
		static std::unique_ptr<Table<RPCTABLE_CLASS>> tbl;
		if (counterInc == 0)
		{
			// If counterInc is 0, whoever is calling this should have already called it with counterInc>0 to
			// create the table, so in this case I believe we don't even need a mutex by design, since the tbl
			// pointer will be read only for all the threads until all threads make their getTbl(-1) calls
			// to release the table
			assert(tbl.get() != nullptr);
			return tbl.get();
		}
		else
		{
			std::unique_lock<std::mutex> lock(mtx);
			counter += counterInc;
			if (counter > 0)
			{
				if (tbl.get() == nullptr)
					tbl = std::make_unique<Table<RPCTABLE_CLASS>>();
			}
			else
			{
				tbl = nullptr;
			}
			return tbl.get();
		}
	}

	static const std::string& getName()
	{
		return getTbl(0)->m_name;
	}

	static const Info* get(uint32_t rpcid)
	{
		auto tbl = getTbl(0);
		assert(tbl->isValid(rpcid));
		return static_cast<Info*>(tbl->m_rpcs[rpcid].get());
	}
};

} // namespace rpc
} // namespace cz

#undef REGISTERRPC
#undef RPCTABLE_START
#undef RPCTABLE_END
#undef RPCTABLE_CLASS
#undef RPCTABLE_CONTENTS
#undef RPCTABLE_TOOMANYRPCS_STRINGIFY
#undef RPCTABLE_TOOMANYRPCS
