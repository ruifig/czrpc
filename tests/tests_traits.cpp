#include "testsPCH.h"
#include "Foo.h"


// Allow "const T&" RPC parameters, for any valid T
CZRPC_ALLOW_CONST_LVALUE_REFS

//
// Make "int*" usable as RPC parameters, for testing
namespace cz
{
namespace rpc
{
	template<>
	struct ParamTraits<int*>
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
}
}

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
	std::future<int> valid6(int a) { return std::future<int>(); }
	float* invalid1() { return nullptr; }
	int invalid2(int, float*) { return 0; }
	std::future<float*> invalid3(int a) { return std::future<float*>(); }
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

		// Test the vector of arithmetic optimization
		s << std::vector<int>(); // empty vector
		s << std::vector<int>{ 1, 2 };
		s << std::vector<int>{ 3, 4 };

		std::vector<std::string> v;
		s << v; //
		v.push_back("A");
		v.push_back("BB");
		s << v;
		s << std::vector<std::string>{"CC", "DD"};
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

		std::vector<int> intv1{ 1,2 };
		std::vector<int> intv2{ 3,4 };
		std::vector<int> intv;
		s >> intv;
		CHECK_EQUAL(0, intv.size());
		s >> intv;
		CHECK_EQUAL(2, intv.size());
		CHECK_ARRAY_EQUAL(intv1, intv, 2);
		s >> intv;
		CHECK_EQUAL(2, intv.size());
		CHECK_ARRAY_EQUAL(intv2, intv, 2);

		// Test reading two std::vector into the same variable to check if the stream
		// clears a variable before unserializing something into it
		std::vector<std::string> vv1{ "A", "BB" };
		std::vector<std::string> vv2{ "CC", "DD" };
		std::vector<std::string> v;
		s >> v;
		CHECK_EQUAL(0, v.size());
		s >> v;
		CHECK_EQUAL(2, v.size());
		CHECK_ARRAY_EQUAL(vv1, v, 2);
		s >> v;
		CHECK_EQUAL(2, v.size());
		CHECK_ARRAY_EQUAL(vv2, v, 2);

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
	CHECK(FunctionTraits<decltype(&Bar::valid6)>::valid == true);

	CHECK(FunctionTraits<decltype(&Bar::invalid1)>::valid == false);
	CHECK(FunctionTraits<decltype(&Bar::invalid2)>::valid == false);
	CHECK(FunctionTraits<decltype(&Bar::invalid3)>::valid == false);

	// Test detection of future
	CHECK(FunctionTraits<decltype(&Bar::valid5)>::isasync == false);
	CHECK(FunctionTraits<decltype(&Bar::valid6)>::isasync == true);
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