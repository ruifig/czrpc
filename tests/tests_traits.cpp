#include "testsPCH.h"


// Allow "const T&" RPC parameters, for any valid T
CZRPC_ALLOW_CONST_LVALUE_REFS;

struct Foo
{
	int val;
	explicit Foo(int val = 0) : val(val)
	{
		//printf("%p: Foo::Foo(%d)\n", this, val);
	}
	Foo(Foo&& other) : val(other.val)
	{
		other.val = -1;
		//printf("%p: Foo::Foo(&& %p %d)\n", this, &other, val);
	}

	Foo(const Foo& other) : val(other.val)
	{
		//printf("%p: Foo::Foo(const& %p %d)\n", this, &other, val);
	}
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

//
// Make "int*" usable as RPC parameters, for testing
template<>
struct cz::rpc::ParamTraits<int*>
{
	using store_type = int*;
	static constexpr bool valid = true;

	template<typename S>
	static void write(S& s, int* v) {
		s << (uint64_t)v;
	}
	template<typename S>
	static void read(S&s, store_type& v) {
		uint64_t tmp;
		s >> tmp;
		v = (int*)tmp;
	}
};


struct Bar
{
	std::string name;

	int misc(int , float , const char* , const std::string& , Foo /*f1*/, Foo& f2, const Foo& /*f3*/, Foo&& f4)
	{
		return 0;
	}

	void valid1() {}
	void valid2(int) {}
	void valid3(const int&) {}
	int valid4() { return 0; }
	int valid5() { return 0; }
	float* invalid1() { return nullptr; }
	int invalid2(int, float*) { return 0; }
};


SUITE(Traits)
{

//
// Test Stream operations
TEST(Stream)
{
	using namespace cz;
	using namespace rpc;

	Stream s;
	
	// Write
	int someInt = 123;
	{
		s << &someInt;
		s << 100;
		s << "Hello";
		s << std::string("World!");
		std::vector<std::string> v;
		v.push_back("A");
		v.push_back("BB");
		s << v;
		Foo foo(200);
		s << foo;
	}

	// Read back the same values
	{
		int* someIntPtr;
		s >> someIntPtr;
		CHECK_EQUAL(someInt, *someIntPtr);
		int i;
		s >> i;
		CHECK_EQUAL(100, i);
		std::string str;
		s >> str;
		CHECK_EQUAL("Hello", str);
		s >> str;
		CHECK_EQUAL("World!", str);
		std::vector<std::string> v;
		s >> v;
		CHECK_EQUAL(2, v.size());
		CHECK_EQUAL("A", v[0]);
		CHECK_EQUAL("BB", v[1]);
		Foo foo;
		s >> foo;
		CHECK_EQUAL(200, foo.val);
		CHECK_EQUAL(0, s.readSize());
	}

	printf("\n");
}

//
// Testing checking if method signatures are valid for RPC calls
TEST(FunctionCheck)
{
	using namespace cz;
	using namespace rpc;

	bool p0 = ParamPack<>::valid; // VALID : No parameters is valid
	bool p1 = ParamPack<int>::valid; // VALID : A native type is valid
	bool p2 = ParamPack<int,float>::valid; // VALID: A native type is valid
	bool p3 = ParamPack<int,int*>::valid; // VALID : Pointers are not normally valid, but we explicitly allowed "int*" above
	bool p4 = ParamPack<int,double&>::valid; // INVALID : Non-const lvalue refs are invalid unless explicitly specified
	bool p5 = ParamPack<int,const double&>::valid; // VALID : const lvalue refs are valid because we said so above.
	CHECK(p0 == true);
	CHECK(p1 == true);
	CHECK(p2 == true);
	CHECK(p3 == true);
	CHECK(p4 == false);
	CHECK(p5 == true);

	CHECK(FunctionTraits<decltype(&Bar::misc)>::valid == true);
	CHECK(FunctionTraits<decltype(&Bar::valid1)>::valid == true);
	CHECK(FunctionTraits<decltype(&Bar::valid2)>::valid == true);
	CHECK(FunctionTraits<decltype(&Bar::valid3)>::valid == true);
	CHECK(FunctionTraits<decltype(&Bar::valid4)>::valid == true);
	CHECK(FunctionTraits<decltype(&Bar::valid5)>::valid == true);

	CHECK(FunctionTraits<decltype(&Bar::invalid1)>::valid == false);
	CHECK(FunctionTraits<decltype(&Bar::invalid2)>::valid == false);
}

//
// Test serializing parameters, then unserialize them and call a method
TEST(FunctionSerialization)
{
	using namespace cz;
	using namespace rpc;

	Bar bar;
	bar.name = "Some Bar object";

	Stream s;

	// Serialize parameters
	Foo tmp(2);
	const Foo& f2 = tmp;
	serializeMethod<decltype(&Bar::misc)>(s, 1, 2.5f, "A", "B", Foo(1), f2, Foo(3), Foo(4));

	// Unserialize and test calling
	using Tuple = FunctionTraits<decltype(&Bar::misc)>::param_tuple;
	Tuple params;
	s >> params;
	callMethod(bar, &Bar::misc, std::move(params));

}

}