+++
date = "2016-08-15T23:16:59+01:00"
next = "/api"
prev = "/introduction/what-is-czrpc"
title = "A small taste"
toc = true
weight = 102
+++

Before we dive into the API documentation lets look at a fully functional (although useless) sample, so you have an idea what to expect.

czrpc makes it possible to call RPCs on a class which doesn't know anything about RPCs or network.
For example, the following shows an RPC-agnostic *Calculator* class, that we'll be using server side to call RPCs on.

```cpp
using namespace cz::rpc;

// RPC-agnostic class that performs calculations.
class Calculator {
public:
    double add(double a, double b) { return a + b; }
    double sub(double a, double b) { return a - b; }
};
```

Now, we specify how to call rpcs on a Calculator instance:

```cpp
// Define the RPC table for the Calculator class
// This needs to be seen by both the server and client code
#define RPCTABLE_CLASS Calculator
#define RPCTABLE_CONTENTS \
    REGISTERRPC(add) \
    REGISTERRPC(sub)
#include "crazygaze/rpc/RPCGenerate.h"
```

czrpc knows now everything it needs about Calculator. We can now create a server that will allow clients to call RPCs on a Calculator instance:

```cpp
// Starts a Server that only accepts 1 client, then shuts down
// when the client disconnects
void RunServer() {
    asio::io_service io;
    // Start thread to run Asio's the io_service
    // we will be using for the server
    std::thread th = std::thread([&io] {
        asio::io_service::work w(io);
        io.run();
    });
 
    // Instance we will be using to serve RPC calls.
    // Note that it's an object that knows nothing about RPCs
    Calculator calc;
 
    // Start listening for a client connection.
    // We specify what Calculator instance clients will use,
    auto acceptor = AsioTransportAcceptor<Calculator, void>::create(io, calc);
    // For simplicity, we are only expecting 1 client
    using ConType = Connection<Calculator, void>;
    std::shared_ptr<ConType> con;
    acceptor->start(9000, [&io, &con](std::shared_ptr<ConType> con_) {
        con = con_;
        // Since this is just a sample, close the server once the first client
        // disconnects
        con->setDisconnectSignal([&io]
        {
            io.stop();
        });
    });
 
    th.join();
}
```

Now, a client:

```cpp
// A client that connects to the server, calls 1 RPC
// then disconnects, causing everything to shut down
void RunClient() {
    // Start a thread to run our Asio io_service
    asio::io_service io;
    std::thread th = std::thread([&io] {
        asio::io_service::work w(io);
        io.run();
    });
 
    // Connect to the server (localhost, port 9000)
    auto con =
        AsioTransport<void, Calculator>::create(io, "127.0.0.1", 9000).get();
 
    // Call one RPC (the add method), specifying an asynchronous handler for
    // when the result arrives
    CZRPC_CALL(*con, add, 1, 2)
        .async([&io](Result<double> res) {
            printf("Result=%f\n", res.get());  // Prints 3.0
            // Since this is a just a sample, stop the io_service after we get
            // the result,
            // so everything shuts down
            io.stop();
        });
 
    th.join();
}
```

And lets run everything:

```cpp
// For testing simplicity, run both the server and client on the same machine,
void RunServerAndClient() {
    auto a = std::thread([] { RunServer(); });
    auto b = std::thread([] { RunClient(); });
    a.join();
    b.join();
}
```

Note that for a sample it looks big, but most of the code is setup code since the provided transport uses Asio.
The RPC calls itself are as simple as:
```cpp
// RPC call using asynchronous handler to handle the result
CZRPC_CALL(*con, add, 1, 2).async([](Result<double> res) {
    printf("Result=%f\n", res.get());  // Prints 3.0
});
```

