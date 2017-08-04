#include "testsPCH.h"
#include "tests_rpc_spas_helper.h"

using namespace cz;
using namespace cz::rpc;

SessionLeakDetector gSessionLeakDetector;

// Server side calls a RPC on the client...
int Tester::testClientAddCall(int a, int b)
{
	auto client = cz::rpc::Connection<Tester, TesterClient>::getCurrent();
	CHECK(client != nullptr);
	CZRPC_CALL(*client, clientAdd, a, b).async(
		[this, r = a+b](Result<int> res)
	{
		CHECK_EQUAL(r, res.get());
		clientCallRes.set_value(res.get());
	});

	return a + b;
}


// This tests what happens when the server tries to call an RPC on a client which doesn't have a local object.
// In other words, the client's connection is in the form Connection<void,SomeRemoteInterface>
// In this case, the client having a InProcessor<void>, should send back an error for all RPC calls it receives.
// In turn, the server sends back this error as a reply to the original RPC call from the client, so the unit test
// can check if everything is still working and not blocked somewhere.
std::future<std::string> Tester::testClientVoid()
{
	auto con = cz::rpc::Connection<Tester, TesterClient>::getCurrent();
	CHECK(con != nullptr);
	//auto session = std::static_pointer_cast<Session<Tester, TesterClient>>(con->getSession());
	//CHECK(session != nullptr);
	auto pr = std::make_shared<std::promise<std::string>>();
	CZRPC_CALL(*con, clientAdd, 1, 2).async(
		[this, pr](Result<int> res)
	{
		CHECK(res.isException());
		pr->set_value(res.getException());
	});

	return pr->get_future();
}
