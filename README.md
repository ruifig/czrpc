# czrpc #

czrpc is a C++ RPC framework for use between trusted parties.
It requires no code generation.

# Features #

* Source available at [https://bitbucket.org/ruifig/czrpc](https://bitbucket.org/ruifig/czrpc)
	* The source code shown in this article is by no means complete. It's meant to show the foundations upon which the framework was built. Also, to shorten things a bit, it's a mix of code from the repository at the time of writing and custom sample code, so it might have errors.
	* Some of the source code which is not directly related to the problem at hand is left intentionally simple with disregard for performance. Any improvements will later be added to source code repository.
* Modern C++ (C++11/14)
	* Requires at least **Visual Studio 2015**. Clang/GCC is fine too, but might not work as-is, since VS is less strict.
* Type-safe
	* The framework detects at **compile time** invalid RPC calls, such as unknown RPC names, wrong number of parameters, or wrong parameter types.
* Relatively small API and not too verbose (considering it requires no code generation) 
* Multiple ways to handle RPC replies
	* Asynchronous handler
	* Futures
	* A client can detect if an RPC caused an exception server side
* Allows the use of potentially any type in RPC parameters
	* Provided the user implements the required functions to deal with that type.
* Bidirectional RPCs (A server can call RPCs on a client)
	* Typically, client code cannot be trusted, but since the framework is to be used between trusted parties, this is not a problem.
* Non intrusive
	* An object being used for RPC calls doesn't need to know anything about RPCs or network.
	* This makes it possible to wrap third party classes for RPC calls.
* Minimal bandwidth overhead per RPC call
* No external dependencies
	* Although the supplied transport (in the source code repository) uses Asio/Boost Asio, the framework itself does not depend on it. You can plug in your own transport.
* No security features provided
	* Because the framework is intended to be used between trusted parties (e.g: between servers).
	* The application can specify its own transport, therefore having a chance to encrypt anything if required.