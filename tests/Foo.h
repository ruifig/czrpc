#pragma once

struct Foo
{
	int val;
	explicit Foo(int val = 0) : val(val)
	{
		++ms_defaultCalls;
	}
	Foo(Foo&& other) : val(other.val)
	{
		++ms_moveCalls;
		other.val = -1;
	}

	Foo(const Foo& other) : val(other.val)
	{
		++ms_copyCalls;
	}

	static void resetCounters()
	{
		ms_defaultCalls = 0;
		ms_moveCalls = 0;
		ms_copyCalls = 0;
	}

	static void check(int defaultCalls, int moveCalls, int copyCalls)
	{
		CHECK(ms_defaultCalls == defaultCalls);
		CHECK(ms_moveCalls == moveCalls);
		CHECK(ms_copyCalls == copyCalls);
		resetCounters();
	}

	static std::atomic<int> ms_defaultCalls;
	static std::atomic<int> ms_moveCalls;
	static std::atomic<int> ms_copyCalls;
};

//
// Make Foo instances usable as RPC parameters
template<>
struct cz::rpc::ParamTraits<Foo> : public cz::rpc::DefaultParamTraits<Foo>
{
	template<typename S>
	static void write(S& s, const Foo& v) {
		s << v.val;
	}
	template<typename S>
	static void read(S&s, Foo& v) {
		s >> v.val;
	}
};

//
// Allow "const Foo&", "Foo&", and "Foo&&" as RPC parameters
CZRPC_DEFINE_CONST_LVALUE_REF(Foo);
CZRPC_DEFINE_NON_CONST_LVALUE_REF(Foo);
CZRPC_DEFINE_RVALUE_REF(Foo);

