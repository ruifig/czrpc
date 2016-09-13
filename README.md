# czrpc #

czrpc is a C++ RPC framework for use between trusted parties.
It requires no code generation.

It was created as part of a technical article for my blog : [http://www.crazygaze.com/blog/2016/06/06/modern-c-lightweight-binary-rpc-framework-without-code-generation/](http://www.crazygaze.com/blog/2016/06/06/modern-c-lightweight-binary-rpc-framework-without-code-generation/)

[![Patreon](https://cloud.githubusercontent.com/assets/8225057/5990484/70413560-a9ab-11e4-8942-1a63607c0b00.png)](https://www.patreon.com/RuiMVFigueira)


# Features #

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

# How to build #

The framework itself is just headers. Nothing to build.
At the moment it only work with Visual Studio, although there isn't any platform specific code.
To run the unit tests or the provided samples:

1. First download Asio by running the batch file "get_standalone_asio.bat".
    * Alternatively, if you prefer Boost, use the "get_boost.bat" batch file. The provided transport can use either Standalone Asio, or Boost Asio. If you use Boost Asio do the following before including "RPC.h"
        * ```#define CZRPC_HAS_BOOST 1```
2. Open the provided Visual Studio Solution
3. Set the "tests" project as startup, or the ChatServer/ChatClient

# TODO #

* Documentation
* More unit tests and samples
* A BSD sockets transport
* Use some build system to build the unit tests and samples in other platforms.

# License #

czrpc is licensed under the MIT License, see LICENSE for more information

# Credits (Patreon supporters) #

* Glenn Fiedler ([GafferonGames](http://gafferongames.com/) / [THE NETWORK PROTOCOL COMPANY, INC.](http://thenetworkprotocolcompany.com/))
* Vlad Shvets