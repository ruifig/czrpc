
#ifndef RPCTABLE_CLASS
	#error "Macro RPCTABLE_CLASS needs to be defined"
#endif
#ifndef RPCTABLE_CONTENTS
	#error "Macro RPCTABLE_CONTENTS needs to be defined"
#endif


#define RPCTABLE_TOOMANYRPCS_STRINGIFY(arg) #arg
#define RPCTABLE_TOOMANYRPCS(arg) RPCTABLE_TOOMANYRPCS_STRINGIFY(arg)

template<> class cz::rpc::Table<RPCTABLE_CLASS> : cz::rpc::TableImpl<RPCTABLE_CLASS>
{
public:
	using Type = RPCTABLE_CLASS;
	#define REGISTERRPC(rpc) rpc,
	enum class RPCId {
		RPCTABLE_CONTENTS
		NUMRPCS
	};

	Table()
	{
		static_assert((unsigned)((int)RPCId::NUMRPCS-1)<(1<<Header::kRPCIdBits),
			RPCTABLE_TOOMANYRPCS(Too many RPCs registered for class RPCTABLE_CLASS));
		#undef REGISTERRPC
		#define REGISTERRPC(func) registerRPC((uint32_t)RPCId::func, #func, &Type::func);
		RPCTABLE_CONTENTS
	}

	static const RPCInfo* get(uint32_t rpcid)
	{
		static Table<RPCTABLE_CLASS> tbl;
		assert(tbl.isValid(rpcid));
		return static_cast<RPCInfo*>(tbl.m_rpcs[rpcid].get());
	}
};

#undef REGISTERRPC
#undef RPCTABLE_START
#undef RPCTABLE_END
#undef RPCTABLE_CLASS
#undef RPCTABLE_CONTENTS
#undef RPCTABLE_TOOMANYRPCS_STRINGIFY
#undef RPCTABLE_TOOMANYRPCS
