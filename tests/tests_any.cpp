#include "testsPCH.h"

using namespace cz::rpc;

SUITE(Any)
{

template<typename V>
void TestConstructor(V&& v, Any::Type expectedType)
{
	Any a(std::forward<V>(v));
	CHECK(a.getType() == expectedType);
}

TEST(Constructors)
{
	Any a;
	CHECK(a.getType() == Any::Type::None);
	TestConstructor(true, Any::Type::Bool);
	TestConstructor(int(-1), Any::Type::Integer);
	TestConstructor((unsigned int)1, Any::Type::UnsignedInteger);
	TestConstructor(float(0.5), Any::Type::Float);
	TestConstructor("Hello", Any::Type::String);
	TestConstructor(std::string("Hello"), Any::Type::String);
	TestConstructor(std::vector<unsigned char>{ 0, 1, 2, 3, 4 }, Any::Type::Blob);
	TestConstructor(std::vector<int>{0, 1}, Any::Type::String);
}

TEST(toString)
{
	CHECK(std::string(Any().toString()) == "");
	CHECK(std::string(Any(true).toString()) == "true");
	CHECK(std::string(Any(false).toString()) == "false");
	CHECK(std::string(Any(int(1234)).toString()) == "1234");
	CHECK(std::string(Any(int(-1234)).toString()) == "-1234");
	CHECK(std::string(Any(unsigned(1234)).toString()) == "1234");
	CHECK(std::string(Any(float(1234.5)).toString()) == "1234.5000");
	CHECK(std::string(Any("hello").toString()) == "hello");
	CHECK(std::string(Any(std::vector<unsigned char>{0, 1}).toString()) == "BLOB{2}");
}

TEST(TupleConversion)
{
	std::tuple<bool, int, int, unsigned, float, std::string, std::vector<unsigned char>> t;

	std::vector<Any> a;
	a.push_back(Any());
	a.push_back(Any(int(1)));
	a.push_back(Any(int(-2)));
	a.push_back(Any(unsigned(3)));
	a.push_back(Any(float(4)));
	a.push_back(Any(std::string("5")));
	a.push_back(Any(std::vector<unsigned char>{0, 1, 2, 3, 4, 5}));

	auto r = toTuple(a, t);
	CHECK(r == false); // The first element can't be converted

	a[0] = Any(true);
	r = toTuple(a, t);
	CHECK(r == true);

	// Make sure the tuple as the expected values
	CHECK(t == std::make_tuple(true, 1, -2, (unsigned)3, float(4), std::string("5"), std::vector<unsigned char>{0, 1, 2, 3, 4, 5}));

}

}
