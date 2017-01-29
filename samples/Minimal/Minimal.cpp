#include "MinimalPCH.h"
using namespace cz::rpc;
using namespace std;

class Calc {
public:
  int add(int a, int b) { return a + b; }
};

#define RPCTABLE_CLASS Calc
#define RPCTABLE_CONTENTS REGISTERRPC(add)
#include "crazygaze/rpc/RPCGenerate.h"

void testSimpleServerAndClient() {
  // Server
  Calc calc;
  SimpleServer<Calc> server(calc, 9000, "");
  // Client and 1 RPC call
  auto con = TCPTransport<void, Calc>::create("127.0.0.1", 9000).get();
  cout << CZRPC_CALL(*con, add, 1, 2).ft().get().get();
}

int main() {
  testSimpleServerAndClient();
  // std::this_thread::sleep_for(std::chrono::milliseconds(10));
  // getSharedData<TCPServiceThread>()->stop();
  return 0;
}
