#include "testsPCH.h"
#include "../samples/SamplesCommon/SimpleServer.h"

using namespace cz::rpc;

struct Vec3
{
	float x = 0;
	float y = 0;
	float z = 0;
	Vec3() {}
	Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
};

//
// Define how to deal with Vec3 in RPC parameters and return values
template <>
struct cz::rpc::ParamTraits<Vec3> : public cz::rpc::DefaultParamTraits<Vec3>
{
	template <typename S>
	static void write(S& s, const Vec3& v) {
		s << v.x << v.y << v.z;
	}
	template <typename S>
	static void read(S& s, Vec3& v) {
		s >> v.x >> v.y >> v.z;
	}
};

CZRPC_DEFINE_CONST_LVALUE_REF(Vec3)

class TestClass
{
public:
	Vec3 add(Vec3 a, Vec3 b) {
		return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
	}
};

#define RPCTABLE_CLASS TestClass
#define RPCTABLE_CONTENTS \
	REGISTERRPC(add)
#include "crazygaze/rpc/RPCGenerate.h"

void RunTestServer()
{
	TestClass obj;
	SimpleServer<TestClass, void> server(obj, 9000);
}

void RunTestClient() {
    asio::io_service io;
    std::thread th = std::thread([&io] {
        asio::io_service::work w(io);
        io.run();
    });

    auto con = AsioTransport<void, TestClass>::create(io, "127.0.0.1", 9000).get();

	CZRPC_CALL(*con, add, Vec3(1,2,3), Vec3(2,3,4))
		.async([&io](Result<Vec3> res)
	{
		Vec3 v  = res.get();
		io.stop();
	});

    th.join();
}

void RunDocTest_ParamTraits()
{
    auto a = std::thread([] { RunTestServer(); });
    auto b = std::thread([] { RunTestClient(); });
    a.join();
    b.join();
}
