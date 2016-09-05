+++
date = "2016-08-15T22:33:06+01:00"
next = "/introduction/a-small-taste"
prev = "/introduction/what-is-czrpc"
title = "What is czrpc?"
toc = true
weight = 101

+++

czrpc is a C++ RPC framework that requires no code generation pre-build step.
It is intended to be used between trusted parties such as several servers, for efficient RPC calls. Thus, it only implements the bare minimum.


It was originally created as part of a [technical article for my blog](http://www.crazygaze.com/blog/2016/06/06/modern-c-lightweight-binary-rpc-framework-without-code-generation/).


Features:

* Requires no code generation pre-build step
	* It doesn't make any use of IDL (Interface Definition Language) files. Debatable if it's the best approach, but it is what I needed.
* Modern C++ (C++11/14)
	* Requires at least Visual Studio 2015. Clang/GCC is fine too, but might not work as-is at the moment.
* Type-safe
	* Detects **at compile time** invalid RPC calls, such as unknown RPC names, wrong number of parameters, or wrong parameter types.
* Relatively small API and not too verbose (considering it requires no code generation)
* Multiple ways to handle RPC replies
	* Asynchronous handler
	* Futures
	* A client can detect if an RPC caused an exception server side
* Allows the use of potentially any type in RPC parameters (or return values)
	* Provided the user implements the required type traits
* Bidirectional RPCs (A server can call RPCs on a client)
	* Typically, clients cannot be trusted, but czrpc is meant to be used between trusted parties.
* Non intrusive
	* A class being used for RPC calls doesn’t need to know anything about RPCs or network. This makes it possible to wrap third party classes for RPC calls.
* Minimal bandwidth overhead per RPC call
	* It intentionally only support binary encoding, for performance and bandwidth reasons.
* No external dependencies
	* Although the provided sample transport requires Boost Asio or Standalone Asio, czrpc itself does not depend on it.
* The user can specify its own transport
	* This also makes it possible to add encryption on top, if required, since czrpc doesn't provided any kind of encryption or secure connections.

